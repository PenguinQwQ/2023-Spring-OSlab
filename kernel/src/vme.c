#include "klib.h"
#include "vme.h"
#include "proc.h"
static TSS32 tss;
void init_gdt() {
  static SegDesc gdt[NR_SEG];
  gdt[SEG_KCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_KERN);
  gdt[SEG_KDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_KERN);
  gdt[SEG_UCODE] = SEG32(STA_X | STA_R,   0,     0xffffffff, DPL_USER);
  gdt[SEG_UDATA] = SEG32(STA_W,           0,     0xffffffff, DPL_USER);
  gdt[SEG_TSS]   = SEG16(STS_T32A,     &tss,  sizeof(tss)-1, DPL_KERN);
  set_gdt(gdt, sizeof(gdt[0]) * NR_SEG);
  set_tr(KSEL(SEG_TSS));
}

void set_tss(uint32_t ss0, uint32_t esp0) {
  tss.ss0 = ss0;
  tss.esp0 = esp0;
}

typedef union free_page{//We need to save the address of the brk pages!
  union free_page *next;
  char buf[PGSIZE];
}page_t;

page_t* free_page_list;

static PD kpd;
static PT kpt[PHY_MEM / PT_SIZE] __attribute__((used));
static void *heap_ptr;

void init_page() {
  extern char end;
  panic_on((size_t)(&end) >= KER_MEM - PGSIZE, "Kernel too big (MLE)");//Make Sure That The Static Kernel Space is not too big.
  static_assert(sizeof(PTE) == 4, "PTE must be 4 bytes");
  static_assert(sizeof(PDE) == 4, "PDE must be 4 bytes");
  static_assert(sizeof(PT) == PGSIZE, "PT must be one page");
  static_assert(sizeof(PD) == PGSIZE, "PD must be one page");
  // Lab1-4: init kpd and kpt, identity mapping of [0 (or 4096), PHY_MEM)
 // TODO();
  static uint32_t K_NR_PDE = PHY_MEM / PT_SIZE;
  for (int i = 0 ; i <= K_NR_PDE - 1 ; i++)
    {
      for (int j = 0 ; j <= NR_PTE - 1 ; j++)
        {
          kpt[i].pte[j].val = MAKE_PTE((i << DIR_SHIFT) | (j << TBL_SHIFT), 7);
        }
      kpd.pde[i].val = MAKE_PDE(&kpt[i], 7);//Here we use &kpt[i]!
    }
  //init the rest pde to zero
  for (int i = K_NR_PDE ; i <= NR_PDE - 1 ; i++)
    kpd.pde[i].val = 0;

  kpt[0].pte[0].val = 0;
  heap_ptr = (void *)KER_MEM;
  set_cr3(&kpd);
  set_cr0(get_cr0() | CR0_PG);
  // Lab1-4: init free memory at [KER_MEM, PHY_MEM), a heap for kernel
  
}

void *kalloc() {
  // Lab1-4: alloc a page from kernel heap, abort when heap empty
 // TODO();
  if((uint32_t)heap_ptr + PGSIZE >= PHY_MEM) assert(0);
  void *re_ptr = heap_ptr;
  memset(re_ptr, 0, PGSIZE);//Set the page to zero!
//  printf("kalloc at :%08x \n", (uint32_t)(re_ptr));
  heap_ptr = (void *)PAGE_DOWN((uint32_t)heap_ptr + PGSIZE);
  assert((uint32_t)re_ptr % PGSIZE == 0);//Check that the re_ptr is aliged!
  return re_ptr;
}

void kfree(void *ptr) {
  // Lab1-4: free a page to kernel heap
  // you can just do nothing :)
  //TODO();
  //Insert the new free page to the free_page_list
}

PD *vm_alloc() {//OK
  // Lab1-4: alloc a new pgdir, map memory under PHY_MEM identityly
//  TODO();
  void *pgdir = kalloc();
  for (int i = 0 ; i <= 31 ; i++)
    {
      PDE* pde = (PDE *)((uint32_t)pgdir + i * sizeof(PDE));
      pde->val = MAKE_PDE(&kpt[i], 3);
    }
  for (int i = 32 ; i <= NR_PDE - 1 ; i++)
    {
      PDE* pde = (PDE *)((uint32_t)pgdir + i * sizeof(PDE));
      pde->val = 0;  
    }
  return (PD *)pgdir;
}

void vm_teardown(PD *pgdir) {
  // Lab1-4: free all pages mapping above PHY_MEM in pgdir, then free itself
  // you can just do nothing :)
  TODO();
}

PD *vm_curr() {
  return (PD*)PAGE_DOWN(get_cr3());
}

PTE *vm_walkpte(PD *pgdir, size_t va, int prot) {
  // Lab1-4: return the pointer of PTE which match va
  // if not exist (PDE of va is empty) and prot&1, alloc PT and fill the PDE
  // if not exist (PDE of va is empty) and !(prot&1), return NULL
  // remember to let pde's prot |= prot, but not pte
  int pd_index = ADDR2DIR(va);
  PDE *pde = &(pgdir->pde[pd_index]);
  if((pde->present == 0) && (prot & 1)) 
  {
      void *pg_table = kalloc();//kalloc a new page as page table
      pde->val = MAKE_PDE((uint32_t)pg_table, prot);
  }
  if((pde->present == 0) && (prot == 0))
      return NULL;
  if(prot) pde->val |= prot;
  PT *pt = PDE2PT(*pde);
  int pt_index = ADDR2TBL(va);
  PTE *pte = &(pt->pte[pt_index]);
  assert((prot & ~7) == 0);
  return pte;
}

void *vm_walk(PD *pgdir, size_t va, int prot) {//OK
  // Lab1-4: translate va to pa
  // if prot&1 and prot voilation ((pte->val & prot & 7) != prot), call vm_pgfault
  // if va is not mapped and !(prot&1), return NULL
  int pd_index = ADDR2DIR(va); // 计算“页目录号”
  PDE *pde = &(pgdir->pde[pd_index]); // 找到对应的页目录项
  if(pde->present == 0 && (!(prot & 1))) return NULL;
  PT *pt = PDE2PT(*pde); // 根据PDE找页表的地址
  int pt_index = ADDR2TBL(va); // 计算“页表号”
  PTE *pte = &(pt->pte[pt_index]); // 找到对应的页表项
  if(pte->present == 0 && (!(prot & 1))) return NULL;
  if((prot & 1) && ((pte->val & prot & 7) != prot)) vm_pgfault(va, 10);
  void *page = PTE2PG(*pte); // 根据PTE找物理页的地址
  void *pa = (void*)((uint32_t)page | ADDR2OFF(va)); // 补上页内偏移量
  return pa;
}

void vm_map(PD *pgdir, size_t va, size_t len, int prot) {//OK
  // Lab1-4: map [PAGE_DOWN(va), PAGE_UP(va+len)) at pgdir, with prot
  // if have already mapped pages, just let pte->prot |= prot
  for (uint32_t pg_st = PAGE_DOWN(va) ; pg_st < PAGE_UP(va + len) ; pg_st += PGSIZE)
  {
    void *new_phy_page = kalloc();//kalloc a new physical page, for the page table entry to map!
    PTE *pte = vm_walkpte(pgdir, pg_st, prot);//get the page table entry!
    assert(pte != NULL);
    if(pte != NULL) //Already exite pte on pgdir!
      {
        pte->val |= prot;
        pte->val = MAKE_PTE(new_phy_page, prot);
      }
    //The pte may be NULL or invalid!
   // assert(pte != NULL && pte->present != 0);//check the pte is exist and is not invalid;

  }
  assert(prot & PTE_P);
  assert((prot & ~7) == 0);
  size_t start = PAGE_DOWN(va);
  size_t end = PAGE_UP(va + len);
  assert(start >= PHY_MEM);
  assert(end >= start);
//  TODO();
}

void vm_unmap(PD *pgdir, size_t va, size_t len) {
  // Lab1-4: unmap and free [va, va+len) at pgdir
  // you can just do nothing :)
  //assert(ADDR2OFF(va) == 0);
  //assert(ADDR2OFF(len) == 0);
  //TODO();
}

void vm_copycurr(PD *pgdir) {
  // Lab2-2: copy memory mapped in curr pd to pgdir
  //TODO();
  PD *curr_pgdir = vm_curr();
  for (size_t pgaddr = PAGE_DOWN(PHY_MEM) ; pgaddr < PAGE_DOWN(USR_MEM) ; pgaddr += PGSIZE)
  {
    PTE *pte = vm_walkpte(curr_pgdir, pgaddr, 7);
    if((pte != NULL) && (pte->present != 0))
      {
        size_t prot = (pte->user_supervisor) << 2 | (pte->read_write << 1) | (pte->present);
        assert(prot <= 7);//Make Sure The Prot is Valid!
        vm_map(pgdir,pgaddr, PGSIZE, prot);
        void *phy_pg = vm_walk(pgdir, pgaddr, prot);
        memcpy(phy_pg, (void *)pgaddr, PGSIZE);
      }
  }
}

void vm_pgfault(size_t va, int errcode) {
  printf("pagefault @ 0x%p, errcode = %d\n", va, errcode);
  panic("pgfault");
}
