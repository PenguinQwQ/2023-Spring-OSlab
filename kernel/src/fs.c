#include "klib.h"
#include "fs.h"
#include "disk.h"
#include "proc.h"

#ifdef EASY_FS

#define MAX_FILE  (SECTSIZE / sizeof(dinode_t))
#define MAX_DEV   16
#define MAX_INODE (MAX_FILE + MAX_DEV)

// On disk inode
typedef struct dinode {
  uint32_t start_sect;
  uint32_t length;
  char name[MAX_NAME + 1];
} dinode_t;

// On OS inode, dinode with special info
struct inode {
  int valid;
  int type;
  int dev; // dev_id if type==TYPE_DEV
  dinode_t dinode;
};

static inode_t inodes[MAX_INODE];

void init_fs() {
  dinode_t buf[MAX_FILE];
  read_disk(buf, 256);
  for (int i = 0; i < MAX_FILE; ++i) {
    inodes[i].valid = 1;
    inodes[i].type = TYPE_FILE;
    inodes[i].dinode = buf[i];
  }
}

inode_t *iopen(const char *path, int type) {
  for (int i = 0; i < MAX_INODE; ++i) {
    if (!inodes[i].valid) continue;
    if (strcmp(path, inodes[i].dinode.name) == 0) {
      return &inodes[i];
    }
  }
  return NULL;
}

int iread(inode_t *inode, uint32_t off, void *buf, uint32_t len) {
  assert(inode);
  char *cbuf = buf;
  char dbuf[SECTSIZE];
  uint32_t curr = -1;
  uint32_t total_len = inode->dinode.length;
  uint32_t st_sect = inode->dinode.start_sect;
  int i;
  for (i = 0; i < len && off < total_len; ++i, ++off) {
    if (curr != off / SECTSIZE) {
      read_disk(dbuf, st_sect + off / SECTSIZE);
      curr = off / SECTSIZE;
    }
    *cbuf++ = dbuf[off % SECTSIZE];
  }
  return i;
}

void iadddev(const char *name, int id) {
  assert(id < MAX_DEV);
  inode_t *inode = &inodes[MAX_FILE + id];
  inode->valid = 1;
  inode->type = TYPE_DEV;
  inode->dev = id;
  strcpy(inode->dinode.name, name);
}

uint32_t isize(inode_t *inode) {
  return inode->dinode.length;
}

int itype(inode_t *inode) {
  return inode->type;
}

uint32_t ino(inode_t *inode) {
  return inode - inodes;
}

int idevid(inode_t *inode) {
  return inode->type == TYPE_DEV ? inode->dev : -1;
}

int iwrite(inode_t *inode, uint32_t off, const void *buf, uint32_t len) {
  panic("write doesn't support");
}

void itrunc(inode_t *inode) {
  panic("trunc doesn't support");
}

inode_t *idup(inode_t *inode) {
  return inode;
}

void iclose(inode_t *inode) { /* do nothing */ }

int iremove(const char *path) {
  panic("remove doesn't support");
}

#else

#define DISK_SIZE (128 * 1024 * 1024)
#define BLK_NUM   (DISK_SIZE / BLK_SIZE)

#define NDIRECT   12
#define NINDIRECT (BLK_SIZE / sizeof(uint32_t))

#define IPERBLK   (BLK_SIZE / sizeof(dinode_t)) // inode num per blk

// super block
typedef struct super_block {
  uint32_t bitmap; // block num of bitmap
  uint32_t istart; // start block no of inode blocks
  uint32_t inum;   // total inode num
  uint32_t root;   // inode no of root dir
} sb_t;

// On disk inode
typedef struct dinode {
  uint32_t type;   // file type
  uint32_t device; // if it is a dev, its dev_id
  uint32_t size;   // file size
  uint32_t addrs[NDIRECT + 1]; // data block addresses, 12 direct and 1 indirect
} dinode_t;

struct inode {
  int no;
  int ref;
  int del;
  dinode_t dinode;
};

#define SUPER_BLOCK 32
static sb_t sb;

void init_fs() {
  bread(&sb, sizeof(sb), SUPER_BLOCK, 0);
}

#define I2BLKNO(no)  (sb.istart + no / IPERBLK)
#define I2BLKOFF(no) ((no % IPERBLK) * sizeof(dinode_t))

static void diread(dinode_t *di, uint32_t no) {
  bread(di, sizeof(dinode_t), I2BLKNO(no), I2BLKOFF(no));
}

static void diwrite(const dinode_t *di, uint32_t no) {
  bwrite(di, sizeof(dinode_t), I2BLKNO(no), I2BLKOFF(no));
}

static uint32_t dialloc(int type) {//Done!
  dinode_t dinode;
  uint32_t x = 1;
  while(x < sb.inum)
  {
    diread(&dinode, x);
    if(dinode.type == TYPE_NONE)
    {
      dinode.type = type;
      diwrite(&dinode, x);
      return x;
    }
    x++;
  }
  assert(0);
}

static void difree(uint32_t no) {
  dinode_t dinode;
  memset(&dinode, 0, sizeof dinode);
  diwrite(&dinode, no);
}


static uint32_t balloc() {
  uint32_t byte;
  for (int i = 0; i < BLK_NUM / 32; ++i) {
    bread(&byte, 4, sb.bitmap, i * 4);
    if (byte != 0xffffffff) {
    uint32_t x = 0;
    while(x <= 31)
    {
      uint32_t flag = (1 << x) & byte;
      uint32_t pos = 32 * i + x;
      if(flag == 0)
      {
        byte = byte | (1 << x);
        bzero(pos);
        bwrite(&byte, 4, sb.bitmap, i * 4);
        return pos;
      } 
      x++;
    }
    }
  }
  assert(0);
}

static void bfree(uint32_t blkno) {
  assert(blkno >= 64); // cannot free first 64 block
  uint32_t data = 0;
  bread(&data, sizeof(uint32_t), sb.bitmap, blkno >> 5);
  uint32_t m = (1 << (blkno % 32));
  uint32_t mask = ~m;
  data &= mask;
  bwrite(&data, sizeof(uint32_t), sb.bitmap, blkno >> 5);
}

#define INODE_NUM 128
//The Kernel Inode Table!
static inode_t inodes[INODE_NUM];

static inode_t *iget(uint32_t no) {
  uint32_t x = 1;
  while (x < INODE_NUM)
  {
    inode_t *node_ptr = &inodes[x];
    if(node_ptr->no == no)
    {
      node_ptr->ref++;
      return node_ptr;
    }
    x++;
  }
  uint32_t y = 1;
  while (y < INODE_NUM)
  {
    inode_t *node_ptr = &inodes[y];
    if(node_ptr->ref == 0)
    {
      node_ptr->ref++;
      node_ptr->no = no;
      node_ptr->del = 0;
      diread(&(inodes[y].dinode), no);
      return node_ptr;
    }
    y++;
  }
  assert(0);
}

static void iupdate(inode_t *inode) {
  diwrite(&inode->dinode, inode->no);
}

static const char* skipelem(const char *path, char *name) {
  const char *s;
  int len;
  while (*path == '/') path++;
  if (*path == 0) return 0;
  s = path;
  while(*path != '/' && *path != 0) path++;
  len = path - s;
  if (len >= MAX_NAME) {
    memcpy(name, s, MAX_NAME);
    name[MAX_NAME] = 0;
  } else {
    memcpy(name, s, len);
    name[len] = 0;
  }
  while (*path == '/') path++;
  return path;
}

static void idirinit(inode_t *inode, inode_t *parent) {
  // Lab3-2: init the dir inode, i.e. create . and .. dirent
  assert(inode->dinode.type == TYPE_DIR);
  assert(parent->dinode.type == TYPE_DIR); // both should be dir
  assert(inode->dinode.size == 0); // inode shoule be empty
  dirent_t dirent;
  // set .
  dirent.inode = inode->no;
  strcpy(dirent.name, ".");
  iwrite(inode, 0, &dirent, sizeof dirent);
  // set ..
  dirent.inode = parent->no;
  strcpy(dirent.name, "..");
  iwrite(inode, sizeof dirent, &dirent, sizeof dirent);
}


static inode_t *ilookup(inode_t *parent, const char *name, uint32_t *off, int type) {
  assert(parent->dinode.type == TYPE_DIR); // parent must be a dir
  dirent_t dirent;
  uint32_t size = parent->dinode.size, empty = size;
  inode_t *f = NULL;
  uint32_t i = 0;
  while(i < size)
  {
    iread(parent, i, &dirent, sizeof(dirent_t));
    if(dirent.inode == 0)
    {
      if(empty == size) empty = i;
      i = i + sizeof(dirent_t);
      continue;
    }
    if(strcmp(dirent.name, name) == 0)
    {
      f = iget(dirent.inode);
      if(off != NULL) *off = i;
      return f;
    }
    i = i + sizeof(dirent_t);
  }
  if (type == TYPE_NONE) return NULL;
  uint32_t num = dialloc(type);
  f = iget(num);
  dirent.inode = num;
  strcpy(dirent.name, name);
  if(type == TYPE_DIR) idirinit(f, parent);
  iwrite(parent, empty, &dirent, sizeof(dirent));
  if(off != NULL) *off = empty;
  return f;
}

static inode_t *iopen_parent(const char *path, char *name) {
  inode_t *ip, *next;
  if (path[0] == '/') {
    ip = iget(sb.root);
  } else {
    ip = idup(proc_curr()->cwd);
  }
  assert(ip);
  while ((path = skipelem(path, name))) {
    if (ip->dinode.type != TYPE_DIR) {
      iclose(ip);
      return NULL;
    }
    if (*path == 0) {
      return ip;
    }
    next = ilookup(ip, name, NULL, 0);
    if (next == NULL) {
      iclose(ip);
      return NULL;
    }
    iclose(ip);
    ip = next;
  }
  iclose(ip);
  return NULL;
}

inode_t *iopen(const char *path, int type) {
  char name[MAX_NAME + 1];
  memset(name, 0, sizeof(name));
  if (skipelem(path, name) == NULL) {
    return path[0] == '/' ? iget(sb.root) : NULL;
  }
  inode_t *ptr = iopen_parent(path, name);
  if(ptr == NULL) return NULL;  
  inode_t *f = ilookup(ptr, name, NULL, type);
  iclose(ptr);
  return f;
}

static uint32_t iwalk(inode_t *inode, uint32_t no) {
  uint32_t blkno = 0, cur;
  if (no < NDIRECT) {
    cur = inode->dinode.addrs[no];
    if(cur == 0)
    {
      cur = balloc();
      inode->dinode.addrs[no] = cur;
      iupdate(inode);
    }
    return inode->dinode.addrs[no];
  }
  no -= NDIRECT;
  if (no < NINDIRECT) {
    if(inode->dinode.addrs[NDIRECT] == 0)
    {
      inode->dinode.addrs[NDIRECT] = balloc();
      iupdate(inode);
    }
    uint32_t ind_blk = inode->dinode.addrs[NDIRECT];
    bread(&blkno, 4, ind_blk, no << 2);
    if(blkno == 0)
    {
      blkno = balloc();
      bwrite(&blkno, 4, ind_blk, no << 2);
    }
    return blkno;
  }
  assert(0); // file too big, not need to handle this case
}

int iread(inode_t *inode, uint32_t off, void *buf, uint32_t len) {
  uint32_t file_sz = inode->dinode.size;
  if(off + len > file_sz) len = file_sz - off;
  uint32_t ret = len, num, no, offset, rd = 0;
  for(; len != 0;)
  {
    offset = off % BLK_SIZE;
    num = off / BLK_SIZE;
    no = iwalk(inode, num);
    if(len < BLK_SIZE - offset) rd = len;
    else rd = BLK_SIZE - offset;
    bread(buf, rd, no, offset);
    buf = buf + rd;
    off = off + rd;
    len = len - rd;
  }
  return ret;
}

int iwrite(inode_t *inode, uint32_t off, const void *buf, uint32_t len) {
  if(off > inode->dinode.size) return -1;
  uint32_t num, no, offset, wr = 0, sz = len, end = off + len;
  for(;len > 0;)
  {
    offset = off % BLK_SIZE;
    num = off / BLK_SIZE;
    no = iwalk(inode, num);
    if(len < BLK_SIZE - offset) wr = len;
    else wr = BLK_SIZE - offset;
    bwrite(buf, wr, no, offset);
    buf = buf + wr;
    off = off + wr;
    len = len - wr;
  }
  if(end > inode->dinode.size)
  {
    inode->dinode.size = end;
    iupdate(inode);
  }
  return sz;
}
void itrunc(inode_t *inode) {
  inode->dinode.size = 0;
  uint32_t blk_no = 0;
  for (int i = 0 ; i < NDIRECT ; i++)
    {
      blk_no = inode->dinode.addrs[i];
      if(blk_no == 0) continue;
      else bfree(blk_no);
    }
  if(inode->dinode.addrs[NDIRECT] == 0)
  {
    iupdate(inode);
    return;
  }
  uint32_t tmp = inode->dinode.addrs[NDIRECT];
  for (int i = 0 ; i < NINDIRECT; i++)
    {
      bread(&blk_no, 4, tmp, i << 2);
      if(blk_no == 0) continue;
      else bfree(blk_no);
    }
  iupdate(inode);
  return;
}

inode_t *idup(inode_t *inode) {
  assert(inode);
  inode->ref ++;
  return inode;
}

void iclose(inode_t *inode) {
  assert(inode);
  if (inode->ref == 1 && inode->del) {
    itrunc(inode);
    difree(inode->no);
  }
  inode->ref -= 1;
}

uint32_t isize(inode_t *inode) {
  return inode->dinode.size;
}

int itype(inode_t *inode) {
  return inode->dinode.type;
}

uint32_t ino(inode_t *inode) {
  return inode->no;
}

int idevid(inode_t *inode) {
  return itype(inode) == TYPE_DEV ? inode->dinode.device : -1;
}

void iadddev(const char *name, int id) {
  inode_t *ip = iopen(name, TYPE_DEV);
  assert(ip);
  ip->dinode.device = id;
  iupdate(ip);
  iclose(ip);
}

static int idirempty(inode_t *inode) {
  assert(inode->dinode.type == TYPE_DIR);
  uint32_t size = inode->dinode.size;
  dirent_t dirent;
  for (uint32_t i = 0; i < size; i += sizeof(dirent_t)) {
    iread(inode, i, &dirent, sizeof dirent);
    if(dirent.inode == 0) continue;
    if(dirent.inode != 0 && (strcmp(dirent.name, ".") == 0 && i == 0)) continue;
    if(dirent.inode != 0 && (strcmp(dirent.name, "..") == 0 && (i == sizeof(dirent_t)))) continue;
    return false;
  }
  return true;
}

int iremove(const char *path) {
  char name[MAX_NAME + 1];
  uint32_t offset;
  dirent_t dirent;
  inode_t *ptr = iopen_parent(path, name);
  if(ptr == NULL) return -1;
  if(strcmp(name, ".") == 0) 
    {
      iclose(ptr);
      return -1;
    }
  if(strcmp(name, "..") == 0)
    {
      iclose(ptr);
      return -1;
    }
  
  inode_t *f = ilookup(ptr, name, &offset, TYPE_NONE);
  if(f == NULL)
  {
    iclose(ptr);
    return -1;
  }
  if(itype(f) == TYPE_DIR)
  {
    if(idirempty(f) == false) 
      {
        iclose(f);
        iclose(ptr);
        return -1;
      }
  }
  memset(&dirent, 0, sizeof(dirent_t));
  f->del = 1;
  iwrite(ptr, offset, &dirent, sizeof(dirent_t));
  return 0;
}

#endif
