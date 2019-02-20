#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data[];  // defined in data.S

// kernel page directory
static pde_t *kpgdir;  // for use in scheduler()

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
void
kvmalloc(void)
{
  kpgdir = setupkvm();
}

// Set up CPU's kernel segment descriptors.
// Run once at boot time on each CPU.
void
seginit(void)
{
  struct cpu *c;

  // Map virtual addresses to linear addresses using identity map.
  // Cannot share a CODE descriptor for both kernel and user
  // because it would have to have DPL_USR, but the CPU forbids
  // an interrupt from CPL=0 to DPL=3.
  c = &cpus[cpunum()];
  c->gdt[SEG_KCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, 0);
  c->gdt[SEG_KDATA] = SEG(STA_W, 0, 0xffffffff, 0);
  c->gdt[SEG_UCODE] = SEG(STA_X|STA_R, 0, 0xffffffff, DPL_USER);
  c->gdt[SEG_UDATA] = SEG(STA_W, 0, 0xffffffff, DPL_USER);

  // Map cpu, and curproc
  c->gdt[SEG_KCPU] = SEG(STA_W, &c->cpu, 8, 0);

  lgdt(c->gdt, sizeof(c->gdt));
  loadgs(SEG_KCPU << 3);
  
  // Initialize cpu-local storage.
  cpu = c;
  proc = 0;
}

/* COMMENTED */

// Return the address of the PTE in page table pgdir
// that corresponds to linear address va.  If create!=0,
// create any required page table pages.
static pte_t *
walkpgdir(pde_t *pgdir, const void *va, int create)
{
	// note that these 2 are PHYSICAL addresses
  pde_t *pde; // page directory entry
  pte_t *pgtab; // page table

  pde = &pgdir[PDX(va)]; // PDX finds the page directory index of virtual address
  if(*pde & PTE_P) { // basically, if the entry is valid (if there is something present)
    pgtab = (pte_t*)PTE_ADDR(*pde); // get page table we are looking for
  } 
	else { // invalid; no pages mapped to this virtual address

    if(!create || (pgtab = (pte_t*)kalloc()) == 0) // if we should not create new page table pages or if kalloc fails, we're fucked
      return 0;
		
		// if create == 1, pgtab points to a freshly alloc'd page 

    // Make sure all those PTE_P bits are zero.
    memset(pgtab, 0, PGSIZE);

    // The permissions here are overly generous, but they can
    // be further restricted by the permissions in the page table 
    // entries, if necessary.

		// set page directory entry of virtual address to map to
		// freshly alloc'd page (pgtab)
    *pde = PADDR(pgtab) | PTE_P | PTE_W | PTE_U;
  }
	// basically, return the entry in the page table corresponding to virtual
	// address (does not return PFN; just address of the ENTRY)
  return &pgtab[PTX(va)]; // PTX finds the page table index of virtual address
}

/* COMMENTED */

// Create PTEs for linear (virtual) addresses starting at la that refer to
// physical addresses starting at pa. la and size might not
// be page-aligned.
static int
mappages(pde_t *pgdir, void *la, uint size, uint pa, int perm)
{
  char *a, *last;
  pte_t *pte;
  
	// this is your "curr"
  a = PGROUNDDOWN(la); // find first page address of pages to be mapped

	// this is your "end"
  last = PGROUNDDOWN(la + size - 1); // find last page address to be mapped 

  for(;;) { // map each part of physical memory that you want

    pte = walkpgdir(pgdir, a, 1); // search for page table entry corresponding to virtual address a; create one if none existed previously

    if(pte == 0) // disaster happened
      return -1;

    if(*pte & PTE_P) // idk... mabye if page table entry is not present?
      panic("remap");

		// page table entry is assigned physical address and "present" in memory
    *pte = pa | perm | PTE_P; // perm = permissions 

    if(a == last) // if "curr" == "end"
      break;

    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// The mappings from logical to linear are one to one (i.e.,
// segmentation doesn't do anything).
// There is one page table per process, plus one that's used
// when a CPU is not running any process (kpgdir).
// A user process uses the same page table as the kernel; the
// page protection bits prevent it from using anything other
// than its memory.
// 
// setupkvm() and exec() set up every page table like this:
//   0..640K          : user memory (text, data, stack, heap)
//   640K..1M         : mapped direct (for IO space)
//   1M..end          : mapped direct (for the kernel's text and data)
//   end..PHYSTOP     : mapped direct (kernel heap and user pages)
//   0xfe000000..0    : mapped direct (devices such as ioapic)
//
// The kernel allocates memory for its heap and for user memory
// between kernend and the end of physical memory (PHYSTOP).
// The virtual address space of each user program includes the kernel
// (which is inaccessible in user mode).  The user program addresses
// range from 0 till 640KB (USERTOP), which where the I/O hole starts
// (both in physical memory and in the kernel's virtual address
// space).
static struct kmap {
  void *p; // start?
  void *e; // end?
  int perm; // permissions
} kmap[] = {
  {(void*)USERTOP,    (void*)0x100000, PTE_W},  // I/O space
  {(void*)0x100000,   data,            0    },  // kernel text, rodata
  {data,              (void*)PHYSTOP,  PTE_W},  // kernel data, memory
  {(void*)0xFE000000, 0,               PTE_W},  // device mappings
};

/* COMMENTED */

// Set up kernel part of a page table.
pde_t*
setupkvm(void)
{
  pde_t *pgdir; // kernel page directory
  struct kmap *k; // mapping structure

  if((pgdir = (pde_t*)kalloc()) == 0) // alloc page for page directory
    return 0;
  memset(pgdir, 0, PGSIZE); // zero page directory out
  k = kmap;
  for(k = kmap; k < &kmap[NELEM(kmap)]; k++) { // for each segment in address space (IO, kernel data, kernel text, device)
		
		// linear to physical mapping is "one to one"
    if(mappages(pgdir, k->p, k->e - k->p, (uint)k->p, k->perm) < 0) 
      return 0;
	}

  return pgdir; // return kernel page directory
}

/* COMMENTED */
// Turn on paging.
void
vmenable(void)
{
  uint cr0;

  switchkvm(); // load kpgdir into cr3
  cr0 = rcr0();
  cr0 |= CR0_PG;
  lcr0(cr0);
}


/* COMMENTED */
// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void
switchkvm(void)
{
	// cr3 is the PTBR
	// make PTBR point to kernel page directory
  lcr3(PADDR(kpgdir));   // switch to the kernel page table
}


/* COMMENTED */
// change processor context and hardware page table -> user proc p
// Switch TSS and h/w page table to correspond to process p.
void
switchuvm(struct proc *p)
{
  pushcli();
  cpu->gdt[SEG_TSS] = SEG16(STS_T32A, &cpu->ts, sizeof(cpu->ts)-1, 0);
  cpu->gdt[SEG_TSS].s = 0;
  cpu->ts.ss0 = SEG_KDATA << 3;
  cpu->ts.esp0 = (uint)proc->kstack + KSTACKSIZE;
  ltr(SEG_TSS << 3);
  if(p->pgdir == 0) // disaster
    panic("switchuvm: no pgdir");
  lcr3(PADDR(p->pgdir));  // switch to new address space (proc p's page directory)
  popcli();
}

/* COMMENTED */
// initialize code segment into page directory slot 0 (nothing is actually
// there; it is just initialized)
// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
void
inituvm(pde_t *pgdir, char *init, uint sz)
{
  char *mem;
  
  if(sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc(); // alloc a page
  memset(mem, 0, PGSIZE); // zero page out 
  mappages(pgdir, 0, PGSIZE, PADDR(mem), PTE_W|PTE_U); // map page table entries on this page as user accessible and writable
  memmove(mem, init, sz);
}

/* COMMENTED */
// actually put program code into page directory slots
// Load a program segment into pgdir.  addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int
loaduvm(pde_t *pgdir, char *addr, struct inode *ip, uint offset, uint sz)
{
  uint i, pa, n;
  pte_t *pte;

  if((uint)addr % PGSIZE != 0) // check if page aligned
    panic("loaduvm: addr must be page aligned");

  for(i = 0; i < sz; i += PGSIZE) { // for each page increment
    if((pte = walkpgdir(pgdir, addr+i, 0)) == 0) // check if address is mapped
      panic("loaduvm: address should exist");
    pa = PTE_ADDR(*pte);
    if(sz - i < PGSIZE)
      n = sz - i;
    else
      n = PGSIZE;
    if(readi(ip, (char*)pa, offset+i, n) != n)
      return -1;
  }
  return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
int
allocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  char *mem;
  uint a;

  if(newsz > USERTOP) // exceeds user memory
    return 0;
  if(newsz < oldsz) // not big enough
    return oldsz;

  a = PGROUNDUP(oldsz); // next page
  for(; a < newsz; a += PGSIZE) { // for each page table you want to create
    mem = kalloc(); // create page
    if(mem == 0){
      cprintf("allocuvm out of memory\n");
      deallocuvm(pgdir, newsz, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE); // zero out page
    mappages(pgdir, (char*)a, PGSIZE, PADDR(mem), PTE_W|PTE_U); // map to physical memory
  }
  return newsz;
}

/* COMMENTED */
// INTUITIVE...

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
int
deallocuvm(pde_t *pgdir, uint oldsz, uint newsz)
{
  pte_t *pte;
  uint a, pa;

  if(newsz >= oldsz)
    return oldsz;

  a = PGROUNDUP(newsz);
  for(; a  < oldsz; a += PGSIZE){
    pte = walkpgdir(pgdir, (char*)a, 0);
    if(pte && (*pte & PTE_P) != 0){
      pa = PTE_ADDR(*pte);
      if(pa == 0)
        panic("kfree");
      kfree((char*)pa);
      *pte = 0;
    }
  }
  return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void
freevm(pde_t *pgdir)
{
  uint i;

  if(pgdir == 0)
    panic("freevm: no pgdir");
  deallocuvm(pgdir, USERTOP, 0);
  for(i = 0; i < NPDENTRIES; i++){
    if(pgdir[i] & PTE_P)
      kfree((char*)PTE_ADDR(pgdir[i]));
  }
  kfree((char*)pgdir);
}

// Given a parent process's page table, create a copy
// of it for a child.
pde_t*
copyuvm(pde_t *pgdir, uint sz)
{
  pde_t *d;
  pte_t *pte;
  uint pa, i;
  char *mem;

  if((d = setupkvm()) == 0)
    return 0;

	// NULL POINTER DEREFERENCE - start i = PGSIZE instead of 0
  for(i = PGSIZE; i < sz; i += PGSIZE){
    if((pte = walkpgdir(pgdir, (void*)i, 0)) == 0)
      panic("copyuvm: pte should exist");
    if(!(*pte & PTE_P))
      panic("copyuvm: page not present");
    pa = PTE_ADDR(*pte);
    if((mem = kalloc()) == 0)
      goto bad;
    memmove(mem, (char*)pa, PGSIZE);

		// POTENTIALLY EDIT THIS for Read-Only (maintain old page protections)
		uint w_mask = (*pte & PTE_W) == 0 ? 0x000: PTE_W;	
    if(mappages(d, (void*)i, PGSIZE, PADDR(mem), w_mask|PTE_U) < 0)
      goto bad;
    /** if(mappages(d, (void*)i, PGSIZE, PADDR(mem), PTE_W|PTE_U) < 0)
      *   goto bad; */
  }
  return d;

bad:
  freevm(d);
  return 0;
}

// Map user virtual address to kernel physical address.
char*
uva2ka(pde_t *pgdir, char *uva)
{
  pte_t *pte;

  pte = walkpgdir(pgdir, uva, 0);
  if((*pte & PTE_P) == 0)
    return 0;
  if((*pte & PTE_U) == 0)
    return 0;
  return (char*)PTE_ADDR(*pte);
}

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
int
copyout(pde_t *pgdir, uint va, void *p, uint len)
{
  char *buf, *pa0;
  uint n, va0;
  
  buf = (char*)p;
  while(len > 0){
    va0 = (uint)PGROUNDDOWN(va);
    pa0 = uva2ka(pgdir, (char*)va0);
    if(pa0 == 0)
      return -1;
    n = PGSIZE - (va - va0);
    if(n > len)
      n = len;
    memmove(pa0 + (va - va0), buf, n);
    len -= n;
    buf += n;
    va = va0 + PGSIZE;
  }
  return 0;
}

/*
 * Read-Only VM
 * Meant to be called in sysproc.c
 *	-sys_mprotect(void)
 *	-sys_munprotect(void) 
 */
int
protect_pages(pde_t* pgdir, void* addr, int len, uint p_size) {
  uint addr_num = (uint)addr; // start address
	uint max_addr = addr_num + (uint)(PGSIZE * len); // end address

	// checks page alignment, nonzero length, within process addresss space
	if(addr_num % PGSIZE != 0 || len <= 0 || max_addr > p_size)
		return -1;

	pte_t* pte;

	// change protection bits
	for(; addr_num < max_addr; addr_num += PGSIZE) {
		if((pte = walkpgdir(pgdir, (void*)addr_num, 0)) == 0) // if we run into an unmapped page, abort (?)
			return -1;
		// to make read-only, you zero out the writeable bit
		// to make read/write, you set the writeable bit
		*pte &= ~(PTE_W);
		
		// let hardware know of change by making PTBR point to itself
		lcr3(PADDR(pgdir));
	}
	return 0;
}

int
unprotect_pages(pde_t* pgdir, void* addr, int len, uint p_size) {
  uint addr_num = (uint)addr; // start address
	uint max_addr = addr_num + (uint)(PGSIZE * len); // end address

	// checks page alignment, nonzero length, within process addresss space
	if(addr_num % PGSIZE != 0 || len <= 0 || max_addr > p_size)
		return -1;

	pte_t* pte;

	// change protection bits
	for(; addr_num < max_addr; addr_num += PGSIZE) {
		if((pte = walkpgdir(pgdir, (void*)addr_num, 0)) == 0) // if we run into an unmapped page, abort (?)
			return -1;
		// to make read-only, you zero out the writeable bit
		// to make read/write, you set the writeable bit
		*pte |= PTE_W;

		// let hardware know of change by making PTBR point to itself
		lcr3(PADDR(pgdir));
	}
	return 0;
}
