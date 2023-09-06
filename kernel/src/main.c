#include "klib.h"
#include "serial.h"
#include "vme.h"
#include "cte.h"
#include "loader.h"
#include "fs.h"
#include "proc.h"
#include "timer.h"
#include "dev.h"

void init_user_and_go();

int main() {
  init_gdt();
  init_serial();
  init_fs();
  init_page(); // uncomment me at Lab1-4
  init_cte(); // uncomment me at Lab1-5
  init_timer(); // uncomment me at Lab1-7
  init_proc(); // uncomment me at Lab2-1
  init_dev(); // uncomment me at Lab3-1
  printf("Hello from OS!\n");
  init_user_and_go();
  panic("should never come back");
}


char *tst_argv[] = {"echo", "hello", "world", NULL};

void init_user_and_go() {
  // Lab1-2: ((void(*)())eip)();
  // Lab1-4: pdgir, stack_switch_call
  // Lab1-6: ctx, irq_iret
  // Lab1-8: argv
  // Lab2-1: proc
  // Lab3-2: add cwd
  /*
proc_t *proc = proc_alloc();
assert(proc);
char *argv[] = {"sh1", NULL};
assert(load_user(proc->pgdir, proc->ctx, "sh1", argv) == 0);
proc_addready(proc);
int cnt = 0;
while (1) 
  {
    proc_yield();
    printf("Proc yield is called! %d time!\n", cnt);
    cnt++;
  }
  */
proc_t *proc = proc_alloc();
// assert(proc->cwd != NULL);
proc->cwd = iopen("/", TYPE_NONE);
assert(proc);
char *argv[] = {"sh", NULL};
assert(load_user(proc->pgdir, proc->ctx, "sh", argv) == 0);
proc_addready(proc);
sti();
while (1) ;
}
