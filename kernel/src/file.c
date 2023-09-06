#include "klib.h"
#include "file.h"

#define TOTAL_FILE 128

file_t files[TOTAL_FILE];

static file_t *falloc() {//DONE
  // Lab3-1: find a file whose ref==0, init it, inc ref and return it, return NULL if none
  //TODO();

  for (int i = 0 ; i <= TOTAL_FILE - 1 ; i++)
    {
      if(files[i].ref == 0)
      {
        files[i].ref++;
        files[i].type = TYPE_NONE;
        return &(files[i]);
      }
    }
  return NULL;
}

file_t *fopen(const char *path, int mode) {
  file_t *fp = falloc();
  inode_t *ip = NULL;
  if (!fp) goto bad;
  // TODO: Lab3-2, determine type according to mode
  // iopen in Lab3-2: if file exist, open and return it
  //       if file not exist and type==TYPE_NONE, return NULL
  //       if file not exist and type!=TYPE_NONE, create the file as type
  // you can ignore this in Lab3-1
  int open_type = 0;
  if((mode & O_CREATE) == 0) open_type = TYPE_NONE;
  if((mode & O_CREATE) && (mode & O_DIR)) open_type = TYPE_DIR;
  if((mode & O_CREATE) && ((mode & O_DIR) == 0)) open_type = TYPE_FILE;

  ip = iopen(path, open_type);
  //printf("Read file at path %s\n", path);
  if (!ip) goto bad;
  int type = itype(ip);
  if (type == TYPE_FILE || type == TYPE_DIR) {
    // TODO: Lab3-2, if type is not DIR, go bad if mode&O_DIR
    if((type == TYPE_FILE) && (mode & O_DIR)) goto bad;
    // TODO: Lab3-2, if type is DIR, go bad if mode WRITE or TRUNC
    if((type == TYPE_DIR) && ((mode & O_WRONLY) || (mode & O_WRONLY) || (mode & O_TRUNC))) goto bad;
    // TODO: Lab3-2, if mode&O_TRUNC, trunc the file
    if((type == TYPE_FILE) && (mode & O_TRUNC)) itrunc(ip);

    fp->type = type; // file_t don't and needn't distingush between file and dir
    fp->inode = ip;
    fp->offset = 0;
  } else if (type == TYPE_DEV) {
    fp->type = TYPE_DEV;
    fp->dev_op = dev_get(idevid(ip));
    iclose(ip);
    ip = NULL;
  } else assert(0);
  fp->readable = !(mode & O_WRONLY);
  fp->writable = (mode & O_WRONLY) || (mode & O_RDWR);
  return fp;
bad:
  if (fp) fclose(fp);
  if (ip) iclose(ip);
  return NULL;
}

int fread(file_t *file, void *buf, uint32_t size) {//DONE
  // Lab3-1, distribute read operation by file's type
  // remember to add offset if type is FILE (check if iread return value >= 0!)
  assert(file);
  assert(buf);
  if (!file->readable) return -1;//The file is not readable!
  int len = -1;
  if(file->type == TYPE_FILE || file->type == TYPE_DIR)
  {/*
    int readable_size = isize(file->inode) - file->offset;
    size = MIN(size, readable_size);
    if(size == 0) return 0;
    */
    len = iread(file->inode, file->offset, buf, size);
    assert(len <= size);
    if(len >= 0) file->offset += (uint32_t)len;
  }
  if(file->type == TYPE_DEV)
  {
    len = file->dev_op->read(buf, size);
    assert(len <= size);
  }
  return len;
}

int fwrite(file_t *file, const void *buf, uint32_t size) {//DONE
  // Lab3-1, distribute write operation by file's type
  // remember to add offset if type is FILE (check if iwrite return value >= 0!)
  if (!file->writable) return -1;
  //TODO();
  int len = -1;
  if(file->type == TYPE_FILE || file->type == TYPE_DIR)
  {/*
    int writeable_size = isize(file->inode) - file->offset;
    size = MIN(size, writeable_size);
    if(size == 0) return 0;
    */
    len = iwrite(file->inode, file->offset, buf, size);
    assert(len <= size);
    if(len == -1) return -1;
    if(len >= 0) file->offset += (uint32_t)len;
  }
  if(file->type == TYPE_DEV)
  {
    len = file->dev_op->write(buf, size);
    assert(len <= size);
  }
  return len;
}

uint32_t fseek(file_t *file, uint32_t off, int whence) {
  // Lab3-1, change file's offset, do not let it cross file's size
  if (file->type == TYPE_FILE || file->type == TYPE_DIR) {
    //TODO();
    switch (whence)
    {
    case SEEK_CUR: file->offset += off; break;
    case SEEK_END: file->offset = isize(file->inode) + off; break;
    case SEEK_SET: file->offset = off; break;
    default:       break;
    }
    assert(file->offset >= 0);
    assert(file->offset <= isize(file->inode));
    return file->offset;
  }
  return -1;
}

file_t *fdup(file_t *file) {
  // Lab3-1, inc file's ref, then return itself
  // TODO();
  file->ref = file->ref + 1;
  return file;
}

void fclose(file_t *file) {
  // Lab3-1, dec file's ref, if ref==0 and it's a file, call iclose
  // TODO();
  assert(file);
  file->ref = file->ref - 1;
  if(file->ref == 0 && (file->type == TYPE_FILE || file->type == TYPE_DIR))
    {
      iclose(file->inode);
    }
}
