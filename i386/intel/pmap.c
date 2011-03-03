/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	File:	pmap.c
 *	Author:	Avadis Tevanian, Jr., Michael Wayne Young
 *	(These guys wrote the Vax version)
 *
 *	Physical Map management code for Intel i386, and i486.
 *
 *	Manages physical address maps.
 *
 *	In addition to hardware address maps, this
 *	module is called upon to provide software-use-only
 *	maps which may or may not be stored in the same
 *	form as hardware maps.  These pseudo-maps are
 *	used to store intermediate results from copy
 *	operations to and from address spaces.
 *
 *	Since the information managed by this module is
 *	also stored by the logical address mapping module,
 *	this module may throw away valid virtual-to-physical
 *	mappings at almost any time.  However, invalidations
 *	of virtual-to-physical mappings must be done as
 *	requested.
 *
 *	In order to cope with hardware architectures which
 *	make virtual-to-physical map invalidates expensive,
 *	this module may delay invalidate or reduced protection
 *	operations until such time as they are actually
 *	necessary.  This module is given full information as
 *	to which processors are currently using which maps,
 *	and to when physical maps must be made correct.
 */

#include <string.h>

#include <mach/machine/vm_types.h>

#include <mach/boolean.h>
#include <kern/debug.h>
#include <kern/printf.h>
#include <kern/thread.h>
#include <kern/zalloc.h>

#include <kern/lock.h>

#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include <i386/vm_param.h>
#include <mach/vm_prot.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_user.h>

#include <mach/machine/vm_param.h>
#include <mach/xen.h>
#include <machine/thread.h>
#include <i386/cpu_number.h>
#include <i386/proc_reg.h>
#include <i386/locore.h>
#include <i386/model_dep.h>

#ifdef	MACH_PSEUDO_PHYS
#define	WRITE_PTE(pte_p, pte_entry)		*(pte_p) = pte_entry?pa_to_ma(pte_entry):0;
#else	/* MACH_PSEUDO_PHYS */
#define	WRITE_PTE(pte_p, pte_entry)		*(pte_p) = (pte_entry);
#endif	/* MACH_PSEUDO_PHYS */

/*
 *	Private data structures.
 */

/*
 *	For each vm_page_t, there is a list of all currently
 *	valid virtual mappings of that page.  An entry is
 *	a pv_entry_t; the list is the pv_table.
 */

typedef struct pv_entry {
	struct pv_entry	*next;		/* next pv_entry */
	pmap_t		pmap;		/* pmap where mapping lies */
	vm_offset_t	va;		/* virtual address for mapping */
} *pv_entry_t;

#define PV_ENTRY_NULL	((pv_entry_t) 0)

pv_entry_t	pv_head_table;		/* array of entries, one per page */

/*
 *	pv_list entries are kept on a list that can only be accessed
 *	with the pmap system locked (at SPLVM, not in the cpus_active set).
 *	The list is refilled from the pv_list_zone if it becomes empty.
 */
pv_entry_t	pv_free_list;		/* free list at SPLVM */
decl_simple_lock_data(, pv_free_list_lock)

#define	PV_ALLOC(pv_e) { \
	simple_lock(&pv_free_list_lock); \
	if ((pv_e = pv_free_list) != 0) { \
	    pv_free_list = pv_e->next; \
	} \
	simple_unlock(&pv_free_list_lock); \
}

#define	PV_FREE(pv_e) { \
	simple_lock(&pv_free_list_lock); \
	pv_e->next = pv_free_list; \
	pv_free_list = pv_e; \
	simple_unlock(&pv_free_list_lock); \
}

zone_t		pv_list_zone;		/* zone of pv_entry structures */

/*
 *	Each entry in the pv_head_table is locked by a bit in the
 *	pv_lock_table.  The lock bits are accessed by the physical
 *	address of the page they lock.
 */

char	*pv_lock_table;		/* pointer to array of bits */
#define pv_lock_table_size(n)	(((n)+BYTE_SIZE-1)/BYTE_SIZE)

/* Has pmap_init completed? */
boolean_t	pmap_initialized = FALSE;

/*
 *	Range of kernel virtual addresses available for kernel memory mapping.
 *	Does not include the virtual addresses used to map physical memory 1-1.
 *	Initialized by pmap_bootstrap.
 */
vm_offset_t kernel_virtual_start;
vm_offset_t kernel_virtual_end;

/* XXX stupid fixed limit - get rid */
vm_size_t morevm = 40 * 1024 * 1024;	/* VM space for kernel map */

/*
 *	Index into pv_head table, its lock bits, and the modify/reference
 *	bits starting at phys_first_addr.
 */
#define pa_index(pa)	(atop(pa - phys_first_addr))

#define pai_to_pvh(pai)		(&pv_head_table[pai])
#define lock_pvh_pai(pai)	(bit_lock(pai, pv_lock_table))
#define unlock_pvh_pai(pai)	(bit_unlock(pai, pv_lock_table))

/*
 *	Array of physical page attribites for managed pages.
 *	One byte per physical page.
 */
char	*pmap_phys_attributes;

/*
 *	Physical page attributes.  Copy bits from PTE definition.
 */
#define	PHYS_MODIFIED	INTEL_PTE_MOD	/* page modified */
#define	PHYS_REFERENCED	INTEL_PTE_REF	/* page referenced */

/*
 *	Amount of virtual memory mapped by one
 *	page-directory entry.
 */
#define	PDE_MAPPED_SIZE		(pdenum2lin(1))

/*
 *	We allocate page table pages directly from the VM system
 *	through this object.  It maps physical memory.
 */
vm_object_t	pmap_object = VM_OBJECT_NULL;

/*
 *	Locking and TLB invalidation
 */

/*
 *	Locking Protocols:
 *
 *	There are two structures in the pmap module that need locking:
 *	the pmaps themselves, and the per-page pv_lists (which are locked
 *	by locking the pv_lock_table entry that corresponds to the pv_head
 *	for the list in question.)  Most routines want to lock a pmap and
 *	then do operations in it that require pv_list locking -- however
 *	pmap_remove_all and pmap_copy_on_write operate on a physical page
 *	basis and want to do the locking in the reverse order, i.e. lock
 *	a pv_list and then go through all the pmaps referenced by that list.
 *	To protect against deadlock between these two cases, the pmap_lock
 *	is used.  There are three different locking protocols as a result:
 *
 *  1.  pmap operations only (pmap_extract, pmap_access, ...)  Lock only
 *		the pmap.
 *
 *  2.  pmap-based operations (pmap_enter, pmap_remove, ...)  Get a read
 *		lock on the pmap_lock (shared read), then lock the pmap
 *		and finally the pv_lists as needed [i.e. pmap lock before
 *		pv_list lock.]
 *
 *  3.  pv_list-based operations (pmap_remove_all, pmap_copy_on_write, ...)
 *		Get a write lock on the pmap_lock (exclusive write); this
 *		also guaranteees exclusive access to the pv_lists.  Lock the
 *		pmaps as needed.
 *
 *	At no time may any routine hold more than one pmap lock or more than
 *	one pv_list lock.  Because interrupt level routines can allocate
 *	mbufs and cause pmap_enter's, the pmap_lock and the lock on the
 *	kernel_pmap can only be held at splvm.
 */

#if	NCPUS > 1
/*
 *	We raise the interrupt level to splvm, to block interprocessor
 *	interrupts during pmap operations.  We must take the CPU out of
 *	the cpus_active set while interrupts are blocked.
 */
#define SPLVM(spl)	{ \
	spl = splvm(); \
	i_bit_clear(cpu_number(), &cpus_active); \
}

#define SPLX(spl)	{ \
	i_bit_set(cpu_number(), &cpus_active); \
	splx(spl); \
}

/*
 *	Lock on pmap system
 */
lock_data_t	pmap_system_lock;

#define PMAP_READ_LOCK(pmap, spl) { \
	SPLVM(spl); \
	lock_read(&pmap_system_lock); \
	simple_lock(&(pmap)->lock); \
}

#define PMAP_WRITE_LOCK(spl) { \
	SPLVM(spl); \
	lock_write(&pmap_system_lock); \
}

#define PMAP_READ_UNLOCK(pmap, spl) { \
	simple_unlock(&(pmap)->lock); \
	lock_read_done(&pmap_system_lock); \
	SPLX(spl); \
}

#define PMAP_WRITE_UNLOCK(spl) { \
	lock_write_done(&pmap_system_lock); \
	SPLX(spl); \
}

#define PMAP_WRITE_TO_READ_LOCK(pmap) { \
	simple_lock(&(pmap)->lock); \
	lock_write_to_read(&pmap_system_lock); \
}

#define LOCK_PVH(index)		(lock_pvh_pai(index))

#define UNLOCK_PVH(index)	(unlock_pvh_pai(index))

#define PMAP_UPDATE_TLBS(pmap, s, e) \
{ \
	cpu_set	cpu_mask = 1 << cpu_number(); \
	cpu_set	users; \
 \
	/* Since the pmap is locked, other updates are locked */ \
	/* out, and any pmap_activate has finished. */ \
 \
	/* find other cpus using the pmap */ \
	users = (pmap)->cpus_using & ~cpu_mask; \
	if (users) { \
	    /* signal them, and wait for them to finish */ \
	    /* using the pmap */ \
	    signal_cpus(users, (pmap), (s), (e)); \
	    while ((pmap)->cpus_using & cpus_active & ~cpu_mask) \
		continue; \
	} \
 \
	/* invalidate our own TLB if pmap is in use */ \
	if ((pmap)->cpus_using & cpu_mask) { \
	    INVALIDATE_TLB((pmap), (s), (e)); \
	} \
}

#else	/* NCPUS > 1 */

#define SPLVM(spl) ((void)(spl))
#define SPLX(spl) ((void)(spl))

#define PMAP_READ_LOCK(pmap, spl)	SPLVM(spl)
#define PMAP_WRITE_LOCK(spl)		SPLVM(spl)
#define PMAP_READ_UNLOCK(pmap, spl)	SPLX(spl)
#define PMAP_WRITE_UNLOCK(spl)		SPLX(spl)
#define PMAP_WRITE_TO_READ_LOCK(pmap)

#define LOCK_PVH(index)
#define UNLOCK_PVH(index)

#define PMAP_UPDATE_TLBS(pmap, s, e) { \
	/* invalidate our own TLB if pmap is in use */ \
	if ((pmap)->cpus_using) { \
	    INVALIDATE_TLB((pmap), (s), (e)); \
	} \
}

#endif	/* NCPUS > 1 */

#define MAX_TBIS_SIZE	32		/* > this -> TBIA */ /* XXX */

#ifdef	MACH_HYP
#if 1
#define INVALIDATE_TLB(pmap, s, e) hyp_mmuext_op_void(MMUEXT_TLB_FLUSH_LOCAL)
#else
#define INVALIDATE_TLB(pmap, s, e) do { \
	if (__builtin_constant_p((e) - (s)) \
		&& (e) - (s) == PAGE_SIZE) \
		hyp_invlpg((pmap) == kernel_pmap ? kvtolin(s) : (s)); \
	else \
		hyp_mmuext_op_void(MMUEXT_TLB_FLUSH_LOCAL); \
} while(0)
#endif
#else	/* MACH_HYP */
#if 0
/* It is hard to know when a TLB flush becomes less expensive than a bunch of
 * invlpgs.  But it surely is more expensive than just one invlpg.  */
#define INVALIDATE_TLB(pmap, s, e) { \
	if (__builtin_constant_p((e) - (s)) \
		&& (e) - (s) == PAGE_SIZE) \
		invlpg_linear(s); \
	else \
		flush_tlb(); \
}
#else
#define INVALIDATE_TLB(pmap, s, e) { \
	(void) (pmap); \
	(void) (s); \
	(void) (e); \
	flush_tlb(); \
}
#endif
#endif	/* MACH_HYP */


#if	NCPUS > 1
/*
 *	Structures to keep track of pending TLB invalidations
 */

#define UPDATE_LIST_SIZE	4

struct pmap_update_item {
	pmap_t		pmap;		/* pmap to invalidate */
	vm_offset_t	start;		/* start address to invalidate */
	vm_offset_t	end;		/* end address to invalidate */
} ;

typedef	struct pmap_update_item	*pmap_update_item_t;

/*
 *	List of pmap updates.  If the list overflows,
 *	the last entry is changed to invalidate all.
 */
struct pmap_update_list {
	decl_simple_lock_data(,	lock)
	int			count;
	struct pmap_update_item	item[UPDATE_LIST_SIZE];
} ;
typedef	struct pmap_update_list	*pmap_update_list_t;

struct pmap_update_list	cpu_update_list[NCPUS];

#endif	/* NCPUS > 1 */

/*
 *	Other useful macros.
 */
#define current_pmap()		(vm_map_pmap(current_thread()->task->map))
#define pmap_in_use(pmap, cpu)	(((pmap)->cpus_using & (1 << (cpu))) != 0)

struct pmap	kernel_pmap_store;
pmap_t		kernel_pmap;

struct zone	*pmap_zone;		/* zone of pmap structures */

int		pmap_debug = 0;		/* flag for debugging prints */

#if 0
int		ptes_per_vm_page;	/* number of hardware ptes needed
					   to map one VM page. */
#else
#define		ptes_per_vm_page	1
#endif

unsigned int	inuse_ptepages_count = 0;	/* debugging */

extern char end;

/*
 * Pointer to the basic page directory for the kernel.
 * Initialized by pmap_bootstrap().
 */
pt_entry_t *kernel_page_dir;

void pmap_remove_range();	/* forward */
#if	NCPUS > 1
void signal_cpus();		/* forward */
#endif	/* NCPUS > 1 */

static inline pt_entry_t *
pmap_pde(pmap_t pmap, vm_offset_t addr)
{
	if (pmap == kernel_pmap)
		addr = kvtolin(addr);
	return &pmap->dirbase[lin2pdenum(addr)];
}

/*
 *	Given an offset and a map, compute the address of the
 *	pte.  If the address is invalid with respect to the map
 *	then PT_ENTRY_NULL is returned (and the map may need to grow).
 *
 *	This is only used internally.
 */
pt_entry_t *
pmap_pte(pmap_t pmap, vm_offset_t addr)
{
	pt_entry_t	*ptp;
	pt_entry_t	pte;

	if (pmap->dirbase == 0)
		return(PT_ENTRY_NULL);
	pte = *pmap_pde(pmap, addr);
	if ((pte & INTEL_PTE_VALID) == 0)
		return(PT_ENTRY_NULL);
	ptp = (pt_entry_t *)ptetokv(pte);
	return(&ptp[ptenum(addr)]);
}

#define DEBUG_PTE_PAGE	0

#if	DEBUG_PTE_PAGE
void ptep_check(ptep)
	ptep_t	ptep;
{
	register pt_entry_t	*pte, *epte;
	int			ctu, ctw;

	/* check the use and wired counts */
	if (ptep == PTE_PAGE_NULL)
		return;
	pte = pmap_pte(ptep->pmap, ptep->va);
	epte = pte + INTEL_PGBYTES/sizeof(pt_entry_t);
	ctu = 0;
	ctw = 0;
	while (pte < epte) {
		if (pte->pfn != 0) {
			ctu++;
			if (pte->wired)
				ctw++;
		}
		pte += ptes_per_vm_page;
	}

	if (ctu != ptep->use_count || ctw != ptep->wired_count) {
		printf("use %d wired %d - actual use %d wired %d\n",
		    	ptep->use_count, ptep->wired_count, ctu, ctw);
		panic("pte count");
	}
}
#endif	/* DEBUG_PTE_PAGE */

/*
 *	Map memory at initialization.  The physical addresses being
 *	mapped are not managed and are never unmapped.
 *
 *	For now, VM is already on, we only need to map the
 *	specified memory.
 */
vm_offset_t pmap_map(virt, start, end, prot)
	register vm_offset_t	virt;
	register vm_offset_t	start;
	register vm_offset_t	end;
	register int		prot;
{
	register int		ps;

	ps = PAGE_SIZE;
	while (start < end) {
		pmap_enter(kernel_pmap, virt, start, prot, FALSE);
		virt += ps;
		start += ps;
	}
	return(virt);
}

/*
 *	Back-door routine for mapping kernel VM at initialization.
 * 	Useful for mapping memory outside the range
 *	[phys_first_addr, phys_last_addr) (i.e., devices).
 *	Otherwise like pmap_map.
 */
vm_offset_t pmap_map_bd(virt, start, end, prot)
	register vm_offset_t	virt;
	register vm_offset_t	start;
	register vm_offset_t	end;
	vm_prot_t		prot;
{
	register pt_entry_t	template;
	register pt_entry_t	*pte;
	int			spl;
#ifdef	MACH_XEN
	int n, i = 0;
	struct mmu_update update[HYP_BATCH_MMU_UPDATES];
#endif	/* MACH_XEN */

	template = pa_to_pte(start)
		| INTEL_PTE_NCACHE|INTEL_PTE_WTHRU
		| INTEL_PTE_VALID;
	if (CPU_HAS_FEATURE(CPU_FEATURE_PGE))
		template |= INTEL_PTE_GLOBAL;
	if (prot & VM_PROT_WRITE)
	    template |= INTEL_PTE_WRITE;

	PMAP_READ_LOCK(pmap, spl);
	while (start < end) {
		pte = pmap_pte(kernel_pmap, virt);
		if (pte == PT_ENTRY_NULL)
			panic("pmap_map_bd: Invalid kernel address\n");
#ifdef	MACH_XEN
		update[i].ptr = kv_to_ma(pte);
		update[i].val = pa_to_ma(template);
		i++;
		if (i == HYP_BATCH_MMU_UPDATES) {
			hyp_mmu_update(kvtolin(&update), i, kvtolin(&n), DOMID_SELF);
			if (n != i)
				panic("couldn't pmap_map_bd\n");
			i = 0;
		}
#else	/* MACH_XEN */
		WRITE_PTE(pte, template)
#endif	/* MACH_XEN */
		pte_increment_pa(template);
		virt += PAGE_SIZE;
		start += PAGE_SIZE;
	}
#ifdef	MACH_XEN
	if (i > HYP_BATCH_MMU_UPDATES)
		panic("overflowed array in pmap_map_bd");
	hyp_mmu_update(kvtolin(&update), i, kvtolin(&n), DOMID_SELF);
	if (n != i)
		panic("couldn't pmap_map_bd\n");
#endif	/* MACH_XEN */
	PMAP_READ_UNLOCK(pmap, spl);
	return(virt);
}

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	Allocate the kernel page directory and page tables,
 *	and direct-map all physical memory.
 *	Called with mapping off.
 */
void pmap_bootstrap()
{
	/*
	 * Mapping is turned off; we must reference only physical addresses.
	 * The load image of the system is to be mapped 1-1 physical = virtual.
	 */

	/*
	 *	Set ptes_per_vm_page for general use.
	 */
#if 0
	ptes_per_vm_page = PAGE_SIZE / INTEL_PGBYTES;
#endif

	/*
	 *	The kernel's pmap is statically allocated so we don't
	 *	have to use pmap_create, which is unlikely to work
	 *	correctly at this part of the boot sequence.
	 */

	kernel_pmap = &kernel_pmap_store;

#if	NCPUS > 1
	lock_init(&pmap_system_lock, FALSE);	/* NOT a sleep lock */
#endif	/* NCPUS > 1 */

	simple_lock_init(&kernel_pmap->lock);

	kernel_pmap->ref_count = 1;

	/*
	 * Determine the kernel virtual address range.
	 * It starts at the end of the physical memory
	 * mapped into the kernel address space,
	 * and extends to a stupid arbitrary limit beyond that.
	 */
	kernel_virtual_start = phystokv(phys_last_addr);
	kernel_virtual_end = phystokv(phys_last_addr) + morevm
		+ (phys_last_addr - phys_first_addr);

	if (kernel_virtual_end < kernel_virtual_start
			|| kernel_virtual_end > VM_MAX_KERNEL_ADDRESS)
		kernel_virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Allocate and clear a kernel page directory.
	 */
	/* Note: initial Xen mapping holds at least 512kB free mapped page.
	 * We use that for directly building our linear mapping. */
#if PAE
	{
		vm_offset_t addr;
		init_alloc_aligned(PDPNUM * INTEL_PGBYTES, &addr);
		kernel_pmap->dirbase = kernel_page_dir = (pt_entry_t*)phystokv(addr);
	}
	kernel_pmap->pdpbase = (pt_entry_t*)phystokv(pmap_grab_page());
	{
		int i;
		for (i = 0; i < PDPNUM; i++)
			WRITE_PTE(&kernel_pmap->pdpbase[i], pa_to_pte(_kvtophys((void *) kernel_pmap->dirbase + i * INTEL_PGBYTES)) | INTEL_PTE_VALID);
	}
#else	/* PAE */
	kernel_pmap->dirbase = kernel_page_dir = (pt_entry_t*)phystokv(pmap_grab_page());
#endif	/* PAE */
	{
		int i;
		for (i = 0; i < NPDES; i++)
			kernel_pmap->dirbase[i] = 0;
	}

#ifdef	MACH_XEN
	/* We don't actually deal with the CR3 register content at all */
	hyp_vm_assist(VMASST_CMD_enable, VMASST_TYPE_pae_extended_cr3);
	/*
	 * Xen may only provide as few as 512KB extra bootstrap linear memory,
	 * which is far from enough to map all available memory, so we need to
	 * map more bootstrap linear memory. We here map 1 (resp. 4 for PAE)
	 * other L1 table(s), thus 4MiB extra memory (resp. 8MiB), which is
	 * enough for a pagetable mapping 4GiB.
	 */
#ifdef PAE
#define NSUP_L1 4
#else
#define NSUP_L1 1
#endif
	pt_entry_t *l1_map[NSUP_L1];
	{
		pt_entry_t *base = (pt_entry_t*) boot_info.pt_base;
		int i;
		int n_l1map;
#ifdef	PAE
		pt_entry_t *l2_map = (pt_entry_t*) ptetokv(base[0]);
#else	/* PAE */
		pt_entry_t *l2_map = base;
#endif	/* PAE */
		for (n_l1map = 0, i = lin2pdenum(VM_MIN_KERNEL_ADDRESS); i < NPTES; i++) {
			if (!(l2_map[i] & INTEL_PTE_VALID)) {
				struct mmu_update update;
				int j, n;

				l1_map[n_l1map] = (pt_entry_t*) phystokv(pmap_grab_page());
				for (j = 0; j < NPTES; j++)
					l1_map[n_l1map][j] = (((pt_entry_t)pfn_to_mfn((i - lin2pdenum(VM_MIN_KERNEL_ADDRESS)) * NPTES + j)) << PAGE_SHIFT) | INTEL_PTE_VALID | INTEL_PTE_WRITE;
				pmap_set_page_readonly_init(l1_map[n_l1map]);
				if (!hyp_mmuext_op_mfn (MMUEXT_PIN_L1_TABLE, kv_to_mfn (l1_map[n_l1map])))
					panic("couldn't pin page %p(%p)", l1_map[n_l1map], (vm_offset_t) kv_to_ma (l1_map[n_l1map]));
				update.ptr = kv_to_ma(&l2_map[i]);
				update.val = kv_to_ma(l1_map[n_l1map]) | INTEL_PTE_VALID | INTEL_PTE_WRITE;
				hyp_mmu_update(kv_to_la(&update), 1, kv_to_la(&n), DOMID_SELF);
				if (n != 1)
					panic("couldn't complete bootstrap map");
				/* added the last L1 table, can stop */
				if (++n_l1map >= NSUP_L1)
					break;
			}
		}
	}
#endif	/* MACH_XEN */

	/*
	 * Allocate and set up the kernel page tables.
	 */
	{
		vm_offset_t va;
		pt_entry_t global = CPU_HAS_FEATURE(CPU_FEATURE_PGE) ? INTEL_PTE_GLOBAL : 0;

		/*
		 * Map virtual memory for all known physical memory, 1-1,
		 * from phys_first_addr to phys_last_addr.
		 * Make any mappings completely in the kernel's text segment read-only.
		 *
		 * Also allocate some additional all-null page tables afterwards
		 * for kernel virtual memory allocation,
		 * because this PMAP module is too stupid
		 * to allocate new kernel page tables later.
		 * XX fix this
		 */
		for (va = phystokv(phys_first_addr); va < kernel_virtual_end; )
		{
			pt_entry_t *pde = kernel_page_dir + lin2pdenum(kvtolin(va));
			pt_entry_t *ptable = (pt_entry_t*)phystokv(pmap_grab_page());
			pt_entry_t *pte;

			/* Initialize the page directory entry.  */
			WRITE_PTE(pde, pa_to_pte((vm_offset_t)_kvtophys(ptable))
				| INTEL_PTE_VALID | INTEL_PTE_WRITE);

			/* Initialize the page table.  */
			for (pte = ptable; (va < phystokv(phys_last_addr)) && (pte < ptable+NPTES); pte++)
			{
				if ((pte - ptable) < ptenum(va))
				{
					WRITE_PTE(pte, 0);
				}
				else
#ifdef	MACH_XEN
				if (va == (vm_offset_t) &hyp_shared_info)
				{
					*pte = boot_info.shared_info | INTEL_PTE_VALID | INTEL_PTE_WRITE;
					va += INTEL_PGBYTES;
				}
				else
#endif	/* MACH_XEN */
				{
					extern char _start[], etext[];

					if (((va >= (vm_offset_t) _start)
					    && (va + INTEL_PGBYTES <= (vm_offset_t)etext))
#ifdef	MACH_XEN
					    || (va >= (vm_offset_t) boot_info.pt_base
					    && (va + INTEL_PGBYTES <=
					    (vm_offset_t) ptable + INTEL_PGBYTES))
#endif	/* MACH_XEN */
					    )
					{
						WRITE_PTE(pte, pa_to_pte(_kvtophys(va))
							| INTEL_PTE_VALID | global);
					}
					else
					{
#ifdef	MACH_XEN
						/* Keep supplementary L1 pages read-only */
						int i;
						for (i = 0; i < NSUP_L1; i++)
							if (va == (vm_offset_t) l1_map[i]) {
								WRITE_PTE(pte, pa_to_pte(_kvtophys(va))
									| INTEL_PTE_VALID | global);
								break;
							}
						if (i == NSUP_L1)
#endif	/* MACH_XEN */
							WRITE_PTE(pte, pa_to_pte(_kvtophys(va))
								| INTEL_PTE_VALID | INTEL_PTE_WRITE | global)

					}
					va += INTEL_PGBYTES;
				}
			}
			for (; pte < ptable+NPTES; pte++)
			{
				WRITE_PTE(pte, 0);
				va += INTEL_PGBYTES;
			}
#ifdef	MACH_XEN
			pmap_set_page_readonly_init(ptable);
			if (!hyp_mmuext_op_mfn (MMUEXT_PIN_L1_TABLE, kv_to_mfn (ptable)))
				panic("couldn't pin page %p(%p)\n", ptable, (vm_offset_t) kv_to_ma (ptable));
#endif	/* MACH_XEN */
		}
	}

	/* Architecture-specific code will turn on paging
	   soon after we return from here.  */
}

#ifdef	MACH_XEN
/* These are only required because of Xen security policies */

/* Set back a page read write */
void pmap_set_page_readwrite(void *_vaddr) {
	vm_offset_t vaddr = (vm_offset_t) _vaddr;
	vm_offset_t paddr = kvtophys(vaddr);
	vm_offset_t canon_vaddr = phystokv(paddr);
	if (hyp_do_update_va_mapping (kvtolin(vaddr), pa_to_pte (pa_to_ma(paddr)) | INTEL_PTE_VALID | INTEL_PTE_WRITE, UVMF_NONE))
		panic("couldn't set hiMMU readwrite for addr %p(%p)\n", vaddr, (vm_offset_t) pa_to_ma (paddr));
	if (canon_vaddr != vaddr)
		if (hyp_do_update_va_mapping (kvtolin(canon_vaddr), pa_to_pte (pa_to_ma(paddr)) | INTEL_PTE_VALID | INTEL_PTE_WRITE, UVMF_NONE))
			panic("couldn't set hiMMU readwrite for paddr %p(%p)\n", canon_vaddr, (vm_offset_t) pa_to_ma (paddr));
}

/* Set a page read only (so as to pin it for instance) */
void pmap_set_page_readonly(void *_vaddr) {
	vm_offset_t vaddr = (vm_offset_t) _vaddr;
	vm_offset_t paddr = kvtophys(vaddr);
	vm_offset_t canon_vaddr = phystokv(paddr);
	if (*pmap_pde(kernel_pmap, vaddr) & INTEL_PTE_VALID) {
		if (hyp_do_update_va_mapping (kvtolin(vaddr), pa_to_pte (pa_to_ma(paddr)) | INTEL_PTE_VALID, UVMF_NONE))
			panic("couldn't set hiMMU readonly for vaddr %p(%p)\n", vaddr, (vm_offset_t) pa_to_ma (paddr));
	}
	if (canon_vaddr != vaddr &&
		*pmap_pde(kernel_pmap, canon_vaddr) & INTEL_PTE_VALID) {
		if (hyp_do_update_va_mapping (kvtolin(canon_vaddr), pa_to_pte (pa_to_ma(paddr)) | INTEL_PTE_VALID, UVMF_NONE))
			panic("couldn't set hiMMU readonly for vaddr %p canon_vaddr %p paddr %p (%p)\n", vaddr, canon_vaddr, paddr, (vm_offset_t) pa_to_ma (paddr));
	}
}

/* This needs to be called instead of pmap_set_page_readonly as long as RC3
 * still points to the bootstrap dirbase.  */
void pmap_set_page_readonly_init(void *_vaddr) {
	vm_offset_t vaddr = (vm_offset_t) _vaddr;
#if PAE
	pt_entry_t *pdpbase = (void*) boot_info.pt_base;
	vm_offset_t dirbase = ptetokv(pdpbase[0]);
#else
	vm_offset_t dirbase = boot_info.pt_base;
#endif
	struct pmap linear_pmap = {
		.dirbase = (void*) dirbase,
	};
	/* Modify our future kernel map (can't use update_va_mapping for this)... */
	if (*pmap_pde(kernel_pmap, vaddr) & INTEL_PTE_VALID)
		if (!hyp_mmu_update_la (kvtolin(vaddr), pa_to_pte (kv_to_ma(vaddr)) | INTEL_PTE_VALID))
			panic("couldn't set hiMMU readonly for vaddr %p(%p)\n", vaddr, (vm_offset_t) kv_to_ma (vaddr));
	/* ... and the bootstrap map.  */
	if (*pmap_pde(&linear_pmap, vaddr) & INTEL_PTE_VALID)
		if (hyp_do_update_va_mapping (vaddr, pa_to_pte (kv_to_ma(vaddr)) | INTEL_PTE_VALID, UVMF_NONE))
			panic("couldn't set MMU readonly for vaddr %p(%p)\n", vaddr, (vm_offset_t) kv_to_ma (vaddr));
}

void pmap_clear_bootstrap_pagetable(pt_entry_t *base) {
	int i;
	pt_entry_t *dir;
	vm_offset_t va = 0;
#if PAE
	int j;
#endif	/* PAE */
	if (!hyp_mmuext_op_mfn (MMUEXT_UNPIN_TABLE, kv_to_mfn(base)))
		panic("pmap_clear_bootstrap_pagetable: couldn't unpin page %p(%p)\n", base, (vm_offset_t) kv_to_ma(base));
#if PAE
	for (j = 0; j < PDPNUM; j++)
	{
		pt_entry_t pdpe = base[j];
		if (pdpe & INTEL_PTE_VALID) {
			dir = (pt_entry_t *) ptetokv(pdpe);
#else	/* PAE */
			dir = base;
#endif	/* PAE */
			for (i = 0; i < NPTES; i++) {
				pt_entry_t pde = dir[i];
				unsigned long pfn = mfn_to_pfn(atop(pde));
				void *pgt = (void*) phystokv(ptoa(pfn));
				if (pde & INTEL_PTE_VALID)
					hyp_free_page(pfn, pgt);
				va += NPTES * INTEL_PGBYTES;
				if (va >= HYP_VIRT_START)
					break;
			}
#if PAE
			hyp_free_page(atop(_kvtophys(dir)), dir);
		} else
			va += NPTES * NPTES * INTEL_PGBYTES;
		if (va >= HYP_VIRT_START)
			break;
	}
#endif	/* PAE */
	hyp_free_page(atop(_kvtophys(base)), base);
}
#endif	/* MACH_XEN */

void pmap_virtual_space(startp, endp)
	vm_offset_t *startp;
	vm_offset_t *endp;
{
	*startp = kernel_virtual_start;
	*endp = kernel_virtual_end;
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void pmap_init()
{
	register long		npages;
	vm_offset_t		addr;
	register vm_size_t	s;
#if	NCPUS > 1
	int			i;
#endif	/* NCPUS > 1 */

	/*
	 *	Allocate memory for the pv_head_table and its lock bits,
	 *	the modify bit array, and the pte_page table.
	 */

	npages = atop(phys_last_addr - phys_first_addr);
	s = (vm_size_t) (sizeof(struct pv_entry) * npages
				+ pv_lock_table_size(npages)
				+ npages);

	s = round_page(s);
	if (kmem_alloc_wired(kernel_map, &addr, s) != KERN_SUCCESS)
		panic("pmap_init");
	memset((char *) addr, 0, s);

	/*
	 *	Allocate the structures first to preserve word-alignment.
	 */
	pv_head_table = (pv_entry_t) addr;
	addr = (vm_offset_t) (pv_head_table + npages);

	pv_lock_table = (char *) addr;
	addr = (vm_offset_t) (pv_lock_table + pv_lock_table_size(npages));

	pmap_phys_attributes = (char *) addr;

	/*
	 *	Create the zone of physical maps,
	 *	and of the physical-to-virtual entries.
	 */
	s = (vm_size_t) sizeof(struct pmap);
	pmap_zone = zinit(s, 0, 400*s, 4096, 0, "pmap"); /* XXX */
	s = (vm_size_t) sizeof(struct pv_entry);
	pv_list_zone = zinit(s, 0, 10000*s, 4096, 0, "pv_list"); /* XXX */

#if	NCPUS > 1
	/*
	 *	Set up the pmap request lists
	 */
	for (i = 0; i < NCPUS; i++) {
	    pmap_update_list_t	up = &cpu_update_list[i];

	    simple_lock_init(&up->lock);
	    up->count = 0;
	}
#endif	/* NCPUS > 1 */

	/*
	 * Indicate that the PMAP module is now fully initialized.
	 */
	pmap_initialized = TRUE;
}

#define valid_page(x) (pmap_initialized && pmap_valid_page(x))

boolean_t pmap_verify_free(phys)
	vm_offset_t	phys;
{
	pv_entry_t	pv_h;
	int		pai;
	int		spl;
	boolean_t	result;

	assert(phys != vm_page_fictitious_addr);
	if (!pmap_initialized)
		return(TRUE);

	if (!pmap_valid_page(phys))
		return(FALSE);

	PMAP_WRITE_LOCK(spl);

	pai = pa_index(phys);
	pv_h = pai_to_pvh(pai);

	result = (pv_h->pmap == PMAP_NULL);
	PMAP_WRITE_UNLOCK(spl);

	return(result);
}

/*
 *	Routine:	pmap_page_table_page_alloc
 *
 *	Allocates a new physical page to be used as a page-table page.
 *
 *	Must be called with the pmap system and the pmap unlocked,
 *	since these must be unlocked to use vm_page_grab.
 */
vm_offset_t
pmap_page_table_page_alloc()
{
	register vm_page_t	m;
	register vm_offset_t	pa;

	check_simple_locks();

	/*
	 *	We cannot allocate the pmap_object in pmap_init,
	 *	because it is called before the zone package is up.
	 *	Allocate it now if it is missing.
	 */
	if (pmap_object == VM_OBJECT_NULL)
	    pmap_object = vm_object_allocate(phys_last_addr - phys_first_addr);

	/*
	 *	Allocate a VM page for the level 2 page table entries.
	 */
	while ((m = vm_page_grab(FALSE)) == VM_PAGE_NULL)
		VM_PAGE_WAIT((void (*)()) 0);

	/*
	 *	Map the page to its physical address so that it
	 *	can be found later.
	 */
	pa = m->phys_addr;
	vm_object_lock(pmap_object);
	vm_page_insert(m, pmap_object, pa);
	vm_page_lock_queues();
	vm_page_wire(m);
	inuse_ptepages_count++;
	vm_page_unlock_queues();
	vm_object_unlock(pmap_object);

	/*
	 *	Zero the page.
	 */
	memset((void *)phystokv(pa), 0, PAGE_SIZE);

	return pa;
}

#ifdef	MACH_XEN
void pmap_map_mfn(void *_addr, unsigned long mfn) {
	vm_offset_t addr = (vm_offset_t) _addr;
	pt_entry_t	*pte, *pdp;
	vm_offset_t	ptp;
	pt_entry_t ma = ((pt_entry_t) mfn) << PAGE_SHIFT;
	if ((pte = pmap_pte(kernel_pmap, addr)) == PT_ENTRY_NULL) {
		ptp = phystokv(pmap_page_table_page_alloc());
		pmap_set_page_readonly((void*) ptp);
		if (!hyp_mmuext_op_mfn (MMUEXT_PIN_L1_TABLE, pa_to_mfn(ptp)))
			panic("couldn't pin page %p(%p)\n",ptp,(vm_offset_t) kv_to_ma(ptp));
		pdp = pmap_pde(kernel_pmap, addr);
		if (!hyp_mmu_update_pte(kv_to_ma(pdp),
			pa_to_pte(kv_to_ma(ptp)) | INTEL_PTE_VALID
					      | INTEL_PTE_USER
					      | INTEL_PTE_WRITE))
			panic("%s:%d could not set pde %p(%p) to %p(%p)\n",__FILE__,__LINE__,kvtophys((vm_offset_t)pdp),(vm_offset_t) kv_to_ma(pdp), ptp, (vm_offset_t) pa_to_ma(ptp));
		pte = pmap_pte(kernel_pmap, addr);
	}
	if (!hyp_mmu_update_pte(kv_to_ma(pte), ma | INTEL_PTE_VALID | INTEL_PTE_WRITE))
		panic("%s:%d could not set pte %p(%p) to %p(%p)\n",__FILE__,__LINE__,pte,(vm_offset_t) kv_to_ma(pte), ma, ma_to_pa(ma));
}
#endif	/* MACH_XEN */

/*
 *	Deallocate a page-table page.
 *	The page-table page must have all mappings removed,
 *	and be removed from its page directory.
 */
void
pmap_page_table_page_dealloc(pa)
	vm_offset_t	pa;
{
	vm_page_t	m;

	vm_object_lock(pmap_object);
	m = vm_page_lookup(pmap_object, pa);
	vm_page_lock_queues();
	vm_page_free(m);
	inuse_ptepages_count--;
	vm_page_unlock_queues();
	vm_object_unlock(pmap_object);
}

/*
 *	Create and return a physical map.
 *
 *	If the size specified for the map
 *	is zero, the map is an actual physical
 *	map, and may be referenced by the
 *	hardware.
 *
 *	If the size specified is non-zero,
 *	the map will be used in software only, and
 *	is bounded by that size.
 */
pmap_t pmap_create(size)
	vm_size_t	size;
{
	register pmap_t			p;
	register pmap_statistics_t	stats;

	/*
	 *	A software use-only map doesn't even need a map.
	 */

	if (size != 0) {
		return(PMAP_NULL);
	}

/*
 *	Allocate a pmap struct from the pmap_zone.  Then allocate
 *	the page descriptor table from the pd_zone.
 */

	p = (pmap_t) zalloc(pmap_zone);
	if (p == PMAP_NULL)
		panic("pmap_create");

	if (kmem_alloc_wired(kernel_map,
			     (vm_offset_t *)&p->dirbase, PDPNUM * INTEL_PGBYTES)
							!= KERN_SUCCESS)
		panic("pmap_create");

	memcpy(p->dirbase, kernel_page_dir, PDPNUM * INTEL_PGBYTES);
#ifdef	MACH_XEN
	{
		int i;
		for (i = 0; i < PDPNUM; i++)
			pmap_set_page_readonly((void*) p->dirbase + i * INTEL_PGBYTES);
	}
#endif	/* MACH_XEN */

#if PAE
	if (kmem_alloc_wired(kernel_map,
			     (vm_offset_t *)&p->pdpbase, INTEL_PGBYTES)
							!= KERN_SUCCESS)
		panic("pmap_create");
	{
		int i;
		for (i = 0; i < PDPNUM; i++)
			WRITE_PTE(&p->pdpbase[i], pa_to_pte(kvtophys((vm_offset_t) p->dirbase + i * INTEL_PGBYTES)) | INTEL_PTE_VALID);
	}
#ifdef	MACH_XEN
	pmap_set_page_readonly(p->pdpbase);
#endif	/* MACH_XEN */
#endif	/* PAE */

	p->ref_count = 1;

	simple_lock_init(&p->lock);
	p->cpus_using = 0;

	/*
	 *	Initialize statistics.
	 */

	stats = &p->stats;
	stats->resident_count = 0;
	stats->wired_count = 0;

	return(p);
}

/*
 *	Retire the given physical map from service.
 *	Should only be called if the map contains
 *	no valid mappings.
 */

void pmap_destroy(p)
	register pmap_t	p;
{
	register pt_entry_t	*pdep;
	register vm_offset_t	pa;
	register int		c, s;
	register vm_page_t	m;

	if (p == PMAP_NULL)
		return;

	SPLVM(s);
	simple_lock(&p->lock);
	c = --p->ref_count;
	simple_unlock(&p->lock);
	SPLX(s);

	if (c != 0) {
	    return;	/* still in use */
	}

	/*
	 *	Free the memory maps, then the
	 *	pmap structure.
	 */
	for (pdep = p->dirbase;
	     pdep < &p->dirbase[lin2pdenum(LINEAR_MIN_KERNEL_ADDRESS)];
	     pdep += ptes_per_vm_page) {
	    if (*pdep & INTEL_PTE_VALID) {
		pa = pte_to_pa(*pdep);
		vm_object_lock(pmap_object);
		m = vm_page_lookup(pmap_object, pa);
		if (m == VM_PAGE_NULL)
		    panic("pmap_destroy: pte page not in object");
		vm_page_lock_queues();
#ifdef	MACH_XEN
		if (!hyp_mmuext_op_mfn (MMUEXT_UNPIN_TABLE, pa_to_mfn(pa)))
		    panic("pmap_destroy: couldn't unpin page %p(%p)\n", pa, (vm_offset_t) kv_to_ma(pa));
		pmap_set_page_readwrite((void*) phystokv(pa));
#endif	/* MACH_XEN */
		vm_page_free(m);
		inuse_ptepages_count--;
		vm_page_unlock_queues();
		vm_object_unlock(pmap_object);
	    }
	}
#ifdef	MACH_XEN
	{
		int i;
		for (i = 0; i < PDPNUM; i++)
			pmap_set_page_readwrite((void*) p->dirbase + i * INTEL_PGBYTES);
	}
#endif	/* MACH_XEN */
	kmem_free(kernel_map, (vm_offset_t)p->dirbase, PDPNUM * INTEL_PGBYTES);
#if PAE
#ifdef	MACH_XEN
	pmap_set_page_readwrite(p->pdpbase);
#endif	/* MACH_XEN */
	kmem_free(kernel_map, (vm_offset_t)p->pdpbase, INTEL_PGBYTES);
#endif	/* PAE */
	zfree(pmap_zone, (vm_offset_t) p);
}

/*
 *	Add a reference to the specified pmap.
 */

void pmap_reference(p)
	register pmap_t	p;
{
	int	s;
	if (p != PMAP_NULL) {
		SPLVM(s);
		simple_lock(&p->lock);
		p->ref_count++;
		simple_unlock(&p->lock);
		SPLX(s);
	}
}

/*
 *	Remove a range of hardware page-table entries.
 *	The entries given are the first (inclusive)
 *	and last (exclusive) entries for the VM pages.
 *	The virtual address is the va for the first pte.
 *
 *	The pmap must be locked.
 *	If the pmap is not the kernel pmap, the range must lie
 *	entirely within one pte-page.  This is NOT checked.
 *	Assumes that the pte-page exists.
 */

/* static */
void pmap_remove_range(pmap, va, spte, epte)
	pmap_t			pmap;
	vm_offset_t		va;
	pt_entry_t		*spte;
	pt_entry_t		*epte;
{
	register pt_entry_t	*cpte;
	int			num_removed, num_unwired;
	int			pai;
	vm_offset_t		pa;
#ifdef	MACH_XEN
	int n, ii = 0;
	struct mmu_update update[HYP_BATCH_MMU_UPDATES];
#endif	/* MACH_XEN */

#if	DEBUG_PTE_PAGE
	if (pmap != kernel_pmap)
		ptep_check(get_pte_page(spte));
#endif	/* DEBUG_PTE_PAGE */
	num_removed = 0;
	num_unwired = 0;

	for (cpte = spte; cpte < epte;
	     cpte += ptes_per_vm_page, va += PAGE_SIZE) {

	    if (*cpte == 0)
		continue;
	    pa = pte_to_pa(*cpte);

	    num_removed++;
	    if (*cpte & INTEL_PTE_WIRED)
		num_unwired++;

	    if (!valid_page(pa)) {

		/*
		 *	Outside range of managed physical memory.
		 *	Just remove the mappings.
		 */
		register int	i = ptes_per_vm_page;
		register pt_entry_t	*lpte = cpte;
		do {
#ifdef	MACH_XEN
		    update[ii].ptr = kv_to_ma(lpte);
		    update[ii].val = 0;
		    ii++;
		    if (ii == HYP_BATCH_MMU_UPDATES) {
			hyp_mmu_update(kvtolin(&update), ii, kvtolin(&n), DOMID_SELF);
			if (n != ii)
				panic("couldn't pmap_remove_range\n");
			ii = 0;
		    }
#else	/* MACH_XEN */
		    *lpte = 0;
#endif	/* MACH_XEN */
		    lpte++;
		} while (--i > 0);
		continue;
	    }

	    pai = pa_index(pa);
	    LOCK_PVH(pai);

	    /*
	     *	Get the modify and reference bits.
	     */
	    {
		register int		i;
		register pt_entry_t	*lpte;

		i = ptes_per_vm_page;
		lpte = cpte;
		do {
		    pmap_phys_attributes[pai] |=
			*lpte & (PHYS_MODIFIED|PHYS_REFERENCED);
#ifdef	MACH_XEN
		    update[ii].ptr = kv_to_ma(lpte);
		    update[ii].val = 0;
		    ii++;
		    if (ii == HYP_BATCH_MMU_UPDATES) {
			hyp_mmu_update(kvtolin(&update), ii, kvtolin(&n), DOMID_SELF);
			if (n != ii)
				panic("couldn't pmap_remove_range\n");
			ii = 0;
		    }
#else	/* MACH_XEN */
		    *lpte = 0;
#endif	/* MACH_XEN */
		    lpte++;
		} while (--i > 0);
	    }

	    /*
	     *	Remove the mapping from the pvlist for
	     *	this physical page.
	     */
	    {
		register pv_entry_t	pv_h, prev, cur;

		pv_h = pai_to_pvh(pai);
		if (pv_h->pmap == PMAP_NULL) {
		    panic("pmap_remove: null pv_list!");
		}
		if (pv_h->va == va && pv_h->pmap == pmap) {
		    /*
		     * Header is the pv_entry.  Copy the next one
		     * to header and free the next one (we cannot
		     * free the header)
		     */
		    cur = pv_h->next;
		    if (cur != PV_ENTRY_NULL) {
			*pv_h = *cur;
			PV_FREE(cur);
		    }
		    else {
			pv_h->pmap = PMAP_NULL;
		    }
		}
		else {
		    cur = pv_h;
		    do {
			prev = cur;
			if ((cur = prev->next) == PV_ENTRY_NULL) {
			    panic("pmap-remove: mapping not in pv_list!");
			}
		    } while (cur->va != va || cur->pmap != pmap);
		    prev->next = cur->next;
		    PV_FREE(cur);
		}
		UNLOCK_PVH(pai);
	    }
	}

#ifdef	MACH_XEN
	if (ii > HYP_BATCH_MMU_UPDATES)
		panic("overflowed array in pmap_remove_range");
	hyp_mmu_update(kvtolin(&update), ii, kvtolin(&n), DOMID_SELF);
	if (n != ii)
		panic("couldn't pmap_remove_range\n");
#endif	/* MACH_XEN */

	/*
	 *	Update the counts
	 */
	pmap->stats.resident_count -= num_removed;
	pmap->stats.wired_count -= num_unwired;
}

/*
 *	Remove the given range of addresses
 *	from the specified map.
 *
 *	It is assumed that the start and end are properly
 *	rounded to the hardware page size.
 */

void pmap_remove(map, s, e)
	pmap_t		map;
	vm_offset_t	s, e;
{
	int			spl;
	register pt_entry_t	*pde;
	register pt_entry_t	*spte, *epte;
	vm_offset_t		l;
	vm_offset_t		_s = s;

	if (map == PMAP_NULL)
		return;

	PMAP_READ_LOCK(map, spl);

	pde = pmap_pde(map, s);
	while (s < e) {
	    l = (s + PDE_MAPPED_SIZE) & ~(PDE_MAPPED_SIZE-1);
	    if (l > e)
		l = e;
	    if (*pde & INTEL_PTE_VALID) {
		spte = (pt_entry_t *)ptetokv(*pde);
		spte = &spte[ptenum(s)];
		epte = &spte[intel_btop(l-s)];
		pmap_remove_range(map, s, spte, epte);
	    }
	    s = l;
	    pde++;
	}
	PMAP_UPDATE_TLBS(map, _s, e);

	PMAP_READ_UNLOCK(map, spl);
}

/*
 *	Routine:	pmap_page_protect
 *
 *	Function:
 *		Lower the permission for all mappings to a given
 *		page.
 */
void pmap_page_protect(phys, prot)
	vm_offset_t	phys;
	vm_prot_t	prot;
{
	pv_entry_t		pv_h, prev;
	register pv_entry_t	pv_e;
	register pt_entry_t	*pte;
	int			pai;
	register pmap_t		pmap;
	int			spl;
	boolean_t		remove;

	assert(phys != vm_page_fictitious_addr);
	if (!valid_page(phys)) {
	    /*
	     *	Not a managed page.
	     */
	    return;
	}

	/*
	 * Determine the new protection.
	 */
	switch (prot) {
	    case VM_PROT_READ:
	    case VM_PROT_READ|VM_PROT_EXECUTE:
		remove = FALSE;
		break;
	    case VM_PROT_ALL:
		return;	/* nothing to do */
	    default:
		remove = TRUE;
		break;
	}

	/*
	 *	Lock the pmap system first, since we will be changing
	 *	several pmaps.
	 */

	PMAP_WRITE_LOCK(spl);

	pai = pa_index(phys);
	pv_h = pai_to_pvh(pai);

	/*
	 * Walk down PV list, changing or removing all mappings.
	 * We do not have to lock the pv_list because we have
	 * the entire pmap system locked.
	 */
	if (pv_h->pmap != PMAP_NULL) {

	    prev = pv_e = pv_h;
	    do {
		register vm_offset_t va;

		pmap = pv_e->pmap;
		/*
		 * Lock the pmap to block pmap_extract and similar routines.
		 */
		simple_lock(&pmap->lock);

		va = pv_e->va;
		pte = pmap_pte(pmap, va);

		/*
		 * Consistency checks.
		 */
		/* assert(*pte & INTEL_PTE_VALID); XXX */
		/* assert(pte_to_phys(*pte) == phys); */

		/*
		 * Remove the mapping if new protection is NONE
		 * or if write-protecting a kernel mapping.
		 */
		if (remove || pmap == kernel_pmap) {
		    /*
		     * Remove the mapping, collecting any modify bits.
		     */
		    if (*pte & INTEL_PTE_WIRED)
			panic("pmap_remove_all removing a wired page");

		    {
			register int	i = ptes_per_vm_page;

			do {
			    pmap_phys_attributes[pai] |=
				*pte & (PHYS_MODIFIED|PHYS_REFERENCED);
#ifdef	MACH_XEN
			    if (!hyp_mmu_update_pte(kv_to_ma(pte++), 0))
			    	panic("%s:%d could not clear pte %p\n",__FILE__,__LINE__,pte-1);
#else	/* MACH_XEN */
			    *pte++ = 0;
#endif	/* MACH_XEN */
			} while (--i > 0);
		    }

		    pmap->stats.resident_count--;

		    /*
		     * Remove the pv_entry.
		     */
		    if (pv_e == pv_h) {
			/*
			 * Fix up head later.
			 */
			pv_h->pmap = PMAP_NULL;
		    }
		    else {
			/*
			 * Delete this entry.
			 */
			prev->next = pv_e->next;
			PV_FREE(pv_e);
		    }
		}
		else {
		    /*
		     * Write-protect.
		     */
		    register int i = ptes_per_vm_page;

		    do {
#ifdef	MACH_XEN
			if (!hyp_mmu_update_pte(kv_to_ma(pte), *pte & ~INTEL_PTE_WRITE))
			    	panic("%s:%d could not enable write on pte %p\n",__FILE__,__LINE__,pte);
#else	/* MACH_XEN */
			*pte &= ~INTEL_PTE_WRITE;
#endif	/* MACH_XEN */
			pte++;
		    } while (--i > 0);

		    /*
		     * Advance prev.
		     */
		    prev = pv_e;
		}
		PMAP_UPDATE_TLBS(pmap, va, va + PAGE_SIZE);

		simple_unlock(&pmap->lock);

	    } while ((pv_e = prev->next) != PV_ENTRY_NULL);

	    /*
	     * If pv_head mapping was removed, fix it up.
	     */
	    if (pv_h->pmap == PMAP_NULL) {
		pv_e = pv_h->next;
		if (pv_e != PV_ENTRY_NULL) {
		    *pv_h = *pv_e;
		    PV_FREE(pv_e);
		}
	    }
	}

	PMAP_WRITE_UNLOCK(spl);
}

/*
 *	Set the physical protection on the
 *	specified range of this map as requested.
 *	Will not increase permissions.
 */
void pmap_protect(map, s, e, prot)
	pmap_t		map;
	vm_offset_t	s, e;
	vm_prot_t	prot;
{
	register pt_entry_t	*pde;
	register pt_entry_t	*spte, *epte;
	vm_offset_t		l;
	int		spl;
	vm_offset_t	_s = s;

	if (map == PMAP_NULL)
		return;

	/*
	 * Determine the new protection.
	 */
	switch (prot) {
	    case VM_PROT_READ:
	    case VM_PROT_READ|VM_PROT_EXECUTE:
		break;
	    case VM_PROT_READ|VM_PROT_WRITE:
	    case VM_PROT_ALL:
		return;	/* nothing to do */
	    default:
		pmap_remove(map, s, e);
		return;
	}

	/*
	 * If write-protecting in the kernel pmap,
	 * remove the mappings; the i386 ignores
	 * the write-permission bit in kernel mode.
	 *
	 * XXX should be #if'd for i386
	 */
	if (map == kernel_pmap) {
	    pmap_remove(map, s, e);
	    return;
	}

	SPLVM(spl);
	simple_lock(&map->lock);

	pde = pmap_pde(map, s);
	while (s < e) {
	    l = (s + PDE_MAPPED_SIZE) & ~(PDE_MAPPED_SIZE-1);
	    if (l > e)
		l = e;
	    if (*pde & INTEL_PTE_VALID) {
		spte = (pt_entry_t *)ptetokv(*pde);
		spte = &spte[ptenum(s)];
		epte = &spte[intel_btop(l-s)];

#ifdef	MACH_XEN
		int n, i = 0;
		struct mmu_update update[HYP_BATCH_MMU_UPDATES];
#endif	/* MACH_XEN */

		while (spte < epte) {
		    if (*spte & INTEL_PTE_VALID) {
#ifdef	MACH_XEN
			update[i].ptr = kv_to_ma(spte);
			update[i].val = *spte & ~INTEL_PTE_WRITE;
			i++;
			if (i == HYP_BATCH_MMU_UPDATES) {
			    hyp_mmu_update(kvtolin(&update), i, kvtolin(&n), DOMID_SELF);
			    if (n != i)
				    panic("couldn't pmap_protect\n");
			    i = 0;
			}
#else	/* MACH_XEN */
			*spte &= ~INTEL_PTE_WRITE;
#endif	/* MACH_XEN */
		    }
		    spte++;
		}
#ifdef	MACH_XEN
		if (i > HYP_BATCH_MMU_UPDATES)
			panic("overflowed array in pmap_protect");
		hyp_mmu_update(kvtolin(&update), i, kvtolin(&n), DOMID_SELF);
		if (n != i)
			panic("couldn't pmap_protect\n");
#endif	/* MACH_XEN */
	    }
	    s = l;
	    pde++;
	}
	PMAP_UPDATE_TLBS(map, _s, e);

	simple_unlock(&map->lock);
	SPLX(spl);
}

/*
 *	Insert the given physical page (p) at
 *	the specified virtual address (v) in the
 *	target physical map with the protection requested.
 *
 *	If specified, the page will be wired down, meaning
 *	that the related pte can not be reclaimed.
 *
 *	NB:  This is the only routine which MAY NOT lazy-evaluate
 *	or lose information.  That is, this routine must actually
 *	insert this page into the given map NOW.
 */
void pmap_enter(pmap, v, pa, prot, wired)
	register pmap_t		pmap;
	vm_offset_t		v;
	register vm_offset_t	pa;
	vm_prot_t		prot;
	boolean_t		wired;
{
	register pt_entry_t	*pte;
	register pv_entry_t	pv_h;
	register int		i, pai;
	pv_entry_t		pv_e;
	pt_entry_t		template;
	int			spl;
	vm_offset_t		old_pa;

	assert(pa != vm_page_fictitious_addr);
if (pmap_debug) printf("pmap(%x, %x)\n", v, pa);
	if (pmap == PMAP_NULL)
		return;

#if !MACH_KDB
	if (pmap == kernel_pmap && (v < kernel_virtual_start || v >= kernel_virtual_end))
	    	panic("pmap_enter(%p, %p) falls in physical memory area!\n", v, pa);
#endif
	if (pmap == kernel_pmap && (prot & VM_PROT_WRITE) == 0
	    && !wired /* hack for io_wire */ ) {
	    /*
	     *	Because the 386 ignores write protection in kernel mode,
	     *	we cannot enter a read-only kernel mapping, and must
	     *	remove an existing mapping if changing it.
	     *
	     *  XXX should be #if'd for i386
	     */
	    PMAP_READ_LOCK(pmap, spl);

	    pte = pmap_pte(pmap, v);
	    if (pte != PT_ENTRY_NULL && *pte != 0) {
		/*
		 *	Invalidate the translation buffer,
		 *	then remove the mapping.
		 */
		pmap_remove_range(pmap, v, pte,
				  pte + ptes_per_vm_page);
		PMAP_UPDATE_TLBS(pmap, v, v + PAGE_SIZE);
	    }
	    PMAP_READ_UNLOCK(pmap, spl);
	    return;
	}

	/*
	 *	Must allocate a new pvlist entry while we're unlocked;
	 *	zalloc may cause pageout (which will lock the pmap system).
	 *	If we determine we need a pvlist entry, we will unlock
	 *	and allocate one.  Then we will retry, throughing away
	 *	the allocated entry later (if we no longer need it).
	 */
	pv_e = PV_ENTRY_NULL;
Retry:
	PMAP_READ_LOCK(pmap, spl);

	/*
	 *	Expand pmap to include this pte.  Assume that
	 *	pmap is always expanded to include enough hardware
	 *	pages to map one VM page.
	 */

	while ((pte = pmap_pte(pmap, v)) == PT_ENTRY_NULL) {
	    /*
	     * Need to allocate a new page-table page.
	     */
	    vm_offset_t	ptp;
	    pt_entry_t	*pdp;
	    int		i;

	    if (pmap == kernel_pmap) {
		/*
		 * Would have to enter the new page-table page in
		 * EVERY pmap.
		 */
		panic("pmap_expand kernel pmap to %#x", v);
	    }

	    /*
	     * Unlock the pmap and allocate a new page-table page.
	     */
	    PMAP_READ_UNLOCK(pmap, spl);

	    ptp = phystokv(pmap_page_table_page_alloc());

	    /*
	     * Re-lock the pmap and check that another thread has
	     * not already allocated the page-table page.  If it
	     * has, discard the new page-table page (and try
	     * again to make sure).
	     */
	    PMAP_READ_LOCK(pmap, spl);

	    if (pmap_pte(pmap, v) != PT_ENTRY_NULL) {
		/*
		 * Oops...
		 */
		PMAP_READ_UNLOCK(pmap, spl);
		pmap_page_table_page_dealloc(kvtophys(ptp));
		PMAP_READ_LOCK(pmap, spl);
		continue;
	    }

	    /*
	     * Enter the new page table page in the page directory.
	     */
	    i = ptes_per_vm_page;
	    /*XX pdp = &pmap->dirbase[pdenum(v) & ~(i-1)];*/
	    pdp = pmap_pde(pmap, v);
	    do {
#ifdef	MACH_XEN
		pmap_set_page_readonly((void *) ptp);
		if (!hyp_mmuext_op_mfn (MMUEXT_PIN_L1_TABLE, kv_to_mfn(ptp)))
			panic("couldn't pin page %p(%p)\n",ptp,(vm_offset_t) kv_to_ma(ptp));
		if (!hyp_mmu_update_pte(pa_to_ma(kvtophys((vm_offset_t)pdp)),
			pa_to_pte(pa_to_ma(kvtophys(ptp))) | INTEL_PTE_VALID
					      | INTEL_PTE_USER
					      | INTEL_PTE_WRITE))
			panic("%s:%d could not set pde %p(%p,%p) to %p(%p,%p) %p\n",__FILE__,__LINE__, pdp, kvtophys((vm_offset_t)pdp), (vm_offset_t) pa_to_ma(kvtophys((vm_offset_t)pdp)), ptp, kvtophys(ptp), (vm_offset_t) pa_to_ma(kvtophys(ptp)), (vm_offset_t) pa_to_pte(kv_to_ma(ptp)));
#else	/* MACH_XEN */
		*pdp = pa_to_pte(ptp) | INTEL_PTE_VALID
				      | INTEL_PTE_USER
				      | INTEL_PTE_WRITE;
#endif	/* MACH_XEN */
		pdp++;
		ptp += INTEL_PGBYTES;
	    } while (--i > 0);

	    /*
	     * Now, get the address of the page-table entry.
	     */
	    continue;
	}

	/*
	 *	Special case if the physical page is already mapped
	 *	at this address.
	 */
	old_pa = pte_to_pa(*pte);
	if (*pte && old_pa == pa) {
	    /*
	     *	May be changing its wired attribute or protection
	     */

	    if (wired && !(*pte & INTEL_PTE_WIRED))
		pmap->stats.wired_count++;
	    else if (!wired && (*pte & INTEL_PTE_WIRED))
		pmap->stats.wired_count--;

	    template = pa_to_pte(pa) | INTEL_PTE_VALID;
	    if (pmap != kernel_pmap)
		template |= INTEL_PTE_USER;
	    if (prot & VM_PROT_WRITE)
		template |= INTEL_PTE_WRITE;
	    if (machine_slot[cpu_number()].cpu_type >= CPU_TYPE_I486
		&& pa >= phys_last_addr)
		template |= INTEL_PTE_NCACHE|INTEL_PTE_WTHRU;
	    if (wired)
		template |= INTEL_PTE_WIRED;
	    i = ptes_per_vm_page;
	    do {
		if (*pte & INTEL_PTE_MOD)
		    template |= INTEL_PTE_MOD;
#ifdef	MACH_XEN
		if (!hyp_mmu_update_pte(kv_to_ma(pte), pa_to_ma(template)))
			panic("%s:%d could not set pte %p to %p\n",__FILE__,__LINE__,pte,template);
#else	/* MACH_XEN */
		WRITE_PTE(pte, template)
#endif	/* MACH_XEN */
		pte++;
		pte_increment_pa(template);
	    } while (--i > 0);
	    PMAP_UPDATE_TLBS(pmap, v, v + PAGE_SIZE);
	}
	else {

	    /*
	     *	Remove old mapping from the PV list if necessary.
	     */
	    if (*pte) {
		/*
		 *	Don't free the pte page if removing last
		 *	mapping - we will immediately replace it.
		 */
		pmap_remove_range(pmap, v, pte,
				  pte + ptes_per_vm_page);
		PMAP_UPDATE_TLBS(pmap, v, v + PAGE_SIZE);
	    }

	    if (valid_page(pa)) {

		/*
		 *	Enter the mapping in the PV list for this
		 *	physical page.
		 */

		pai = pa_index(pa);
		LOCK_PVH(pai);
		pv_h = pai_to_pvh(pai);

		if (pv_h->pmap == PMAP_NULL) {
		    /*
		     *	No mappings yet
		     */
		    pv_h->va = v;
		    pv_h->pmap = pmap;
		    pv_h->next = PV_ENTRY_NULL;
		}
		else {
#if	DEBUG
		    {
			/* check that this mapping is not already there */
			pv_entry_t	e = pv_h;
			while (e != PV_ENTRY_NULL) {
			    if (e->pmap == pmap && e->va == v)
				panic("pmap_enter: already in pv_list");
			    e = e->next;
			}
		    }
#endif	/* DEBUG */

		    /*
		     *	Add new pv_entry after header.
		     */
		    if (pv_e == PV_ENTRY_NULL) {
			PV_ALLOC(pv_e);
			if (pv_e == PV_ENTRY_NULL) {
			    UNLOCK_PVH(pai);
			    PMAP_READ_UNLOCK(pmap, spl);

			    /*
			     * Refill from zone.
			     */
			    pv_e = (pv_entry_t) zalloc(pv_list_zone);
			    goto Retry;
			}
		    }
		    pv_e->va = v;
		    pv_e->pmap = pmap;
		    pv_e->next = pv_h->next;
		    pv_h->next = pv_e;
		    /*
		     *	Remember that we used the pvlist entry.
		     */
		    pv_e = PV_ENTRY_NULL;
		}
		UNLOCK_PVH(pai);
	    }

	    /*
	     *	And count the mapping.
	     */

	    pmap->stats.resident_count++;
	    if (wired)
		pmap->stats.wired_count++;

	    /*
	     *	Build a template to speed up entering -
	     *	only the pfn changes.
	     */
	    template = pa_to_pte(pa) | INTEL_PTE_VALID;
	    if (pmap != kernel_pmap)
		template |= INTEL_PTE_USER;
	    if (prot & VM_PROT_WRITE)
		template |= INTEL_PTE_WRITE;
	    if (machine_slot[cpu_number()].cpu_type >= CPU_TYPE_I486
		&& pa >= phys_last_addr)
		template |= INTEL_PTE_NCACHE|INTEL_PTE_WTHRU;
	    if (wired)
		template |= INTEL_PTE_WIRED;
	    i = ptes_per_vm_page;
	    do {
#ifdef	MACH_XEN
		if (!(hyp_mmu_update_pte(kv_to_ma(pte), pa_to_ma(template))))
			panic("%s:%d could not set pte %p to %p\n",__FILE__,__LINE__,pte,template);
#else	/* MACH_XEN */
		WRITE_PTE(pte, template)
#endif	/* MACH_XEN */
		pte++;
		pte_increment_pa(template);
	    } while (--i > 0);
	}

	if (pv_e != PV_ENTRY_NULL) {
	    PV_FREE(pv_e);
	}

	PMAP_READ_UNLOCK(pmap, spl);
}

/*
 *	Routine:	pmap_change_wiring
 *	Function:	Change the wiring attribute for a map/virtual-address
 *			pair.
 *	In/out conditions:
 *			The mapping must already exist in the pmap.
 */
void pmap_change_wiring(map, v, wired)
	register pmap_t	map;
	vm_offset_t	v;
	boolean_t	wired;
{
	register pt_entry_t	*pte;
	register int		i;
	int			spl;

	/*
	 *	We must grab the pmap system lock because we may
	 *	change a pte_page queue.
	 */
	PMAP_READ_LOCK(map, spl);

	if ((pte = pmap_pte(map, v)) == PT_ENTRY_NULL)
		panic("pmap_change_wiring: pte missing");

	if (wired && !(*pte & INTEL_PTE_WIRED)) {
	    /*
	     *	wiring down mapping
	     */
	    map->stats.wired_count++;
	    i = ptes_per_vm_page;
	    do {
		*pte++ |= INTEL_PTE_WIRED;
	    } while (--i > 0);
	}
	else if (!wired && (*pte & INTEL_PTE_WIRED)) {
	    /*
	     *	unwiring mapping
	     */
	    map->stats.wired_count--;
	    i = ptes_per_vm_page;
	    do {
#ifdef	MACH_XEN
		if (!(hyp_mmu_update_pte(kv_to_ma(pte), *pte & ~INTEL_PTE_WIRED)))
			panic("%s:%d could not wire down pte %p\n",__FILE__,__LINE__,pte);
#else	/* MACH_XEN */
		*pte &= ~INTEL_PTE_WIRED;
#endif	/* MACH_XEN */
		pte++;
	    } while (--i > 0);
	}

	PMAP_READ_UNLOCK(map, spl);
}

/*
 *	Routine:	pmap_extract
 *	Function:
 *		Extract the physical page address associated
 *		with the given map/virtual_address pair.
 */

vm_offset_t pmap_extract(pmap, va)
	register pmap_t	pmap;
	vm_offset_t	va;
{
	register pt_entry_t	*pte;
	register vm_offset_t	pa;
	int			spl;

	SPLVM(spl);
	simple_lock(&pmap->lock);
	if ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL)
	    pa = (vm_offset_t) 0;
	else if (!(*pte & INTEL_PTE_VALID))
	    pa = (vm_offset_t) 0;
	else
	    pa = pte_to_pa(*pte) + (va & INTEL_OFFMASK);
	simple_unlock(&pmap->lock);
	SPLX(spl);
	return(pa);
}

/*
 *	Copy the range specified by src_addr/len
 *	from the source map to the range dst_addr/len
 *	in the destination map.
 *
 *	This routine is only advisory and need not do anything.
 */
#if	0
void pmap_copy(dst_pmap, src_pmap, dst_addr, len, src_addr)
	pmap_t		dst_pmap;
	pmap_t		src_pmap;
	vm_offset_t	dst_addr;
	vm_size_t	len;
	vm_offset_t	src_addr;
{
#ifdef	lint
	dst_pmap++; src_pmap++; dst_addr++; len++; src_addr++;
#endif	/* lint */
}
#endif	/* 0 */

/*
 *	Routine:	pmap_collect
 *	Function:
 *		Garbage collects the physical map system for
 *		pages which are no longer used.
 *		Success need not be guaranteed -- that is, there
 *		may well be pages which are not referenced, but
 *		others may be collected.
 *	Usage:
 *		Called by the pageout daemon when pages are scarce.
 */
void pmap_collect(p)
	pmap_t 		p;
{
	register pt_entry_t	*pdp, *ptp;
	pt_entry_t		*eptp;
	vm_offset_t		pa;
	int			spl, wired;

	if (p == PMAP_NULL)
		return;

	if (p == kernel_pmap)
		return;

	/*
	 *	Garbage collect map.
	 */
	PMAP_READ_LOCK(p, spl);
	for (pdp = p->dirbase;
	     pdp < &p->dirbase[lin2pdenum(LINEAR_MIN_KERNEL_ADDRESS)];
	     pdp += ptes_per_vm_page)
	{
	    if (*pdp & INTEL_PTE_VALID) {

		pa = pte_to_pa(*pdp);
		ptp = (pt_entry_t *)phystokv(pa);
		eptp = ptp + NPTES*ptes_per_vm_page;

		/*
		 * If the pte page has any wired mappings, we cannot
		 * free it.
		 */
		wired = 0;
		{
		    register pt_entry_t *ptep;
		    for (ptep = ptp; ptep < eptp; ptep++) {
			if (*ptep & INTEL_PTE_WIRED) {
			    wired = 1;
			    break;
			}
		    }
		}
		if (!wired) {
		    /*
		     * Remove the virtual addresses mapped by this pte page.
		     */
		    { /*XXX big hack*/
		    vm_offset_t va = pdenum2lin(pdp - p->dirbase);
		    if (p == kernel_pmap)
		    	va = lintokv(va);
		    pmap_remove_range(p,
				va,
				ptp,
				eptp);
		    }

		    /*
		     * Invalidate the page directory pointer.
		     */
		    {
			register int i = ptes_per_vm_page;
			register pt_entry_t *pdep = pdp;
			do {
#ifdef	MACH_XEN
			    unsigned long pte = *pdep;
			    void *ptable = (void*) ptetokv(pte);
			    if (!(hyp_mmu_update_pte(pa_to_ma(kvtophys((vm_offset_t)pdep++)), 0)))
			        panic("%s:%d could not clear pde %p\n",__FILE__,__LINE__,pdep-1);
			    if (!hyp_mmuext_op_mfn (MMUEXT_UNPIN_TABLE, kv_to_mfn(ptable)))
				panic("couldn't unpin page %p(%p)\n", ptable, (vm_offset_t) pa_to_ma(kvtophys((vm_offset_t)ptable)));
			    pmap_set_page_readwrite(ptable);
#else	/* MACH_XEN */
			    *pdep++ = 0;
#endif	/* MACH_XEN */
			} while (--i > 0);
		    }

		    PMAP_READ_UNLOCK(p, spl);

		    /*
		     * And free the pte page itself.
		     */
		    {
			register vm_page_t m;

			vm_object_lock(pmap_object);
			m = vm_page_lookup(pmap_object, pa);
			if (m == VM_PAGE_NULL)
			    panic("pmap_collect: pte page not in object");
			vm_page_lock_queues();
			vm_page_free(m);
			inuse_ptepages_count--;
			vm_page_unlock_queues();
			vm_object_unlock(pmap_object);
		    }

		    PMAP_READ_LOCK(p, spl);
		}
	    }
	}
	PMAP_UPDATE_TLBS(p, VM_MIN_ADDRESS, VM_MAX_ADDRESS);

	PMAP_READ_UNLOCK(p, spl);
	return;

}

/*
 *	Routine:	pmap_activate
 *	Function:
 *		Binds the given physical map to the given
 *		processor, and returns a hardware map description.
 */
#if	0
void pmap_activate(my_pmap, th, my_cpu)
	register pmap_t	my_pmap;
	thread_t	th;
	int		my_cpu;
{
	PMAP_ACTIVATE(my_pmap, th, my_cpu);
}
#endif	/* 0 */

/*
 *	Routine:	pmap_deactivate
 *	Function:
 *		Indicates that the given physical map is no longer
 *		in use on the specified processor.  (This is a macro
 *		in pmap.h)
 */
#if	0
void pmap_deactivate(pmap, th, which_cpu)
	pmap_t		pmap;
	thread_t	th;
	int		which_cpu;
{
#ifdef	lint
	pmap++; th++; which_cpu++;
#endif	/* lint */
	PMAP_DEACTIVATE(pmap, th, which_cpu);
}
#endif	/* 0 */

/*
 *	Routine:	pmap_kernel
 *	Function:
 *		Returns the physical map handle for the kernel.
 */
#if	0
pmap_t pmap_kernel()
{
    	return (kernel_pmap);
}
#endif	/* 0 */

/*
 *	pmap_zero_page zeros the specified (machine independent) page.
 *	See machine/phys.c or machine/phys.s for implementation.
 */
#if	0
pmap_zero_page(phys)
	register vm_offset_t	phys;
{
	register int	i;

	assert(phys != vm_page_fictitious_addr);
	i = PAGE_SIZE / INTEL_PGBYTES;
	phys = intel_pfn(phys);

	while (i--)
		zero_phys(phys++);
}
#endif	/* 0 */

/*
 *	pmap_copy_page copies the specified (machine independent) page.
 *	See machine/phys.c or machine/phys.s for implementation.
 */
#if	0
pmap_copy_page(src, dst)
	vm_offset_t	src, dst;
{
	int	i;

	assert(src != vm_page_fictitious_addr);
	assert(dst != vm_page_fictitious_addr);
	i = PAGE_SIZE / INTEL_PGBYTES;

	while (i--) {
		copy_phys(intel_pfn(src), intel_pfn(dst));
		src += INTEL_PGBYTES;
		dst += INTEL_PGBYTES;
	}
}
#endif	/* 0 */

/*
 *	Routine:	pmap_pageable
 *	Function:
 *		Make the specified pages (by pmap, offset)
 *		pageable (or not) as requested.
 *
 *		A page which is not pageable may not take
 *		a fault; therefore, its page table entry
 *		must remain valid for the duration.
 *
 *		This routine is merely advisory; pmap_enter
 *		will specify that these pages are to be wired
 *		down (or not) as appropriate.
 */
void
pmap_pageable(pmap, start, end, pageable)
	pmap_t		pmap;
	vm_offset_t	start;
	vm_offset_t	end;
	boolean_t	pageable;
{
#ifdef	lint
	pmap++; start++; end++; pageable++;
#endif	/* lint */
}

/*
 *	Clear specified attribute bits.
 */
void
phys_attribute_clear(phys, bits)
	vm_offset_t	phys;
	int		bits;
{
	pv_entry_t		pv_h;
	register pv_entry_t	pv_e;
	register pt_entry_t	*pte;
	int			pai;
	register pmap_t		pmap;
	int			spl;

	assert(phys != vm_page_fictitious_addr);
	if (!valid_page(phys)) {
	    /*
	     *	Not a managed page.
	     */
	    return;
	}

	/*
	 *	Lock the pmap system first, since we will be changing
	 *	several pmaps.
	 */

	PMAP_WRITE_LOCK(spl);

	pai = pa_index(phys);
	pv_h = pai_to_pvh(pai);

	/*
	 * Walk down PV list, clearing all modify or reference bits.
	 * We do not have to lock the pv_list because we have
	 * the entire pmap system locked.
	 */
	if (pv_h->pmap != PMAP_NULL) {
	    /*
	     * There are some mappings.
	     */
	    for (pv_e = pv_h; pv_e != PV_ENTRY_NULL; pv_e = pv_e->next) {
		register vm_offset_t va;

		pmap = pv_e->pmap;
		/*
		 * Lock the pmap to block pmap_extract and similar routines.
		 */
		simple_lock(&pmap->lock);

		va = pv_e->va;
		pte = pmap_pte(pmap, va);

#if	0
		/*
		 * Consistency checks.
		 */
		assert(*pte & INTEL_PTE_VALID);
		/* assert(pte_to_phys(*pte) == phys); */
#endif

		/*
		 * Clear modify or reference bits.
		 */
		{
		    register int	i = ptes_per_vm_page;
		    do {
#ifdef	MACH_XEN
			if (!(hyp_mmu_update_pte(kv_to_ma(pte), *pte & ~bits)))
			    panic("%s:%d could not clear bits %lx from pte %p\n",__FILE__,__LINE__,bits,pte);
#else	/* MACH_XEN */
			*pte &= ~bits;
#endif	/* MACH_XEN */
		    } while (--i > 0);
		}
		PMAP_UPDATE_TLBS(pmap, va, va + PAGE_SIZE);
		simple_unlock(&pmap->lock);
	    }
	}

	pmap_phys_attributes[pai] &= ~bits;

	PMAP_WRITE_UNLOCK(spl);
}

/*
 *	Check specified attribute bits.
 */
boolean_t
phys_attribute_test(phys, bits)
	vm_offset_t	phys;
	int		bits;
{
	pv_entry_t		pv_h;
	register pv_entry_t	pv_e;
	register pt_entry_t	*pte;
	int			pai;
	register pmap_t		pmap;
	int			spl;

	assert(phys != vm_page_fictitious_addr);
	if (!valid_page(phys)) {
	    /*
	     *	Not a managed page.
	     */
	    return (FALSE);
	}

	/*
	 *	Lock the pmap system first, since we will be checking
	 *	several pmaps.
	 */

	PMAP_WRITE_LOCK(spl);

	pai = pa_index(phys);
	pv_h = pai_to_pvh(pai);

	if (pmap_phys_attributes[pai] & bits) {
	    PMAP_WRITE_UNLOCK(spl);
	    return (TRUE);
	}

	/*
	 * Walk down PV list, checking all mappings.
	 * We do not have to lock the pv_list because we have
	 * the entire pmap system locked.
	 */
	if (pv_h->pmap != PMAP_NULL) {
	    /*
	     * There are some mappings.
	     */
	    for (pv_e = pv_h; pv_e != PV_ENTRY_NULL; pv_e = pv_e->next) {

		pmap = pv_e->pmap;
		/*
		 * Lock the pmap to block pmap_extract and similar routines.
		 */
		simple_lock(&pmap->lock);

		{
		    register vm_offset_t va;

		    va = pv_e->va;
		    pte = pmap_pte(pmap, va);

#if	0
		    /*
		     * Consistency checks.
		     */
		    assert(*pte & INTEL_PTE_VALID);
		    /* assert(pte_to_phys(*pte) == phys); */
#endif
		}

		/*
		 * Check modify or reference bits.
		 */
		{
		    register int	i = ptes_per_vm_page;

		    do {
			if (*pte & bits) {
			    simple_unlock(&pmap->lock);
			    PMAP_WRITE_UNLOCK(spl);
			    return (TRUE);
			}
		    } while (--i > 0);
		}
		simple_unlock(&pmap->lock);
	    }
	}
	PMAP_WRITE_UNLOCK(spl);
	return (FALSE);
}

/*
 *	Clear the modify bits on the specified physical page.
 */

void pmap_clear_modify(phys)
	register vm_offset_t	phys;
{
	phys_attribute_clear(phys, PHYS_MODIFIED);
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */

boolean_t pmap_is_modified(phys)
	register vm_offset_t	phys;
{
	return (phys_attribute_test(phys, PHYS_MODIFIED));
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */

void pmap_clear_reference(phys)
	vm_offset_t	phys;
{
	phys_attribute_clear(phys, PHYS_REFERENCED);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */

boolean_t pmap_is_referenced(phys)
	vm_offset_t	phys;
{
	return (phys_attribute_test(phys, PHYS_REFERENCED));
}

#if	NCPUS > 1
/*
*	    TLB Coherence Code (TLB "shootdown" code)
*
* Threads that belong to the same task share the same address space and
* hence share a pmap.  However, they  may run on distinct cpus and thus
* have distinct TLBs that cache page table entries. In order to guarantee
* the TLBs are consistent, whenever a pmap is changed, all threads that
* are active in that pmap must have their TLB updated. To keep track of
* this information, the set of cpus that are currently using a pmap is
* maintained within each pmap structure (cpus_using). Pmap_activate() and
* pmap_deactivate add and remove, respectively, a cpu from this set.
* Since the TLBs are not addressable over the bus, each processor must
* flush its own TLB; a processor that needs to invalidate another TLB
* needs to interrupt the processor that owns that TLB to signal the
* update.
*
* Whenever a pmap is updated, the lock on that pmap is locked, and all
* cpus using the pmap are signaled to invalidate. All threads that need
* to activate a pmap must wait for the lock to clear to await any updates
* in progress before using the pmap. They must ACQUIRE the lock to add
* their cpu to the cpus_using set. An implicit assumption made
* throughout the TLB code is that all kernel code that runs at or higher
* than splvm blocks out update interrupts, and that such code does not
* touch pageable pages.
*
* A shootdown interrupt serves another function besides signaling a
* processor to invalidate. The interrupt routine (pmap_update_interrupt)
* waits for the both the pmap lock (and the kernel pmap lock) to clear,
* preventing user code from making implicit pmap updates while the
* sending processor is performing its update. (This could happen via a
* user data write reference that turns on the modify bit in the page
* table). It must wait for any kernel updates that may have started
* concurrently with a user pmap update because the IPC code
* changes mappings.
* Spinning on the VALUES of the locks is sufficient (rather than
* having to acquire the locks) because any updates that occur subsequent
* to finding the lock unlocked will be signaled via another interrupt.
* (This assumes the interrupt is cleared before the low level interrupt code
* calls pmap_update_interrupt()).
*
* The signaling processor must wait for any implicit updates in progress
* to terminate before continuing with its update. Thus it must wait for an
* acknowledgement of the interrupt from each processor for which such
* references could be made. For maintaining this information, a set
* cpus_active is used. A cpu is in this set if and only if it can
* use a pmap. When pmap_update_interrupt() is entered, a cpu is removed from
* this set; when all such cpus are removed, it is safe to update.
*
* Before attempting to acquire the update lock on a pmap, a cpu (A) must
* be at least at the priority of the interprocessor interrupt
* (splip<=splvm). Otherwise, A could grab a lock and be interrupted by a
* kernel update; it would spin forever in pmap_update_interrupt() trying
* to acquire the user pmap lock it had already acquired. Furthermore A
* must remove itself from cpus_active.  Otherwise, another cpu holding
* the lock (B) could be in the process of sending an update signal to A,
* and thus be waiting for A to remove itself from cpus_active. If A is
* spinning on the lock at priority this will never happen and a deadlock
* will result.
*/

/*
 *	Signal another CPU that it must flush its TLB
 */
void    signal_cpus(use_list, pmap, start, end)
	cpu_set		use_list;
	pmap_t		pmap;
	vm_offset_t	start, end;
{
	register int		which_cpu, j;
	register pmap_update_list_t	update_list_p;

	while ((which_cpu = ffs(use_list)) != 0) {
	    which_cpu -= 1;	/* convert to 0 origin */

	    update_list_p = &cpu_update_list[which_cpu];
	    simple_lock(&update_list_p->lock);

	    j = update_list_p->count;
	    if (j >= UPDATE_LIST_SIZE) {
		/*
		 *	list overflowed.  Change last item to
		 *	indicate overflow.
		 */
		update_list_p->item[UPDATE_LIST_SIZE-1].pmap  = kernel_pmap;
		update_list_p->item[UPDATE_LIST_SIZE-1].start = VM_MIN_ADDRESS;
		update_list_p->item[UPDATE_LIST_SIZE-1].end   = VM_MAX_KERNEL_ADDRESS;
	    }
	    else {
		update_list_p->item[j].pmap  = pmap;
		update_list_p->item[j].start = start;
		update_list_p->item[j].end   = end;
		update_list_p->count = j+1;
	    }
	    cpu_update_needed[which_cpu] = TRUE;
	    simple_unlock(&update_list_p->lock);

	    if ((cpus_idle & (1 << which_cpu)) == 0)
		interrupt_processor(which_cpu);
	    use_list &= ~(1 << which_cpu);
	}
}

void process_pmap_updates(my_pmap)
	register pmap_t		my_pmap;
{
	register int		my_cpu = cpu_number();
	register pmap_update_list_t	update_list_p;
	register int		j;
	register pmap_t		pmap;

	update_list_p = &cpu_update_list[my_cpu];
	simple_lock(&update_list_p->lock);

	for (j = 0; j < update_list_p->count; j++) {
	    pmap = update_list_p->item[j].pmap;
	    if (pmap == my_pmap ||
		pmap == kernel_pmap) {

		INVALIDATE_TLB(pmap,
				update_list_p->item[j].start,
				update_list_p->item[j].end);
	    }
	}
	update_list_p->count = 0;
	cpu_update_needed[my_cpu] = FALSE;
	simple_unlock(&update_list_p->lock);
}

/*
 *	Interrupt routine for TBIA requested from other processor.
 */
void pmap_update_interrupt(void)
{
	register int		my_cpu;
	register pmap_t		my_pmap;
	int			s;

	my_cpu = cpu_number();

	/*
	 *	Exit now if we're idle.  We'll pick up the update request
	 *	when we go active, and we must not put ourselves back in
	 *	the active set because we'll never process the interrupt
	 *	while we're idle (thus hanging the system).
	 */
	if (cpus_idle & (1 << my_cpu))
	    return;

	if (current_thread() == THREAD_NULL)
	    my_pmap = kernel_pmap;
	else {
	    my_pmap = current_pmap();
	    if (!pmap_in_use(my_pmap, my_cpu))
		my_pmap = kernel_pmap;
	}

	/*
	 *	Raise spl to splvm (above splip) to block out pmap_extract
	 *	from IO code (which would put this cpu back in the active
	 *	set).
	 */
	s = splvm();

	do {

	    /*
	     *	Indicate that we're not using either user or kernel
	     *	pmap.
	     */
	    i_bit_clear(my_cpu, &cpus_active);

	    /*
	     *	Wait for any pmap updates in progress, on either user
	     *	or kernel pmap.
	     */
	    while (*(volatile int *)&my_pmap->lock.lock_data ||
		   *(volatile int *)&kernel_pmap->lock.lock_data)
		continue;

	    process_pmap_updates(my_pmap);

	    i_bit_set(my_cpu, &cpus_active);

	} while (cpu_update_needed[my_cpu]);

	splx(s);
}
#else	/* NCPUS > 1 */
/*
 *	Dummy routine to satisfy external reference.
 */
void pmap_update_interrupt()
{
	/* should never be called. */
}
#endif	/* NCPUS > 1 */

#ifdef i386
/* Unmap page 0 to trap NULL references.  */
void
pmap_unmap_page_zero ()
{
  int *pte;

  pte = (int *) pmap_pte (kernel_pmap, 0);
  if (!pte)
    return;
  assert (pte);
#ifdef	MACH_XEN
  if (!hyp_mmu_update_pte(kv_to_ma(pte), 0))
    printf("couldn't unmap page 0\n");
#else	/* MACH_XEN */
  *pte = 0;
  INVALIDATE_TLB(kernel_pmap, 0, PAGE_SIZE);
#endif	/* MACH_XEN */
}
#endif /* i386 */
