#include "klib.h"
#include "vme.h"
#include "cte.h"
#include "loader.h"
#include "disk.h"
#include "fs.h"
#include <elf.h>

uint32_t load_elf(PD *pgdir, const char *name) {
  //Before Load, we should store current pgdir by vm_curr
  Elf32_Ehdr elf;
  Elf32_Phdr ph;
  inode_t *inode = iopen(name, TYPE_NONE);
  if (!inode) return -1;
  iread(inode, 0, &elf, sizeof(elf));
  if (*(uint32_t*)(&elf) != 0x464c457f) { // check ELF magic number
    iclose(inode);
    return -1;
  }
    PD* curr_pgdir = vm_curr();
    set_cr3(pgdir);//set the cr3
  //load different sections!
  for (int i = 0; i < elf.e_phnum; ++i) {
    iread(inode, elf.e_phoff + i * sizeof(ph), &ph, sizeof(ph));
    if (ph.p_type == PT_LOAD) {
      // Lab1-2: Load segment to physical memory
    uint32_t vaddr = ph.p_vaddr; 
    uint32_t offset = ph.p_offset;
    uint32_t filesz = ph.p_filesz, memsz = ph.p_memsz;
    uint32_t prot = 0;
    if((ph.p_flags & PF_W) != 0 ) prot = 7;//Writeable
    else prot = 5; //Read-Only
    vm_map(pgdir, vaddr, memsz, prot);
    void *paddr = vm_walk(pgdir, vaddr, prot);
    assert(paddr != NULL);//Check the paddr is not NULL
    iread(inode, offset, (void *)paddr, filesz);
    memset((void *)((uint32_t)paddr + filesz), 0, memsz - filesz);
      // Lab1-4: Load segment to virtual memory
    //  TODO();
    }
  }
  // TODO: Lab1-4 alloc stack memory in pgdir
  vm_map(pgdir, USR_MEM - PGSIZE, PGSIZE, 7);//kalloc the user stack!
  iclose(inode);
  set_cr3(curr_pgdir);//Return the current pgdir
  return elf.e_entry;
}

#define MAX_ARGS_NUM 31

uint32_t load_arg(PD *pgdir, char *const argv[]) {
  // Lab1-8: Load argv to user stack
  char *stack_top = (char*)vm_walk(pgdir, USR_MEM - PGSIZE, 7) + PGSIZE;
 // printf("Loading arg... Now stack_top is %0x8x \n", (uint32_t)stack_top);
  size_t argv_va[MAX_ARGS_NUM + 1];
  int argc;

  if(argv != NULL)
  {
 // printf("(In load_arg)The result of strcmp(echo, argv[0]) is %d\n", strcmp(argv[0], "echo"));
  for (argc = 0; argv[argc]; ++argc) {
    assert(argc < MAX_ARGS_NUM);
    // push the string of argv[argc] to stack, record its va to argv_va[argc]
  //  TODO();
    size_t arg_len = strlen(argv[argc]);
    stack_top -= (arg_len + 1);
    stack_top -= ADDR2OFF(stack_top) % 4; // align to 4 bytes
    strcpy(stack_top, argv[argc]);
   // *(char *)((uint32_t)stack_top + (arg_len + 1)) = '\0';
 //   printf("(In load_arg)The loaded %dth string is %s\n", argc, argv[argc]);
    argv_va[argc] = (USR_MEM - PGSIZE) + ADDR2OFF(stack_top);
  }
  }
  else argc = 0;
 // printf("The argc number is %d \n", argc);
 
  argv_va[argc] = 0; // set last argv NULL
  stack_top -= ADDR2OFF(stack_top) % 4; // align to 4 bytes
  for (int i = argc; i >= 0; --i) {
    // push the address of argv_va[argc] to stack to make argv array
    stack_top -= sizeof(size_t);
    *(size_t *)stack_top = argv_va[i];
  }
  // push the address of the argv array as argument for _start
 // TODO();
  stack_top -= sizeof(size_t);
  *(size_t *)(stack_top) = (USR_MEM - PGSIZE) + ADDR2OFF(stack_top + sizeof(size_t));
  // push argc as argument for _start
  stack_top -= sizeof(size_t);
  *(size_t *)stack_top = argc;
  stack_top -= sizeof(size_t); // a hole for return value (useless but necessary)
  return USR_MEM - PGSIZE + ADDR2OFF(stack_top);
}

int load_user(PD *pgdir, Context *ctx, const char *name, char *const argv[]) {
  size_t eip = load_elf(pgdir, name);
  if (eip == -1) return -1;
  ctx->cs = USEL(SEG_UCODE);
  ctx->ds = USEL(SEG_UDATA);
  ctx->eip = eip;
  // TODO: Lab1-6 init ctx->ss and esp
  ctx->ss = USEL(SEG_UDATA);
  ctx->esp = load_arg(pgdir, argv);
  ctx->eflags = 0x202; // TODO: Lab1-7 change me to 0x202
  return 0;
}
