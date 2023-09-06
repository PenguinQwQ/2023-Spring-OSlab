#include "boot.h"
// DO NOT DEFINE ANY NON-LOCAL VARIBLE!
int var;
void load_kernel() {
//  char hello[] = {'\n', 'h', 'e', 'l', 'l', 'o', '\n', 0};
//  putstr(hello);
//  while (1) ;
var = 1;
  // remove both lines above before write codes below
  Elf32_Ehdr *elf = (void *)0x8000;//elf points to the 0x8000
  copy_from_disk(elf, 255 * SECTSIZE, SECTSIZE);//1-255 section are the kernel sections
  Elf32_Phdr *ph, *eph;
  ph = (void*)((uint32_t)elf + elf->e_phoff);//program header
  eph = ph + elf->e_phnum;//
  for (; ph < eph; ph++) {
    if (ph->p_type == PT_LOAD) {
      // TODO: Lab1-2, Load kernel and jump
   //   assert(ph->p_type == PT_LOAD);
      uint32_t offset = ph->p_offset;
      uint32_t vaddr = ph->p_vaddr;
      uint32_t filesz = ph->p_filesz, memsz = ph->p_memsz;
   //   assert(filesz <= memsz);
      memcpy((void *)vaddr, (const void *)(0x8000 + offset), filesz);
      memset((void *)(0x8000 + offset + filesz), 0, memsz - filesz);
    }
  }
  uint32_t entry = elf->e_entry; // change me
  ((void(*)())entry)();
}
