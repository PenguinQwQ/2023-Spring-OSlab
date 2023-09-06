#include "klib.h"
#include "cte.h"
#include "sysnum.h"
#include "vme.h"
#include "serial.h"
#include "loader.h"
#include "proc.h"
#include "timer.h"
#include "file.h"
#include "fs.h"

typedef int (*syshandle_t)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);

extern void *syscall_handle[NR_SYS];

void do_syscall(Context *ctx) {
  // TODO: Lab1-5 call specific syscall handle and set ctx register
  int sysnum = ctx->eax;
  uint32_t arg1 = ctx->ebx;
  uint32_t arg2 = ctx->ecx;
  uint32_t arg3 = ctx->edx;
  uint32_t arg4 = ctx->esi;
  uint32_t arg5 = ctx->edi;
  int res;
  if (sysnum < 0 || sysnum >= NR_SYS) {
    res = -1;
  } else {
    res = ((syshandle_t)(syscall_handle[sysnum]))(arg1, arg2, arg3, arg4, arg5);
  }
  ctx->eax = res;
}

int sys_write(int fd, const void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
  //return serial_write(buf, count);
  assert((fd >= 0) && (fd <= 31));
  assert(buf);
  file_t *file = proc_getfile(proc_curr(), fd);
  if(file == NULL) printf("sys write file %d failed!\n", fd);
  if(file == NULL) return -1;
  int len = fwrite(file, buf, count);
  return len;
}

int sys_read(int fd, void *buf, size_t count) {
  // TODO: rewrite me at Lab3-1
  // return serial_read(buf, count);
  assert((fd >= 0) && (fd <= 31));
  assert(buf);
  file_t *file = proc_getfile(proc_curr(), fd);
  if(file == NULL) printf("sys read file %d failed!\n", fd);
  if(file == NULL) return -1;
  int len = fread(file, buf, count);
  assert(len != -1);
  assert(len <= count);
  return len;
}

int sys_brk(void *addr) {
  // TODO: Lab1-5
 //size_t brk = proc_curr()->brk; // use brk of proc instead of this in Lab2-1
  size_t new_brk = PAGE_UP(addr);
  if (proc_curr()->brk == 0) {
    proc_curr()->brk = new_brk;
  } else if (new_brk > proc_curr()->brk) {//new_brk > brk, so we need to gap the mem-map
  //  TODO();
  PD* pgdir = vm_curr();
  vm_map(pgdir, proc_curr()->brk, new_brk - proc_curr()->brk, 7);
  proc_curr()->brk = new_brk;
  } else if (new_brk < proc_curr()->brk) {
    // can just do nothing
  }
  return 0;
}

void sys_sleep(int ticks) {
 // TODO(); // Lab1-7
  uint32_t begin_time = get_tick();
  while(1)
  {
    uint32_t now_time = get_tick();
    if(now_time - begin_time >= ticks)
      break;
   // sti(); hlt(); cli();
   proc_yield();
  }
}

int sys_exec(const char *path, char *const argv[]) {
 // TODO(); // Lab1-8, Lab2-1
  PD* pgdir = vm_alloc();
  Context ctx;
  int ret = load_user(pgdir, &ctx, path, argv);
  if(ret != 0) return -1;
  assert(ret == 0);
  proc_curr()->pgdir = pgdir;
  set_cr3(pgdir);
  //Old proc will not be executed, so we don't need to set_tss
  irq_iret(&ctx);  
}

int sys_getpid() {
 // TODO(); // Lab2-1
 return proc_curr()->pid;
}

void sys_yield() {
  proc_yield();
}

int sys_fork() {
  //TODO(); // Lab2-2
  proc_t *new_proc = proc_alloc();
  if(new_proc == NULL) return -1;
  assert(new_proc);
 // assert(new_proc->status == UNINIT);
  proc_copycurr(new_proc);
  proc_addready(new_proc);
  return new_proc->pid;
}

void sys_exit(int status) {
  //TODO(); // Lab2-3
  //while(1) proc_yield();
  proc_makezombie(proc_curr(), status);
  INT(0x81);
  assert(0);//Defend the zombie proc run back!
}

int sys_wait(int *status) {
  //TODO(); // Lab2-3, Lab2-4
 // sys_sleep(250);
  int ret = 0;
  if(proc_curr()->child_num == 0) return -1;//Don't Have Child Proc!
  proc_t *son = NULL;
  //First just easily P(sem)
  sem_p(&(proc_curr()->zombie_sem));
  //Find directly!
  son = proc_findzombie(proc_curr());
  assert(son != NULL);
  if(status != NULL)
    *status = son->exit_code;
  ret = son->pid;
  proc_free(son);
  proc_curr()->child_num--;
  return ret;
}

int sys_sem_open(int value) {
  //TODO(); // Lab2-5
  int id = proc_allocusem(proc_curr());
  if(id == -1) return -1;
  usem_t *ptr = usem_alloc(value);
  if(ptr == NULL) return -1;
  proc_curr()->usems[id] = ptr;
  return id;
}

int sys_sem_p(int sem_id) {
  // TODO(); // Lab2-5
  usem_t *ptr = proc_getusem(proc_curr(), sem_id);
  if(ptr == NULL) return -1;
  sem_p(&(ptr->sem));
  return 0;
}

int sys_sem_v(int sem_id) {
  // TODO(); // Lab2-5
  usem_t *ptr = proc_getusem(proc_curr(), sem_id);
  if(ptr == NULL) return -1;
  sem_v(&(ptr->sem));
  return 0;
}

int sys_sem_close(int sem_id) {
  // TODO(); // Lab2-5
  usem_t *ptr = proc_getusem(proc_curr(), sem_id);
  if(ptr == NULL) return -1;
  proc_curr()->usems[sem_id] = NULL;
  return 0;
}

int sys_open(const char *path, int mode) {
  int fd = proc_allocfile(proc_curr());
  if(fd == -1) return -1;//Current Proc has none available free file slot
  file_t *file = fopen(path, mode);
  if(file == NULL) return -1;//Open failure
  proc_curr()->files[fd] = file;
  return fd;
}

int sys_close(int fd) {
  //TODO(); // Lab3-1
  assert((fd >= 0) && (fd <= 31));
  file_t *file = proc_getfile(proc_curr(), fd);
  if(file == NULL) return -1;//Close Failure
  fclose(file);
  proc_curr()->files[fd] = NULL;
  return 0;//Close Success
}

int sys_dup(int fd) {
  //TODO(); // Lab3-1
  int id = proc_allocfile(proc_curr());
  if(id == -1) return -1;
  file_t *file = proc_getfile(proc_curr(), fd);
  if(file == NULL) return -1;
  proc_curr()->files[id] = fdup(file);
  return id;
}

uint32_t sys_lseek(int fd, uint32_t off, int whence) {
  //TODO(); // Lab3-1
  file_t *file = proc_getfile(proc_curr(), fd);
  if(file == NULL) return -1;
  uint32_t offset = fseek(file, off, whence);
  return offset;
}

int sys_fstat(int fd, struct stat *st) {
 // TODO(); // Lab3-1
 file_t *file = proc_getfile(proc_curr(), fd);
 if(file == NULL) return -1;
 assert(file != NULL);
 if(file->type == TYPE_FILE || file->type == TYPE_DIR)
 { 
  st->node = ino(file->inode);
  st->size = isize(file->inode);
  st->type = file->type;
 }
 else if(file->type == TYPE_DEV)
 {
  st->node = 0;
  st->size = 0;
  st->type = TYPE_DEV;
 }
 else assert(0);
 return 0;
}

int sys_chdir(const char *path) {//To be modified!
  //TODO(); // Lab3-2
  //TODO();//First finish the make!
  void *dir = (void *)iopen(path, TYPE_NONE);
  if(dir == NULL) return -1;
  if(itype(dir) != TYPE_DIR)
  {
    iclose(dir);
    return -1;
  }
  iclose(proc_curr()->cwd);
  proc_curr()->cwd = dir;
  return 0;
}

int sys_unlink(const char *path) {
  return iremove(path);
}

// optional syscall

void *sys_mmap() {
  TODO();
}

void sys_munmap(void *addr) {
  TODO();
}

int sys_clone(void (*entry)(void*), void *stack, void *arg) {
  TODO();
}

int sys_kill(int pid) {
  TODO();
}

int sys_cv_open() {
  TODO();
}

int sys_cv_wait(int cv_id, int sem_id) {
  TODO();
}

int sys_cv_sig(int cv_id) {
  TODO();
}

int sys_cv_sigall(int cv_id) {
  TODO();
}

int sys_cv_close(int cv_id) {
  TODO();
}

int sys_pipe(int fd[2]) {
  TODO();
}

int sys_link(const char *oldpath, const char *newpath) {
  TODO();
}

int sys_symlink(const char *oldpath, const char *newpath) {
  TODO();
}

void *syscall_handle[NR_SYS] = {
  [SYS_write] = sys_write,
  [SYS_read] = sys_read,
  [SYS_brk] = sys_brk,
  [SYS_sleep] = sys_sleep,
  [SYS_exec] = sys_exec,
  [SYS_getpid] = sys_getpid,
  [SYS_yield] = sys_yield,
  [SYS_fork] = sys_fork,
  [SYS_exit] = sys_exit,
  [SYS_wait] = sys_wait,
  [SYS_sem_open] = sys_sem_open,
  [SYS_sem_p] = sys_sem_p,
  [SYS_sem_v] = sys_sem_v,
  [SYS_sem_close] = sys_sem_close,
  [SYS_open] = sys_open,
  [SYS_close] = sys_close,
  [SYS_dup] = sys_dup,
  [SYS_lseek] = sys_lseek,
  [SYS_fstat] = sys_fstat,
  [SYS_chdir] = sys_chdir,
  [SYS_unlink] = sys_unlink,
  [SYS_mmap] = sys_mmap,
  [SYS_munmap] = sys_munmap,
  [SYS_clone] = sys_clone,
  [SYS_kill] = sys_kill,
  [SYS_cv_open] = sys_cv_open,
  [SYS_cv_wait] = sys_cv_wait,
  [SYS_cv_sig] = sys_cv_sig,
  [SYS_cv_sigall] = sys_cv_sigall,
  [SYS_cv_close] = sys_cv_close,
  [SYS_pipe] = sys_pipe,
  [SYS_link] = sys_link,
  [SYS_symlink] = sys_symlink};
