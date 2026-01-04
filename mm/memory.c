/*
 * memory.c - Memory management for x86_64
 *
 * 64-bit page table structure:
 * - PML4: 512 entries, each covering 512GB (at pg_dir = 0x1000)
 * - PDPT: 512 entries, each covering 1GB
 * - PD: 512 entries, each covering 2MB
 * - PT: 512 entries, each covering 4KB
 *
 * Virtual address breakdown (48-bit canonical):
 * Bits 47-39: PML4 index (9 bits)
 * Bits 38-30: PDPT index (9 bits)
 * Bits 29-21: PD index (9 bits)
 * Bits 20-12: PT index (9 bits)
 * Bits 11-0: Page offset (12 bits)
 */

#include <signal.h>

#include <linux/config.h>
#include <linux/head.h>
#include <linux/kernel.h>
#include <asm/system.h>

int do_exit(long code);

/* PML4 is at physical address 0x1000 */
#define PML4_ADDR 0x1000

/* Invalidate TLB by reloading CR3 */
static inline void invalidate(void)
{
	unsigned long cr3;
	__asm__ volatile ("movq %%cr3, %0" : "=r"(cr3));
	__asm__ volatile ("movq %0, %%cr3" :: "r"(cr3));
}

/* Page table entry flags */
#define PAGE_PRESENT  0x001
#define PAGE_WRITE    0x002
#define PAGE_USER     0x004
#define PAGE_ACCESSED 0x020
#define PAGE_DIRTY    0x040

/* Extract page table indices from virtual address */
#define PML4_INDEX(addr)  (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr)  (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)    (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)    (((addr) >> 12) & 0x1FF)
#define PAGE_OFFSET(addr) ((addr) & 0xFFF)

/* Get physical address from page table entry */
#define PTE_ADDR(pte) ((pte) & 0x000FFFFFFFFFF000UL)

#if (BUFFER_END < 0x100000)
#define LOW_MEM 0x100000
#else
#define LOW_MEM BUFFER_END
#endif

/* these are not to be changed - they are calculated from the above */
#define PAGING_MEMORY (HIGH_MEMORY - LOW_MEM)
#define PAGING_PAGES (PAGING_MEMORY/4096)
#define MAP_NR(addr) (((addr)-LOW_MEM)>>12)

#if (PAGING_PAGES < 10)
#error "Won't work"
#endif

/* Copy 4KB page */
static inline void copy_page(unsigned long from, unsigned long to)
{
	unsigned long *src = (unsigned long *)from;
	unsigned long *dst = (unsigned long *)to;
	int i;
	for (i = 0; i < 4096/sizeof(unsigned long); i++)
		dst[i] = src[i];
}

static unsigned short mem_map [ PAGING_PAGES ] = {0,};

/*
 * Get physical address of first (actually last :-) free page, and mark it
 * used. If no free pages left, return 0.
 */
unsigned long get_free_page(void)
{
	int i;
	unsigned long page;
	
	/* Search backwards for a free page */
	for (i = PAGING_PAGES - 1; i >= 0; i--) {
		if (mem_map[i] == 0) {
			mem_map[i] = 1;
			page = LOW_MEM + (i << 12);
			/* Zero the page */
			{
				unsigned long *p = (unsigned long *)page;
				int j;
				for (j = 0; j < 4096/sizeof(unsigned long); j++)
					p[j] = 0;
			}
			return page;
		}
	}
	return 0;
}

/*
 * Free a page of memory at physical address 'addr'.
 */
void free_page(unsigned long addr)
{
	if (addr < LOW_MEM) return;
	if (addr > HIGH_MEMORY)
		panic("trying to free nonexistent page");
	addr -= LOW_MEM;
	addr >>= 12;
	if (mem_map[addr]--) return;
	mem_map[addr] = 0;
	panic("trying to free free page");
}

/*
 * Get pointer to PML4 (page map level 4)
 */
static inline unsigned long *get_pml4(void)
{
	return (unsigned long *)PML4_ADDR;
}

/*
 * Get or create PDPT entry from PML4
 */
static unsigned long *get_pdpt(unsigned long addr, int create)
{
	unsigned long *pml4 = get_pml4();
	unsigned long pml4e = pml4[PML4_INDEX(addr)];
	
	if (!(pml4e & PAGE_PRESENT)) {
		if (!create)
			return 0;
		unsigned long page = get_free_page();
		if (!page)
			return 0;
		pml4[PML4_INDEX(addr)] = page | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
		return (unsigned long *)page;
	}
	return (unsigned long *)PTE_ADDR(pml4e);
}

/*
 * Get or create PD entry from PDPT
 */
static unsigned long *get_pd(unsigned long addr, int create)
{
	unsigned long *pdpt = get_pdpt(addr, create);
	if (!pdpt)
		return 0;
	
	unsigned long pdpte = pdpt[PDPT_INDEX(addr)];
	
	if (!(pdpte & PAGE_PRESENT)) {
		if (!create)
			return 0;
		unsigned long page = get_free_page();
		if (!page)
			return 0;
		pdpt[PDPT_INDEX(addr)] = page | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
		return (unsigned long *)page;
	}
	return (unsigned long *)PTE_ADDR(pdpte);
}

/*
 * Get or create PT entry from PD
 */
static unsigned long *get_pt(unsigned long addr, int create)
{
	unsigned long *pd = get_pd(addr, create);
	if (!pd)
		return 0;
	
	unsigned long pde = pd[PD_INDEX(addr)];
	
	if (!(pde & PAGE_PRESENT)) {
		if (!create)
			return 0;
		unsigned long page = get_free_page();
		if (!page)
			return 0;
		pd[PD_INDEX(addr)] = page | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
		return (unsigned long *)page;
	}
	return (unsigned long *)PTE_ADDR(pde);
}

/*
 * Get page table entry for a virtual address
 */
static unsigned long *get_pte(unsigned long addr, int create)
{
	unsigned long *pt = get_pt(addr, create);
	if (!pt)
		return 0;
	return &pt[PT_INDEX(addr)];
}

/*
 * This function frees a continuous block of page tables.
 * For 64-bit, we work in 2MB blocks (one PD entry = 512 PT entries).
 */
int free_page_tables(unsigned long from, unsigned long size)
{
	unsigned long addr;
	unsigned long *pt;
	int i;

	if (from & 0x1FFFFF)
		panic("free_page_tables called with wrong alignment");
	if (!from)
		panic("Trying to free up swapper memory space");
	
	size = (size + 0x1FFFFF) & ~0x1FFFFF;  /* Round up to 2MB */
	
	for (addr = from; addr < from + size; addr += 0x1000) {
		unsigned long *pte = get_pte(addr, 0);
		if (pte && (*pte & PAGE_PRESENT)) {
			unsigned long page = PTE_ADDR(*pte);
			if (page >= LOW_MEM)
				free_page(page);
			*pte = 0;
		}
	}
	
	invalidate();
	return 0;
}

/*
 * Copy page tables for fork().
 * For 64-bit, we copy in 2MB blocks.
 */
int copy_page_tables(unsigned long from, unsigned long to, long size)
{
	unsigned long addr;

	if ((from & 0x1FFFFF) || (to & 0x1FFFFF))
		panic("copy_page_tables called with wrong alignment");
	
	size = (size + 0x1FFFFF) & ~0x1FFFFF;  /* Round up to 2MB */
	
	for (addr = 0; addr < size; addr += 0x1000) {
		unsigned long *from_pte = get_pte(from + addr, 0);
		if (!from_pte || !(*from_pte & PAGE_PRESENT))
			continue;
		
		unsigned long *to_pte = get_pte(to + addr, 1);
		if (!to_pte)
			return -1;  /* Out of memory */
		
		unsigned long this_page = *from_pte;
		
		/* Make both pages read-only for copy-on-write */
		this_page &= ~PAGE_WRITE;
		*to_pte = this_page;
		
		/* If page is above LOW_MEM, mark source read-only too and increment ref count */
		unsigned long phys = PTE_ADDR(this_page);
		if (phys >= LOW_MEM) {
			*from_pte = this_page;
			mem_map[MAP_NR(phys)]++;
		}
	}
	
	invalidate();
	return 0;
}

/*
 * Put a page in memory at the wanted address.
 */
unsigned long put_page(unsigned long page, unsigned long address)
{
	unsigned long *pte;

	if (page < LOW_MEM || page > HIGH_MEMORY)
		printk("Trying to put page %p at %p\n", page, address);
	if (mem_map[MAP_NR(page)] != 1)
		printk("mem_map disagrees with %p at %p\n", page, address);
	
	pte = get_pte(address, 1);
	if (!pte)
		return 0;
	
	*pte = page | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	return page;
}

/*
 * Un-write-protect a page (for copy-on-write)
 */
void un_wp_page(unsigned long *table_entry)
{
	unsigned long old_page, new_page;

	old_page = PTE_ADDR(*table_entry);
	if (old_page >= LOW_MEM && mem_map[MAP_NR(old_page)] == 1) {
		*table_entry |= PAGE_WRITE;
		invalidate();
		return;
	}
	if (!(new_page = get_free_page()))
		do_exit(SIGSEGV);
	if (old_page >= LOW_MEM)
		mem_map[MAP_NR(old_page)]--;
	*table_entry = new_page | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;
	invalidate();
	copy_page(old_page, new_page);
}

/*
 * Handle write-protect page fault
 */
void do_wp_page(unsigned long error_code, unsigned long address)
{
	unsigned long *pte = get_pte(address, 0);
	if (pte)
		un_wp_page(pte);
}

/*
 * Verify write access to address (used before writing to user space)
 */
void write_verify(unsigned long address)
{
	unsigned long *pte = get_pte(address, 0);
	if (!pte)
		return;
	if ((*pte & (PAGE_PRESENT | PAGE_WRITE)) == PAGE_PRESENT)
		un_wp_page(pte);
}

/*
 * Handle page fault for non-present page
 */
void do_no_page(unsigned long error_code, unsigned long address)
{
	unsigned long page;

	if ((page = get_free_page()))
		if (put_page(page, address))
			return;
	do_exit(SIGSEGV);
}

/*
 * Calculate and display memory usage
 */
void calc_mem(void)
{
	int i, free = 0;

	for (i = 0; i < PAGING_PAGES; i++)
		if (!mem_map[i])
			free++;
	printk("%d pages free (of %d)\n\r", free, PAGING_PAGES);
}
