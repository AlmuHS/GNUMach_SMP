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
#include <kern/slab.h>

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
#include <i386/spl.h>
#include <i386at/biosmem.h>
#include <i386at/model_dep.h>

#if	NCPUS > 1
#include <i386/mp_desc.h>
#endif

#include <ddb/db_output.h>
#include <machine/db_machdep.h>

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
 *	The list is refilled from the pv_list_cache if it becomes empty.
 */
pv_entry_t	pv_free_list;		/* free list at SPLVM */
def_simple_lock_data(static, pv_free_list_lock)

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

struct kmem_cache	pv_list_cache;		/* cache of pv_entry structures */

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

/*
 *	Index into pv_head table, its lock bits, and the modify/reference
 *	bits.
 */
#define pa_index(pa)	vm_page_table_index(pa)

#define pai_to_pvh(pai)		(&pv_head_table[pai])
#define lock_pvh_pai(pai)	(bit_lock(pai, pv_lock_table))
#define unlock_pvh_pai(pai)	(bit_unlock(pai, pv_lock_table))

/*
 *	Array of physical page attributes for managed pages.
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
		cpu_pause(); \
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

#ifdef	MACH_PV_PAGETABLES
#define INVALIDATE_TLB(pmap, s, e) do { \
	if (__builtin_constant_p((e) - (s)) \
		&& (e) - (s) == PAGE_SIZE) \
		hyp_invlpg((pmap) == kernel_pmap ? kvtolin(s) : (s)); \
	else \
		hyp_mmuext_op_void(MMUEXT_TLB_FLUSH_LOCAL); \
} while(0)
#else	/* MACH_PV_PAGETABLES */
/* It is hard to know when a TLB flush becomes less expensive than a bunch of
 * invlpgs.  But it surely is more expensive than just one invlpg.  */
#define INVALIDATE_TLB(pmap, s, e) do { \
	if (__builtin_constant_p((e) - (s)) \
		&& (e) - (s) == PAGE_SIZE) \
		invlpg_linear((pmap) == kernel_pmap ? kvtolin(s) : (s)); \
	else \
		flush_tlb(); \
} while (0)
#endif	/* MACH_PV_PAGETABLES */


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

cpu_set		cpus_active;
cpu_set		cpus_idle;
volatile
boolean_t	cpu_update_needed[NCPUS];

#endif	/* NCPUS > 1 */

/*
 *	Other useful macros.
 */
#define current_pmap()		(vm_map_pmap(current_thread()->task->map))
#define pmap_in_use(pmap, cpu)	(((pmap)->cpus_using & (1 << (cpu))) != 0)

struct pmap	kernel_pmap_store;
pmap_t		kernel_pmap;

struct kmem_cache pmap_cache;  /* cache of pmap structures */
struct kmem_cache pt_cache;    /* cache of page tables */
struct kmem_cache pd_cache;    /* cache of page directories */
#if PAE
struct kmem_cache pdpt_cache;  /* cache of page directory pointer tables */
#ifdef __x86_64__
struct kmem_cache l4_cache;    /* cache of L4 tables */
#endif /* __x86_64__ */
#endif /* PAE */

boolean_t		pmap_debug = FALSE;	/* flag for debugging prints */

#if 0
int		ptes_per_vm_page;	/* number of hardware ptes needed
					   to map one VM page. */
#else
#define		ptes_per_vm_page	1
#endif

unsigned int	inuse_ptepages_count = 0;	/* debugging */

/*
 * Pointer to the basic page directory for the kernel.
 * Initialized by pmap_bootstrap().
 */
pt_entry_t *kernel_page_dir;

/*
 * Two slots for temporary physical page mapping, to allow for
 * physical-to-physical transfers.
 */
static pmap_mapwindow_t mapwindows[PMAP_NMAPWINDOWS * NCPUS];
#define MAPWINDOW_SIZE (PMAP_NMAPWINDOWS * NCPUS * PAGE_SIZE)

#ifdef __x86_64__
static inline pt_entry_t *
pmap_l4base(const pmap_t pmap, vm_offset_t lin_addr)
{
	return &pmap->l4base[lin2l4num(lin_addr)];
}
#endif

#ifdef PAE
static inline pt_entry_t *
pmap_ptp(const pmap_t pmap, vm_offset_t lin_addr)
{
	pt_entry_t *pdp_table;
#ifdef __x86_64__
	pt_entry_t *l4_table;
	l4_table = pmap_l4base(pmap, lin_addr);
	if (l4_table == PT_ENTRY_NULL)
		return(PT_ENTRY_NULL);
	pt_entry_t pdp = *l4_table;
	if ((pdp & INTEL_PTE_VALID) == 0)
		return PT_ENTRY_NULL;
	pdp_table = (pt_entry_t *) ptetokv(pdp);
#else /* __x86_64__ */
	pdp_table = pmap->pdpbase;
#endif /* __x86_64__ */
	return &pdp_table[lin2pdpnum(lin_addr)];
}
#endif

static inline pt_entry_t *
pmap_pde(const pmap_t pmap, vm_offset_t addr)
{
	pt_entry_t *page_dir;
	if (pmap == kernel_pmap)
		addr = kvtolin(addr);
#if PAE
	pt_entry_t *pdp_table;
	pdp_table = pmap_ptp(pmap, addr);
        if (pdp_table == PT_ENTRY_NULL)
		return(PT_ENTRY_NULL);
	pt_entry_t pde = *pdp_table;
	if ((pde & INTEL_PTE_VALID) == 0)
		return PT_ENTRY_NULL;
	page_dir = (pt_entry_t *) ptetokv(pde);
#else /* PAE */
	page_dir = pmap->dirbase;
#endif /* PAE */
	return &page_dir[lin2pdenum(addr)];
}

/*
 *	Given an offset and a map, compute the address of the
 *	pte.  If the address is invalid with respect to the map
 *	then PT_ENTRY_NULL is returned (and the map may need to grow).
 *
 *	This is only used internally.
 */
pt_entry_t *
pmap_pte(const pmap_t pmap, vm_offset_t addr)
{
	pt_entry_t	*ptp;
	pt_entry_t	pte;

#ifdef __x86_64__
	if (pmap->l4base == 0)
		return(PT_ENTRY_NULL);
#elif PAE
	if (pmap->pdpbase == 0)
		return(PT_ENTRY_NULL);
#else
	if (pmap->dirbase == 0)
		return(PT_ENTRY_NULL);
#endif
	ptp = pmap_pde(pmap, addr);
	if (ptp == 0)
		return(PT_ENTRY_NULL);
	pte = *ptp;
	if ((pte & INTEL_PTE_VALID) == 0)
		return(PT_ENTRY_NULL);
	ptp = (pt_entry_t *)ptetokv(pte);
	return(&ptp[ptenum(addr)]);
}

#define DEBUG_PTE_PAGE	0

#if	DEBUG_PTE_PAGE
void ptep_check(ptep_t ptep)
{
	pt_entry_t		*pte, *epte;
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
 *	Back-door routine for mapping kernel VM at initialization.
 * 	Useful for mapping memory outside the range of direct mapped
 *	physical memory (i.e., devices).
 */
vm_offset_t pmap_map_bd(
	vm_offset_t	virt,
	phys_addr_t	start,
	phys_addr_t	end,
	vm_prot_t	prot)
{
	pt_entry_t	template;
	pt_entry_t	*pte;
	int		spl;
#ifdef	MACH_PV_PAGETABLES
	int n, i = 0;
	struct mmu_update update[HYP_BATCH_MMU_UPDATES];
#endif	/* MACH_PV_PAGETABLES */

	template = pa_to_pte(start)
		| INTEL_PTE_NCACHE|INTEL_PTE_WTHRU
		| INTEL_PTE_VALID;
	if (CPU_HAS_FEATURE(CPU_FEATURE_PGE))
		template |= INTEL_PTE_GLOBAL;
	if (prot & VM_PROT_WRITE)
	    template |= INTEL_PTE_WRITE;

	PMAP_READ_LOCK(kernel_pmap, spl);
	while (start < end) {
		pte = pmap_pte(kernel_pmap, virt);
		if (pte == PT_ENTRY_NULL)
			panic("pmap_map_bd: Invalid kernel address\n");
#ifdef	MACH_PV_PAGETABLES
		update[i].ptr = kv_to_ma(pte);
		update[i].val = pa_to_ma(template);
		i++;
		if (i == HYP_BATCH_MMU_UPDATES) {
			hyp_mmu_update(kvtolin(&update), i, kvtolin(&n), DOMID_SELF);
			if (n != i)
				panic("couldn't pmap_map_bd\n");
			i = 0;
		}
#else	/* MACH_PV_PAGETABLES */
		WRITE_PTE(pte, template)
#endif	/* MACH_PV_PAGETABLES */
		pte_increment_pa(template);
		virt += PAGE_SIZE;
		start += PAGE_SIZE;
	}
#ifdef	MACH_PV_PAGETABLES
	if (i > HYP_BATCH_MMU_UPDATES)
		panic("overflowed array in pmap_map_bd");
	hyp_mmu_update(kvtolin(&update), i, kvtolin(&n), DOMID_SELF);
	if (n != i)
		panic("couldn't pmap_map_bd\n");
#endif	/* MACH_PV_PAGETABLES */
	PMAP_READ_UNLOCK(kernel_pmap, spl);
	return(virt);
}

#ifdef PAE
static void pmap_bootstrap_pae(void)
{
	vm_offset_t addr;
	pt_entry_t *pdp_kernel;

#ifdef __x86_64__
#ifdef MACH_HYP
	kernel_pmap->user_l4base = NULL;
	kernel_pmap->user_pdpbase = NULL;
#endif
	kernel_pmap->l4base = (pt_entry_t*)phystokv(pmap_grab_page());
	memset(kernel_pmap->l4base, 0, INTEL_PGBYTES);
#else
	const int PDPNUM_KERNEL = PDPNUM;
#endif	/* x86_64 */

	init_alloc_aligned(PDPNUM_KERNEL * INTEL_PGBYTES, &addr);
	kernel_page_dir = (pt_entry_t*)phystokv(addr);
	memset(kernel_page_dir, 0, PDPNUM_KERNEL * INTEL_PGBYTES);

	pdp_kernel = (pt_entry_t*)phystokv(pmap_grab_page());
	memset(pdp_kernel, 0, INTEL_PGBYTES);
	for (int i = 0; i < PDPNUM_KERNEL; i++) {
		int pdp_index = i;
#ifdef __x86_64__
		pdp_index += lin2pdpnum(VM_MIN_KERNEL_ADDRESS);
#endif
		WRITE_PTE(&pdp_kernel[pdp_index],
			  pa_to_pte(_kvtophys((void *) kernel_page_dir
					      + i * INTEL_PGBYTES))
			  | INTEL_PTE_VALID
#if (defined(__x86_64__) && !defined(MACH_HYP)) || defined(MACH_PV_PAGETABLES)
			  | INTEL_PTE_WRITE
#endif
			);
	}

#ifdef __x86_64__
        /* only fill the kernel pdpte during bootstrap */
	WRITE_PTE(&kernel_pmap->l4base[lin2l4num(VM_MIN_KERNEL_ADDRESS)],
                  pa_to_pte(_kvtophys(pdp_kernel)) | INTEL_PTE_VALID | INTEL_PTE_WRITE);
#ifdef	MACH_PV_PAGETABLES
	pmap_set_page_readonly_init(kernel_pmap->l4base);
#endif /* MACH_PV_PAGETABLES */
#else	/* x86_64 */
        kernel_pmap->pdpbase = pdp_kernel;
#endif	/* x86_64 */
}
#endif /* PAE */

#ifdef	MACH_PV_PAGETABLES
#ifdef PAE
#define NSUP_L1 4
#else
#define NSUP_L1 1
#endif
static void pmap_bootstrap_xen(pt_entry_t *l1_map[NSUP_L1])
{
	/* We don't actually deal with the CR3 register content at all */
	hyp_vm_assist(VMASST_CMD_enable, VMASST_TYPE_pae_extended_cr3);
	/*
	 * Xen may only provide as few as 512KB extra bootstrap linear memory,
	 * which is far from enough to map all available memory, so we need to
	 * map more bootstrap linear memory. We here map 1 (resp. 4 for PAE)
	 * other L1 table(s), thus 4MiB extra memory (resp. 8MiB), which is
	 * enough for a pagetable mapping 4GiB.
	 */
	vm_offset_t la;
	int n_l1map;
	for (n_l1map = 0, la = VM_MIN_KERNEL_ADDRESS; la >= VM_MIN_KERNEL_ADDRESS; la += NPTES * PAGE_SIZE) {
		pt_entry_t *base = (pt_entry_t*) boot_info.pt_base;
#ifdef	PAE
#ifdef __x86_64__
		base = (pt_entry_t*) ptetokv(base[0]);
#endif /* x86_64 */
		pt_entry_t *l2_map = (pt_entry_t*) ptetokv(base[lin2pdpnum(la)]);
#else	/* PAE */
		pt_entry_t *l2_map = base;
#endif	/* PAE */
		/* Like lin2pdenum, but works with non-contiguous boot L3 */
		l2_map += (la >> PDESHIFT) & PDEMASK;
		if (!(*l2_map & INTEL_PTE_VALID)) {
			struct mmu_update update;
			unsigned j, n;

			l1_map[n_l1map] = (pt_entry_t*) phystokv(pmap_grab_page());
			for (j = 0; j < NPTES; j++)
				l1_map[n_l1map][j] = (((pt_entry_t)pfn_to_mfn(lin2pdenum(la - VM_MIN_KERNEL_ADDRESS) * NPTES + j)) << PAGE_SHIFT) | INTEL_PTE_VALID | INTEL_PTE_WRITE;
			pmap_set_page_readonly_init(l1_map[n_l1map]);
			if (!hyp_mmuext_op_mfn (MMUEXT_PIN_L1_TABLE, kv_to_mfn (l1_map[n_l1map])))
				panic("couldn't pin page %p(%lx)", l1_map[n_l1map], (vm_offset_t) kv_to_ma (l1_map[n_l1map]));
			update.ptr = kv_to_ma(l2_map);
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
#endif	/* MACH_PV_PAGETABLES */

/*
 *	Bootstrap the system enough to run with virtual memory.
 *	Allocate the kernel page directory and page tables,
 *	and direct-map all physical memory.
 *	Called with mapping off.
 */
void pmap_bootstrap(void)
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
	kernel_virtual_start = phystokv(biosmem_directmap_end());
	kernel_virtual_end = kernel_virtual_start + VM_KERNEL_MAP_SIZE;

	if (kernel_virtual_end < kernel_virtual_start
			|| kernel_virtual_end > VM_MAX_KERNEL_ADDRESS - PAGE_SIZE)
		kernel_virtual_end = VM_MAX_KERNEL_ADDRESS - PAGE_SIZE;

	/*
	 * Allocate and clear a kernel page directory.
	 */
	/* Note: initial Xen mapping holds at least 512kB free mapped page.
	 * We use that for directly building our linear mapping. */
#if PAE
	pmap_bootstrap_pae();
#else	/* PAE */
	kernel_pmap->dirbase = kernel_page_dir = (pt_entry_t*)phystokv(pmap_grab_page());
	{
		unsigned i;
		for (i = 0; i < NPDES; i++)
			kernel_page_dir[i] = 0;
	}
#endif	/* PAE */

#ifdef	MACH_PV_PAGETABLES
	pt_entry_t *l1_map[NSUP_L1];
	pmap_bootstrap_xen(l1_map);
#endif	/* MACH_PV_PAGETABLES */

	/*
	 * Allocate and set up the kernel page tables.
	 */
	{
		vm_offset_t va;
		pt_entry_t global = CPU_HAS_FEATURE(CPU_FEATURE_PGE) ? INTEL_PTE_GLOBAL : 0;

		/*
		 * Map virtual memory for all directly mappable physical memory, 1-1,
		 * Make any mappings completely in the kernel's text segment read-only.
		 *
		 * Also allocate some additional all-null page tables afterwards
		 * for kernel virtual memory allocation,
		 * because this PMAP module is too stupid
		 * to allocate new kernel page tables later.
		 * XX fix this
		 */
		for (va = phystokv(0); va >= phystokv(0) && va < kernel_virtual_end; )
		{
			pt_entry_t *pde = kernel_page_dir + lin2pdenum_cont(kvtolin(va));
			pt_entry_t *ptable = (pt_entry_t*)phystokv(pmap_grab_page());
			pt_entry_t *pte;

			/* Initialize the page directory entry.  */
			WRITE_PTE(pde, pa_to_pte((vm_offset_t)_kvtophys(ptable))
				| INTEL_PTE_VALID | INTEL_PTE_WRITE);

			/* Initialize the page table.  */
			for (pte = ptable; (va < phystokv(biosmem_directmap_end())) && (pte < ptable+NPTES); pte++)
			{
				if ((pte - ptable) < ptenum(va))
				{
					WRITE_PTE(pte, 0);
				}
				else
#ifdef	MACH_PV_PAGETABLES
				if (va == (vm_offset_t) &hyp_shared_info)
				{
					*pte = boot_info.shared_info | INTEL_PTE_VALID | INTEL_PTE_WRITE;
					va += INTEL_PGBYTES;
				}
				else
#endif	/* MACH_PV_PAGETABLES */
				{
					extern char _start[], etext[];

					if (((va >= (vm_offset_t) _start)
					    && (va + INTEL_PGBYTES <= (vm_offset_t)etext))
#ifdef	MACH_PV_PAGETABLES
					    || (va >= (vm_offset_t) boot_info.pt_base
					    && (va + INTEL_PGBYTES <=
					    (vm_offset_t) ptable + INTEL_PGBYTES))
#endif	/* MACH_PV_PAGETABLES */
					    )
					{
						WRITE_PTE(pte, pa_to_pte(_kvtophys(va))
							| INTEL_PTE_VALID | global);
					}
					else
					{
#ifdef	MACH_PV_PAGETABLES
						/* Keep supplementary L1 pages read-only */
						int i;
						for (i = 0; i < NSUP_L1; i++)
							if (va == (vm_offset_t) l1_map[i]) {
								WRITE_PTE(pte, pa_to_pte(_kvtophys(va))
									| INTEL_PTE_VALID | global);
								break;
							}
						if (i == NSUP_L1)
#endif	/* MACH_PV_PAGETABLES */
							WRITE_PTE(pte, pa_to_pte(_kvtophys(va))
								| INTEL_PTE_VALID | INTEL_PTE_WRITE | global)

					}
					va += INTEL_PGBYTES;
				}
			}
			for (; pte < ptable+NPTES; pte++)
			{
				if (va >= kernel_virtual_end - MAPWINDOW_SIZE && va < kernel_virtual_end)
				{
					pmap_mapwindow_t *win = &mapwindows[atop(va - (kernel_virtual_end - MAPWINDOW_SIZE))];
					win->entry = pte;
					win->vaddr = va;
				}
				WRITE_PTE(pte, 0);
				va += INTEL_PGBYTES;
			}
#ifdef	MACH_PV_PAGETABLES
			pmap_set_page_readonly_init(ptable);
			if (!hyp_mmuext_op_mfn (MMUEXT_PIN_L1_TABLE, kv_to_mfn (ptable)))
				panic("couldn't pin page %p(%lx)\n", ptable, (vm_offset_t) kv_to_ma (ptable));
#endif	/* MACH_PV_PAGETABLES */
		}
	}

	/* Architecture-specific code will turn on paging
	   soon after we return from here.  */
}

#ifdef	MACH_PV_PAGETABLES
/* These are only required because of Xen security policies */

/* Set back a page read write */
void pmap_set_page_readwrite(void *_vaddr) {
	vm_offset_t vaddr = (vm_offset_t) _vaddr;
	phys_addr_t paddr = kvtophys(vaddr);
	vm_offset_t canon_vaddr = phystokv(paddr);
	if (hyp_do_update_va_mapping (kvtolin(vaddr), pa_to_pte (pa_to_ma(paddr)) | INTEL_PTE_VALID | INTEL_PTE_WRITE, UVMF_NONE))
		panic("couldn't set hiMMU readwrite for addr %lx(%lx)\n", vaddr, (vm_offset_t) pa_to_ma (paddr));
	if (canon_vaddr != vaddr)
		if (hyp_do_update_va_mapping (kvtolin(canon_vaddr), pa_to_pte (pa_to_ma(paddr)) | INTEL_PTE_VALID | INTEL_PTE_WRITE, UVMF_NONE))
			panic("couldn't set hiMMU readwrite for paddr %lx(%lx)\n", canon_vaddr, (vm_offset_t) pa_to_ma (paddr));
}

/* Set a page read only (so as to pin it for instance) */
void pmap_set_page_readonly(void *_vaddr) {
	vm_offset_t vaddr = (vm_offset_t) _vaddr;
	phys_addr_t paddr = kvtophys(vaddr);
	vm_offset_t canon_vaddr = phystokv(paddr);
	if (*pmap_pde(kernel_pmap, vaddr) & INTEL_PTE_VALID) {
		if (hyp_do_update_va_mapping (kvtolin(vaddr), pa_to_pte (pa_to_ma(paddr)) | INTEL_PTE_VALID, UVMF_NONE))
			panic("couldn't set hiMMU readonly for vaddr %lx(%lx)\n", vaddr, (vm_offset_t) pa_to_ma (paddr));
	}
	if (canon_vaddr != vaddr &&
		*pmap_pde(kernel_pmap, canon_vaddr) & INTEL_PTE_VALID) {
		if (hyp_do_update_va_mapping (kvtolin(canon_vaddr), pa_to_pte (pa_to_ma(paddr)) | INTEL_PTE_VALID, UVMF_NONE))
			panic("couldn't set hiMMU readonly for vaddr %lx canon_vaddr %lx paddr %lx (%lx)\n", vaddr, canon_vaddr, paddr, (vm_offset_t) pa_to_ma (paddr));
	}
}

/* This needs to be called instead of pmap_set_page_readonly as long as RC3
 * still points to the bootstrap dirbase, to also fix the bootstrap table.  */
void pmap_set_page_readonly_init(void *_vaddr) {
	vm_offset_t vaddr = (vm_offset_t) _vaddr;
#if PAE
	pt_entry_t *pdpbase = (void*) boot_info.pt_base;
#ifdef __x86_64__
	pdpbase = (pt_entry_t *) ptetokv(pdpbase[lin2l4num(vaddr)]);
#endif
	/* The bootstrap table does not necessarily use contiguous pages for the pde tables */
	pt_entry_t *dirbase = (void*) ptetokv(pdpbase[lin2pdpnum(vaddr)]);
#else
	pt_entry_t *dirbase = (void*) boot_info.pt_base;
#endif
	pt_entry_t *pte = &dirbase[lin2pdenum(vaddr) & PTEMASK];
	/* Modify our future kernel map (can't use update_va_mapping for this)... */
	if (*pmap_pde(kernel_pmap, vaddr) & INTEL_PTE_VALID) {
		if (!hyp_mmu_update_la (kvtolin(vaddr), pa_to_pte (kv_to_ma(vaddr)) | INTEL_PTE_VALID))
			panic("couldn't set hiMMU readonly for vaddr %lx(%lx)\n", vaddr, (vm_offset_t) kv_to_ma (vaddr));
	}
	/* ... and the bootstrap map.  */
	if (*pte & INTEL_PTE_VALID) {
		if (hyp_do_update_va_mapping (vaddr, pa_to_pte (kv_to_ma(vaddr)) | INTEL_PTE_VALID, UVMF_NONE))
			panic("couldn't set MMU readonly for vaddr %lx(%lx)\n", vaddr, (vm_offset_t) kv_to_ma (vaddr));
	}
}

void pmap_clear_bootstrap_pagetable(pt_entry_t *base) {
	unsigned i;
	pt_entry_t *dir;
	vm_offset_t va = 0;
#ifdef __x86_64__
	int l4i, l3i;
#else
#if PAE
	unsigned j;
#endif	/* PAE */
#endif
	if (!hyp_mmuext_op_mfn (MMUEXT_UNPIN_TABLE, kv_to_mfn(base)))
		panic("pmap_clear_bootstrap_pagetable: couldn't unpin page %p(%lx)\n", base, (vm_offset_t) kv_to_ma(base));
#ifdef __x86_64__
	/* 4-level page table */
	for (l4i = 0; l4i < NPTES && va < HYP_VIRT_START && va < 0x0000800000000000UL; l4i++) {
		pt_entry_t l4e = base[l4i];
		pt_entry_t *l3;
		if (!(l4e & INTEL_PTE_VALID)) {
			va += NPTES * NPTES * NPTES * INTEL_PGBYTES;
			continue;
		}
		l3 = (pt_entry_t *) ptetokv(l4e);

		for (l3i = 0; l3i < NPTES && va < HYP_VIRT_START; l3i++) {
			pt_entry_t l3e = l3[l3i];
			if (!(l3e & INTEL_PTE_VALID)) {
				va += NPTES * NPTES * INTEL_PGBYTES;
				continue;
			}
			dir = (pt_entry_t *) ptetokv(l3e);
#else
#if PAE
	/* 3-level page table */
	for (j = 0; j < PDPNUM && va < HYP_VIRT_START; j++)
	{
			pt_entry_t pdpe = base[j];
			if (!(pdpe & INTEL_PTE_VALID)) {
				va += NPTES * NPTES * INTEL_PGBYTES;
				continue;
			}
			dir = (pt_entry_t *) ptetokv(pdpe);
#else	/* PAE */
			/* 2-level page table */
			dir = base;
#endif	/* PAE */
#endif
			for (i = 0; i < NPTES && va < HYP_VIRT_START; i++) {
				pt_entry_t pde = dir[i];
				unsigned long pfn = atop(pte_to_pa(pde));
				void *pgt = (void*) phystokv(ptoa(pfn));
				if (pde & INTEL_PTE_VALID)
					hyp_free_page(pfn, pgt);
				va += NPTES * INTEL_PGBYTES;
			}
#ifndef __x86_64__
#if PAE
			hyp_free_page(atop(_kvtophys(dir)), dir);
	}
#endif	/* PAE */
#else
			hyp_free_page(atop(_kvtophys(dir)), dir);
		}
		hyp_free_page(atop(_kvtophys(l3)), l3);
	}
#endif
	hyp_free_page(atop(_kvtophys(base)), base);
}
#endif	/* MACH_PV_PAGETABLES */

/*
 * Create a temporary mapping for a given physical entry
 *
 * This can be used to access physical pages which are not mapped 1:1 by
 * phystokv().
 */
pmap_mapwindow_t *pmap_get_mapwindow(pt_entry_t entry)
{
	pmap_mapwindow_t *map;
	int cpu = cpu_number();

	assert(entry != 0);

	/* Find an empty one.  */
	for (map = &mapwindows[cpu * PMAP_NMAPWINDOWS]; map < &mapwindows[(cpu+1) * PMAP_NMAPWINDOWS]; map++)
		if (!(*map->entry))
			break;
	assert(map < &mapwindows[(cpu+1) * PMAP_NMAPWINDOWS]);

#ifdef MACH_PV_PAGETABLES
	if (!hyp_mmu_update_pte(kv_to_ma(map->entry), pa_to_ma(entry)))
		panic("pmap_get_mapwindow");
#else /* MACH_PV_PAGETABLES */
	WRITE_PTE(map->entry, entry);
#endif /* MACH_PV_PAGETABLES */
	INVALIDATE_TLB(kernel_pmap, map->vaddr, map->vaddr + PAGE_SIZE);
	return map;
}

/*
 * Destroy a temporary mapping for a physical entry
 */
void pmap_put_mapwindow(pmap_mapwindow_t *map)
{
#ifdef MACH_PV_PAGETABLES
	if (!hyp_mmu_update_pte(kv_to_ma(map->entry), 0))
		panic("pmap_put_mapwindow");
#else /* MACH_PV_PAGETABLES */
	WRITE_PTE(map->entry, 0);
#endif /* MACH_PV_PAGETABLES */
	INVALIDATE_TLB(kernel_pmap, map->vaddr, map->vaddr + PAGE_SIZE);
}

void pmap_virtual_space(
	vm_offset_t *startp,
	vm_offset_t *endp)
{
	*startp = kernel_virtual_start;
	*endp = kernel_virtual_end - MAPWINDOW_SIZE;
}

/*
 *	Initialize the pmap module.
 *	Called by vm_init, to initialize any structures that the pmap
 *	system needs to map virtual memory.
 */
void pmap_init(void)
{
	unsigned long		npages;
	vm_offset_t		addr;
	vm_size_t		s;
#if	NCPUS > 1
	int			i;
#endif	/* NCPUS > 1 */

	/*
	 *	Allocate memory for the pv_head_table and its lock bits,
	 *	the modify bit array, and the pte_page table.
	 */

	npages = vm_page_table_size();
	s = (vm_size_t) (sizeof(struct pv_entry) * npages
				+ pv_lock_table_size(npages)
				+ npages);

	s = round_page(s);
	if (kmem_alloc_wired(kernel_map, &addr, s) != KERN_SUCCESS)
		panic("pmap_init");
	memset((void *) addr, 0, s);

	/*
	 *	Allocate the structures first to preserve word-alignment.
	 */
	pv_head_table = (pv_entry_t) addr;
	addr = (vm_offset_t) (pv_head_table + npages);

	pv_lock_table = (char *) addr;
	addr = (vm_offset_t) (pv_lock_table + pv_lock_table_size(npages));

	pmap_phys_attributes = (char *) addr;

	/*
	 *	Create the cache of physical maps,
	 *	and of the physical-to-virtual entries.
	 */
	s = (vm_size_t) sizeof(struct pmap);
	kmem_cache_init(&pmap_cache, "pmap", s, 0, NULL, 0);
	kmem_cache_init(&pt_cache, "pmap_L1",
			INTEL_PGBYTES, INTEL_PGBYTES, NULL,
			KMEM_CACHE_PHYSMEM);
	kmem_cache_init(&pd_cache, "pmap_L2",
			INTEL_PGBYTES, INTEL_PGBYTES, NULL,
			KMEM_CACHE_PHYSMEM);
#if PAE
	kmem_cache_init(&pdpt_cache, "pmap_L3",
			INTEL_PGBYTES, INTEL_PGBYTES, NULL,
			KMEM_CACHE_PHYSMEM);
#ifdef __x86_64__
	kmem_cache_init(&l4_cache, "pmap_L4",
			INTEL_PGBYTES, INTEL_PGBYTES, NULL,
			KMEM_CACHE_PHYSMEM);
#endif /* __x86_64__ */
#endif /* PAE */
	s = (vm_size_t) sizeof(struct pv_entry);
	kmem_cache_init(&pv_list_cache, "pv_entry", s, 0, NULL, 0);

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

static inline boolean_t
valid_page(phys_addr_t addr)
{
	struct vm_page *p;

	if (!pmap_initialized)
		return FALSE;

	p = vm_page_lookup_pa(addr);
	return (p != NULL);
}

/*
 *	Routine:	pmap_page_table_page_alloc
 *
 *	Allocates a new physical page to be used as a page-table page.
 *
 *	Must be called with the pmap system and the pmap unlocked,
 *	since these must be unlocked to use vm_page_grab.
 */
static vm_offset_t
pmap_page_table_page_alloc(void)
{
	vm_page_t	m;
	phys_addr_t	pa;

	check_simple_locks();

	/*
	 *	We cannot allocate the pmap_object in pmap_init,
	 *	because it is called before the cache package is up.
	 *	Allocate it now if it is missing.
	 */
	if (pmap_object == VM_OBJECT_NULL)
	    pmap_object = vm_object_allocate(vm_page_table_size() * PAGE_SIZE);

	/*
	 *	Allocate a VM page for the level 2 page table entries.
	 */
	while ((m = vm_page_grab(VM_PAGE_DIRECTMAP)) == VM_PAGE_NULL)
		VM_PAGE_WAIT((void (*)()) 0);

	/*
	 *	Map the page to its physical address so that it
	 *	can be found later.
	 */
	pa = m->phys_addr;
	assert(pa == (vm_offset_t) pa);
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

	/* Add a ptp if none exist yet for this pte */
	if ((pte = pmap_pte(kernel_pmap, addr)) == PT_ENTRY_NULL) {
		ptp = phystokv(pmap_page_table_page_alloc());
#ifdef	MACH_PV_PAGETABLES
		pmap_set_page_readonly((void*) ptp);
		if (!hyp_mmuext_op_mfn (MMUEXT_PIN_L1_TABLE, pa_to_mfn(ptp)))
			panic("couldn't pin page %lx(%lx)\n",ptp,(vm_offset_t) kv_to_ma(ptp));
#endif	/* MACH_PV_PAGETABLES */
		pdp = pmap_pde(kernel_pmap, addr);

#ifdef	MACH_PV_PAGETABLES
		if (!hyp_mmu_update_pte(kv_to_ma(pdp),
			pa_to_pte(kv_to_ma(ptp)) | INTEL_PTE_VALID
#ifndef __x86_64__
					      | INTEL_PTE_USER
#endif
					      | INTEL_PTE_WRITE))
			panic("%s:%d could not set pde %llx(%lx) to %lx(%lx)\n",__FILE__,__LINE__,kvtophys((vm_offset_t)pdp),(vm_offset_t) kv_to_ma(pdp), ptp, (vm_offset_t) pa_to_ma(ptp));
#else	/* MACH_PV_PAGETABLES */
		*pdp = pa_to_pte(kvtophys(ptp)) | INTEL_PTE_VALID
#ifndef __x86_64__
						| INTEL_PTE_USER
#endif
						| INTEL_PTE_WRITE;
#endif	/* MACH_PV_PAGETABLES */
		pte = pmap_pte(kernel_pmap, addr);
	}

#ifdef	MACH_PV_PAGETABLES
	if (!hyp_mmu_update_pte(kv_to_ma(pte), ma | INTEL_PTE_VALID | INTEL_PTE_WRITE))
		panic("%s:%d could not set pte %p(%lx) to %llx(%llx)\n",__FILE__,__LINE__,pte,(vm_offset_t) kv_to_ma(pte), ma, ma_to_pa(ma));
#else	/* MACH_PV_PAGETABLES */
	/* Note: in this case, mfn is actually a pfn.  */
	WRITE_PTE(pte, ma | INTEL_PTE_VALID | INTEL_PTE_WRITE);
#endif	/* MACH_PV_PAGETABLES */
}
#endif	/* MACH_XEN */

/*
 *	Deallocate a page-table page.
 *	The page-table page must have all mappings removed,
 *	and be removed from its page directory.
 */
static void
pmap_page_table_page_dealloc(vm_offset_t pa)
{
	vm_page_t	m;

	vm_object_lock(pmap_object);
	m = vm_page_lookup(pmap_object, pa);
	vm_page_lock_queues();
#ifdef	MACH_PV_PAGETABLES
        if (!hyp_mmuext_op_mfn (MMUEXT_UNPIN_TABLE, pa_to_mfn(pa)))
                panic("couldn't unpin page %llx(%lx)\n", pa, (vm_offset_t) kv_to_ma(pa));
        pmap_set_page_readwrite((void*) phystokv(pa));
#endif	/* MACH_PV_PAGETABLES */
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
pmap_t pmap_create(vm_size_t size)
{
#ifdef __x86_64__
	// needs to be reworked if we want to dynamically allocate PDPs for kernel
	const int PDPNUM = PDPNUM_KERNEL;
#endif
	pt_entry_t		*page_dir[PDPNUM];
	int			i;
	pmap_t			p;
	pmap_statistics_t	stats;

	/*
	 *	A software use-only map doesn't even need a map.
	 */

	if (size != 0) {
		return(PMAP_NULL);
	}

/*
 *	Allocate a pmap struct from the pmap_cache.  Then allocate
 *	the page descriptor table.
 */

	p = (pmap_t) kmem_cache_alloc(&pmap_cache);
	if (p == PMAP_NULL)
		return PMAP_NULL;

	for (i = 0; i < PDPNUM; i++) {
		page_dir[i] = (pt_entry_t *) kmem_cache_alloc(&pd_cache);
		if (page_dir[i] == NULL) {
			i -= 1;
			while (i >= 0) {
				kmem_cache_free(&pd_cache,
						(vm_address_t) page_dir[i]);
				i -= 1;
			}
			kmem_cache_free(&pmap_cache, (vm_address_t) p);
			return PMAP_NULL;
		}
		memcpy(page_dir[i],
		       (void *) kernel_page_dir + i * INTEL_PGBYTES,
		       INTEL_PGBYTES);
	}

#ifdef LINUX_DEV
#if VM_MIN_KERNEL_ADDRESS != 0
	/* Do not map BIOS in user tasks */
	page_dir
#if PAE
		[lin2pdpnum(LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)]
#else
		[0]
#endif
		[lin2pdenum(LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)]
		= 0;
#endif
#endif /* LINUX_DEV */

#ifdef	MACH_PV_PAGETABLES
	{
		for (i = 0; i < PDPNUM; i++)
			pmap_set_page_readonly((void *) page_dir[i]);
	}
#endif	/* MACH_PV_PAGETABLES */

#if PAE
	pt_entry_t *pdp_kernel = (pt_entry_t *) kmem_cache_alloc(&pdpt_cache);
	if (pdp_kernel == NULL) {
		for (i = 0; i < PDPNUM; i++)
			kmem_cache_free(&pd_cache, (vm_address_t) page_dir[i]);
		kmem_cache_free(&pmap_cache, (vm_address_t) p);
		return PMAP_NULL;
	}

	memset(pdp_kernel, 0, INTEL_PGBYTES);
	{
		for (i = 0; i < PDPNUM; i++) {
			int pdp_index = i;
#ifdef __x86_64__
			pdp_index += lin2pdpnum(VM_MIN_KERNEL_ADDRESS);
#endif
			WRITE_PTE(&pdp_kernel[pdp_index],
				  pa_to_pte(kvtophys((vm_offset_t) page_dir[i]))
				  | INTEL_PTE_VALID
#if (defined(__x86_64__) && !defined(MACH_HYP)) || defined(MACH_PV_PAGETABLES)
				  | INTEL_PTE_WRITE
#endif
				  );
			}
	}
#ifdef __x86_64__
	p->l4base = (pt_entry_t *) kmem_cache_alloc(&l4_cache);
	if (p->l4base == NULL)
		panic("pmap_create");
	memset(p->l4base, 0, INTEL_PGBYTES);
	WRITE_PTE(&p->l4base[lin2l4num(VM_MIN_KERNEL_ADDRESS)],
		  pa_to_pte(kvtophys((vm_offset_t) pdp_kernel)) | INTEL_PTE_VALID | INTEL_PTE_WRITE);
#ifdef	MACH_PV_PAGETABLES
	// FIXME: use kmem_cache_alloc instead
	if (kmem_alloc_wired(kernel_map,
			     (vm_offset_t *)&p->user_pdpbase, INTEL_PGBYTES)
							!= KERN_SUCCESS)
		panic("pmap_create");
	memset(p->user_pdpbase, 0, INTEL_PGBYTES);
	{
		int i;
		for (i = 0; i < lin2pdpnum(VM_MAX_USER_ADDRESS); i++)
			WRITE_PTE(&p->user_pdpbase[i], pa_to_pte(kvtophys((vm_offset_t) page_dir[i])) | INTEL_PTE_VALID | INTEL_PTE_WRITE);
	}
	// FIXME: use kmem_cache_alloc instead
	if (kmem_alloc_wired(kernel_map,
			     (vm_offset_t *)&p->user_l4base, INTEL_PGBYTES)
							!= KERN_SUCCESS)
		panic("pmap_create");
	memset(p->user_l4base, 0, INTEL_PGBYTES);
	WRITE_PTE(&p->user_l4base[0], pa_to_pte(kvtophys((vm_offset_t) p->user_pdpbase)) | INTEL_PTE_VALID | INTEL_PTE_WRITE);
#endif	/* MACH_PV_PAGETABLES */
#else	/* _x86_64 */
	p->pdpbase = pdp_kernel;
#endif	/* _x86_64 */
#ifdef	MACH_PV_PAGETABLES 
#ifdef __x86_64__
	pmap_set_page_readonly(p->l4base);
	pmap_set_page_readonly(p->user_l4base);
	pmap_set_page_readonly(p->user_pdpbase);
#else
	pmap_set_page_readonly(p->pdpbase);
#endif
#endif	/* MACH_PV_PAGETABLES */
#else	/* PAE */
	p->dirbase = page_dir[0];
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

void pmap_destroy(pmap_t p)
{
	int		c, s;

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
         * Free the page table tree.
         */
#if PAE
#ifdef __x86_64__
	for (int l4i = 0; l4i < NPTES; l4i++) {
		pt_entry_t pdp = (pt_entry_t) p->l4base[l4i];
		if (!(pdp & INTEL_PTE_VALID))
			continue;
		pt_entry_t *pdpbase = (pt_entry_t*) ptetokv(pdp);
#else /* __x86_64__ */
		pt_entry_t *pdpbase = p->pdpbase;
#endif /* __x86_64__ */
		for (int l3i = 0; l3i < NPTES; l3i++) {
			pt_entry_t pde = (pt_entry_t) pdpbase[l3i];
			if (!(pde & INTEL_PTE_VALID))
				continue;
			pt_entry_t *pdebase = (pt_entry_t*) ptetokv(pde);
			if (
#ifdef __x86_64__
			    l4i < lin2l4num(VM_MAX_USER_ADDRESS) ||
			    (l4i == lin2l4num(VM_MAX_USER_ADDRESS) && l3i < lin2pdpnum(VM_MAX_USER_ADDRESS))
#else /* __x86_64__ */
			    l3i < lin2pdpnum(VM_MAX_USER_ADDRESS)
#endif /* __x86_64__ */
			    )
			for (int l2i = 0; l2i < NPTES; l2i++)
#else /* PAE */
			pt_entry_t *pdebase = p->dirbase;
			for (int l2i = 0; l2i < lin2pdenum(VM_MAX_USER_ADDRESS); l2i++)
#endif /* PAE */
			{
				pt_entry_t pte = (pt_entry_t) pdebase[l2i];
				if (!(pte & INTEL_PTE_VALID))
					continue;
				kmem_cache_free(&pt_cache, (vm_offset_t)ptetokv(pte));
			}
			kmem_cache_free(&pd_cache, (vm_offset_t)pdebase);
#if PAE
		}
		kmem_cache_free(&pdpt_cache, (vm_offset_t)pdpbase);
#ifdef __x86_64__
	}
	kmem_cache_free(&l4_cache, (vm_offset_t) p->l4base);
#endif /* __x86_64__ */
#endif /* PAE */

        /* Finally, free the pmap itself */
	kmem_cache_free(&pmap_cache, (vm_offset_t) p);
}

/*
 *	Add a reference to the specified pmap.
 */

void pmap_reference(pmap_t p)
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

static
void pmap_remove_range(
	pmap_t			pmap,
	vm_offset_t		va,
	pt_entry_t		*spte,
	pt_entry_t		*epte)
{
	pt_entry_t		*cpte;
	unsigned long		num_removed, num_unwired;
	unsigned long		pai;
	phys_addr_t		pa;
#ifdef	MACH_PV_PAGETABLES
	int n, ii = 0;
	struct mmu_update update[HYP_BATCH_MMU_UPDATES];
#endif	/* MACH_PV_PAGETABLES */

	if (pmap == kernel_pmap && (va < kernel_virtual_start || va + (epte-spte)*PAGE_SIZE > kernel_virtual_end))
		panic("pmap_remove_range(%lx-%lx) falls in physical memory area!\n", (unsigned long) va, (unsigned long) va + (epte-spte)*PAGE_SIZE);

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

	    assert(*cpte & INTEL_PTE_VALID);

	    pa = pte_to_pa(*cpte);

	    num_removed++;
	    if (*cpte & INTEL_PTE_WIRED)
		num_unwired++;

	    if (!valid_page(pa)) {

		/*
		 *	Outside range of managed physical memory.
		 *	Just remove the mappings.
		 */
		int		i = ptes_per_vm_page;
		pt_entry_t	*lpte = cpte;
		do {
#ifdef	MACH_PV_PAGETABLES
		    update[ii].ptr = kv_to_ma(lpte);
		    update[ii].val = 0;
		    ii++;
		    if (ii == HYP_BATCH_MMU_UPDATES) {
			hyp_mmu_update(kvtolin(&update), ii, kvtolin(&n), DOMID_SELF);
			if (n != ii)
				panic("couldn't pmap_remove_range\n");
			ii = 0;
		    }
#else	/* MACH_PV_PAGETABLES */
		    *lpte = 0;
#endif	/* MACH_PV_PAGETABLES */
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
		int		i;
		pt_entry_t	*lpte;

		i = ptes_per_vm_page;
		lpte = cpte;
		do {
		    pmap_phys_attributes[pai] |=
			*lpte & (PHYS_MODIFIED|PHYS_REFERENCED);
#ifdef	MACH_PV_PAGETABLES
		    update[ii].ptr = kv_to_ma(lpte);
		    update[ii].val = 0;
		    ii++;
		    if (ii == HYP_BATCH_MMU_UPDATES) {
			hyp_mmu_update(kvtolin(&update), ii, kvtolin(&n), DOMID_SELF);
			if (n != ii)
				panic("couldn't pmap_remove_range\n");
			ii = 0;
		    }
#else	/* MACH_PV_PAGETABLES */
		    *lpte = 0;
#endif	/* MACH_PV_PAGETABLES */
		    lpte++;
		} while (--i > 0);
	    }

	    /*
	     *	Remove the mapping from the pvlist for
	     *	this physical page.
	     */
	    {
		pv_entry_t	pv_h, prev, cur;

		pv_h = pai_to_pvh(pai);
		if (pv_h->pmap == PMAP_NULL) {
		    panic("pmap_remove: null pv_list for pai %lx at va %lx!", pai, (unsigned long) va);
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

#ifdef	MACH_PV_PAGETABLES
	if (ii > HYP_BATCH_MMU_UPDATES)
		panic("overflowed array in pmap_remove_range");
	hyp_mmu_update(kvtolin(&update), ii, kvtolin(&n), DOMID_SELF);
	if (n != ii)
		panic("couldn't pmap_remove_range\n");
#endif	/* MACH_PV_PAGETABLES */

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

void pmap_remove(
	pmap_t		map,
	vm_offset_t	s, 
	vm_offset_t	e)
{
	int			spl;
	pt_entry_t		*spte, *epte;
	vm_offset_t		l;
	vm_offset_t		_s = s;

	if (map == PMAP_NULL)
		return;

	PMAP_READ_LOCK(map, spl);

	while (s < e) {
	    pt_entry_t *pde = pmap_pde(map, s);

	    l = (s + PDE_MAPPED_SIZE) & ~(PDE_MAPPED_SIZE-1);
	    if (l > e || l < s)
		l = e;
	    if (pde && (*pde & INTEL_PTE_VALID)) {
		spte = (pt_entry_t *)ptetokv(*pde);
		spte = &spte[ptenum(s)];
		epte = &spte[intel_btop(l-s)];
		pmap_remove_range(map, s, spte, epte);
	    }
	    s = l;
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
void pmap_page_protect(
	phys_addr_t	phys,
	vm_prot_t	prot)
{
	pv_entry_t		pv_h, prev;
	pv_entry_t		pv_e;
	pt_entry_t		*pte;
	unsigned long		pai;
	pmap_t			pmap;
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
		vm_offset_t va;

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
		assert(*pte & INTEL_PTE_VALID);
		assert(pte_to_pa(*pte) == phys);

		/*
		 * Remove the mapping if new protection is NONE
		 * or if write-protecting a kernel mapping.
		 */
		if (remove || pmap == kernel_pmap) {
		    /*
		     * Remove the mapping, collecting any modify bits.
		     */

		    if (*pte & INTEL_PTE_WIRED) {
			pmap->stats.wired_count--;
		    }

		    {
			int	i = ptes_per_vm_page;

			do {
			    pmap_phys_attributes[pai] |=
				*pte & (PHYS_MODIFIED|PHYS_REFERENCED);
#ifdef	MACH_PV_PAGETABLES
			    if (!hyp_mmu_update_pte(kv_to_ma(pte++), 0))
			    	panic("%s:%d could not clear pte %p\n",__FILE__,__LINE__,pte-1);
#else	/* MACH_PV_PAGETABLES */
			    *pte++ = 0;
#endif	/* MACH_PV_PAGETABLES */
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
		    int i = ptes_per_vm_page;

		    do {
#ifdef	MACH_PV_PAGETABLES
			if (!hyp_mmu_update_pte(kv_to_ma(pte), *pte & ~INTEL_PTE_WRITE))
			    	panic("%s:%d could not disable write on pte %p\n",__FILE__,__LINE__,pte);
#else	/* MACH_PV_PAGETABLES */
			*pte &= ~INTEL_PTE_WRITE;
#endif	/* MACH_PV_PAGETABLES */
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
void pmap_protect(
	pmap_t		map,
	vm_offset_t	s, 
	vm_offset_t	e,
	vm_prot_t	prot)
{
	pt_entry_t	*spte, *epte;
	vm_offset_t	l;
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

#if !(__i486__ || __i586__ || __i686__)
	/*
	 * If write-protecting in the kernel pmap,
	 * remove the mappings; the i386 ignores
	 * the write-permission bit in kernel mode.
	 */
	if (map == kernel_pmap) {
	    pmap_remove(map, s, e);
	    return;
	}
#endif

	SPLVM(spl);
	simple_lock(&map->lock);

	while (s < e) {
	    pt_entry_t *pde = pde = pmap_pde(map, s);

	    l = (s + PDE_MAPPED_SIZE) & ~(PDE_MAPPED_SIZE-1);
	    if (l > e || l < s)
		l = e;
	    if (pde && (*pde & INTEL_PTE_VALID)) {
		spte = (pt_entry_t *)ptetokv(*pde);
		spte = &spte[ptenum(s)];
		epte = &spte[intel_btop(l-s)];

#ifdef	MACH_PV_PAGETABLES
		int n, i = 0;
		struct mmu_update update[HYP_BATCH_MMU_UPDATES];
#endif	/* MACH_PV_PAGETABLES */

		while (spte < epte) {
		    if (*spte & INTEL_PTE_VALID) {
#ifdef	MACH_PV_PAGETABLES
			update[i].ptr = kv_to_ma(spte);
			update[i].val = *spte & ~INTEL_PTE_WRITE;
			i++;
			if (i == HYP_BATCH_MMU_UPDATES) {
			    hyp_mmu_update(kvtolin(&update), i, kvtolin(&n), DOMID_SELF);
			    if (n != i)
				    panic("couldn't pmap_protect\n");
			    i = 0;
			}
#else	/* MACH_PV_PAGETABLES */
			*spte &= ~INTEL_PTE_WRITE;
#endif	/* MACH_PV_PAGETABLES */
		    }
		    spte++;
		}
#ifdef	MACH_PV_PAGETABLES
		if (i > HYP_BATCH_MMU_UPDATES)
			panic("overflowed array in pmap_protect");
		hyp_mmu_update(kvtolin(&update), i, kvtolin(&n), DOMID_SELF);
		if (n != i)
			panic("couldn't pmap_protect\n");
#endif	/* MACH_PV_PAGETABLES */
	    }
	    s = l;
	}
	PMAP_UPDATE_TLBS(map, _s, e);

	simple_unlock(&map->lock);
	SPLX(spl);
}

typedef	pt_entry_t* (*pmap_level_getter_t)(const pmap_t pmap, vm_offset_t addr);
/*
* Expand one single level of the page table tree
*/
static inline pt_entry_t* pmap_expand_level(pmap_t pmap, vm_offset_t v, int spl,
                                            pmap_level_getter_t pmap_level,
                                            pmap_level_getter_t pmap_level_upper,
                                            int n_per_vm_page,
                                            struct kmem_cache *cache)
{
	pt_entry_t		*pte;

	/*
	 *	Expand pmap to include this pte.  Assume that
	 *	pmap is always expanded to include enough hardware
	 *	pages to map one VM page.
	 */
	while ((pte = pmap_level(pmap, v)) == PT_ENTRY_NULL) {
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
		panic("pmap_expand kernel pmap to %#zx", v);
	    }

	    /*
	     * Unlock the pmap and allocate a new page-table page.
	     */
	    PMAP_READ_UNLOCK(pmap, spl);

	    while (!(ptp = kmem_cache_alloc(cache)))
		VM_PAGE_WAIT((void (*)()) 0);
	    memset((void *)ptp, 0, PAGE_SIZE);

	    /*
	     * Re-lock the pmap and check that another thread has
	     * not already allocated the page-table page.  If it
	     * has, discard the new page-table page (and try
	     * again to make sure).
	     */
	    PMAP_READ_LOCK(pmap, spl);

	    if (pmap_level(pmap, v) != PT_ENTRY_NULL) {
		/*
		 * Oops...
		 */
		PMAP_READ_UNLOCK(pmap, spl);
		kmem_cache_free(cache, ptp);
		PMAP_READ_LOCK(pmap, spl);
		continue;
	    }

	    /*
	     * Enter the new page table page in the page directory.
	     */
	    i = n_per_vm_page;
	    pdp = pmap_level_upper(pmap, v);
	    do {
#ifdef	MACH_PV_PAGETABLES
		pmap_set_page_readonly((void *) ptp);
		if (!hyp_mmuext_op_mfn (MMUEXT_PIN_L1_TABLE, kv_to_mfn(ptp)))
			panic("couldn't pin page %lx(%lx)\n",ptp,(vm_offset_t) kv_to_ma(ptp));
		if (!hyp_mmu_update_pte(pa_to_ma(kvtophys((vm_offset_t)pdp)),
			pa_to_pte(pa_to_ma(kvtophys(ptp))) | INTEL_PTE_VALID
					      | (pmap != kernel_pmap ? INTEL_PTE_USER : 0)
					      | INTEL_PTE_WRITE))
			panic("%s:%d could not set pde %p(%llx,%lx) to %lx(%llx,%lx) %lx\n",__FILE__,__LINE__, pdp, kvtophys((vm_offset_t)pdp), (vm_offset_t) pa_to_ma(kvtophys((vm_offset_t)pdp)), ptp, kvtophys(ptp), (vm_offset_t) pa_to_ma(kvtophys(ptp)), (vm_offset_t) pa_to_pte(kv_to_ma(ptp)));
#else	/* MACH_PV_PAGETABLES */
		*pdp = pa_to_pte(kvtophys(ptp)) | INTEL_PTE_VALID
					        | (pmap != kernel_pmap ? INTEL_PTE_USER : 0)
					        | INTEL_PTE_WRITE;
#endif	/* MACH_PV_PAGETABLES */
		pdp++;	/* Note: This is safe b/c we stay in one page.  */
		ptp += INTEL_PGBYTES;
	    } while (--i > 0);

	    /*
	     * Now, get the address of the page-table entry.
	     */
	    continue;
	}
        return pte;
}

/*
 * Expand, if required, the PMAP to include the virtual address V.
 * PMAP needs to be locked, and it will be still locked on return. It
 * can temporarily unlock the PMAP, during allocation or deallocation
 * of physical pages.
 */
static inline pt_entry_t* pmap_expand(pmap_t pmap, vm_offset_t v, int spl)
{
#ifdef PAE
#ifdef __x86_64__
	pmap_expand_level(pmap, v, spl, pmap_ptp, pmap_l4base, 1, &pdpt_cache);
#endif /* __x86_64__ */
	pmap_expand_level(pmap, v, spl, pmap_pde, pmap_ptp, 1, &pd_cache);
#endif /* PAE */
	return pmap_expand_level(pmap, v, spl, pmap_pte, pmap_pde, ptes_per_vm_page, &pt_cache);
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
void pmap_enter(
	pmap_t			pmap,
	vm_offset_t		v,
	phys_addr_t		pa,
	vm_prot_t		prot,
	boolean_t		wired)
{
	boolean_t		is_physmem;
	pt_entry_t		*pte;
	pv_entry_t		pv_h;
	unsigned long		i, pai;
	pv_entry_t		pv_e;
	pt_entry_t		template;
	int			spl;
	phys_addr_t		old_pa;

	assert(pa != vm_page_fictitious_addr);
	if (pmap_debug) printf("pmap(%zx, %llx)\n", v, (unsigned long long) pa);
	if (pmap == PMAP_NULL)
		return;

	if (pmap == kernel_pmap && (v < kernel_virtual_start || v >= kernel_virtual_end))
		panic("pmap_enter(%lx, %llx) falls in physical memory area!\n", (unsigned long) v, (unsigned long long) pa);
#if !(__i486__ || __i586__ || __i686__)
	if (pmap == kernel_pmap && (prot & VM_PROT_WRITE) == 0
	    && !wired /* hack for io_wire */ ) {
	    /*
	     *	Because the 386 ignores write protection in kernel mode,
	     *	we cannot enter a read-only kernel mapping, and must
	     *	remove an existing mapping if changing it.
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
#endif

	/*
	 *	Must allocate a new pvlist entry while we're unlocked;
	 *	Allocating may cause pageout (which will lock the pmap system).
	 *	If we determine we need a pvlist entry, we will unlock
	 *	and allocate one.  Then we will retry, throughing away
	 *	the allocated entry later (if we no longer need it).
	 */
	pv_e = PV_ENTRY_NULL;
Retry:
	PMAP_READ_LOCK(pmap, spl);

	pte = pmap_expand(pmap, v, spl);

	if (vm_page_ready())
		is_physmem = (vm_page_lookup_pa(pa) != NULL);
	else
		is_physmem = (pa < biosmem_directmap_end());

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
		&& !is_physmem)
		template |= INTEL_PTE_NCACHE|INTEL_PTE_WTHRU;
	    if (wired)
		template |= INTEL_PTE_WIRED;
	    i = ptes_per_vm_page;
	    do {
		if (*pte & INTEL_PTE_MOD)
		    template |= INTEL_PTE_MOD;
#ifdef	MACH_PV_PAGETABLES
		if (!hyp_mmu_update_pte(kv_to_ma(pte), pa_to_ma(template)))
			panic("%s:%d could not set pte %p to %llx\n",__FILE__,__LINE__,pte,template);
#else	/* MACH_PV_PAGETABLES */
		WRITE_PTE(pte, template)
#endif	/* MACH_PV_PAGETABLES */
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
			     * Refill from cache.
			     */
			    pv_e = (pv_entry_t) kmem_cache_alloc(&pv_list_cache);
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
		&& !is_physmem)
		template |= INTEL_PTE_NCACHE|INTEL_PTE_WTHRU;
	    if (wired)
		template |= INTEL_PTE_WIRED;
	    i = ptes_per_vm_page;
	    do {
#ifdef	MACH_PV_PAGETABLES
		if (!(hyp_mmu_update_pte(kv_to_ma(pte), pa_to_ma(template))))
			panic("%s:%d could not set pte %p to %llx\n",__FILE__,__LINE__,pte,template);
#else	/* MACH_PV_PAGETABLES */
		WRITE_PTE(pte, template)
#endif	/* MACH_PV_PAGETABLES */
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
void pmap_change_wiring(
	pmap_t		map,
	vm_offset_t	v,
	boolean_t	wired)
{
	pt_entry_t	*pte;
	int		i;
	int		spl;

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
#ifdef	MACH_PV_PAGETABLES
		if (!(hyp_mmu_update_pte(kv_to_ma(pte), *pte & ~INTEL_PTE_WIRED)))
			panic("%s:%d could not wire down pte %p\n",__FILE__,__LINE__,pte);
#else	/* MACH_PV_PAGETABLES */
		*pte &= ~INTEL_PTE_WIRED;
#endif	/* MACH_PV_PAGETABLES */
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

phys_addr_t pmap_extract(
	pmap_t		pmap,
	vm_offset_t	va)
{
	pt_entry_t	*pte;
	phys_addr_t	pa;
	int		spl;

	SPLVM(spl);
	simple_lock(&pmap->lock);
	if ((pte = pmap_pte(pmap, va)) == PT_ENTRY_NULL)
	    pa = 0;
	else if (!(*pte & INTEL_PTE_VALID))
	    pa = 0;
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
void pmap_copy(
	pmap_t		dst_pmap,
	pmap_t		src_pmap,
	vm_offset_t	dst_addr,
	vm_size_t	len,
	vm_offset_t	src_addr)
{
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
void pmap_collect(pmap_t p)
{
	pt_entry_t	        *ptp;
	pt_entry_t		*eptp;
	phys_addr_t		pa;
	int			spl, wired;

	if (p == PMAP_NULL)
		return;

	if (p == kernel_pmap)
		return;

	/*
	 * Free the page table tree.
	 */
	PMAP_READ_LOCK(p, spl);
#if PAE
#ifdef __x86_64__
	for (int l4i = 0; l4i < lin2l4num(VM_MAX_USER_ADDRESS); l4i++) {
		pt_entry_t pdp = (pt_entry_t) p->l4base[l4i];
		if (!(pdp & INTEL_PTE_VALID))
			continue;
		pt_entry_t *pdpbase = (pt_entry_t*) ptetokv(pdp);
		for (int l3i = 0; l3i < NPTES; l3i++)
#else /* __x86_64__ */
		pt_entry_t *pdpbase = p->pdpbase;
		for (int l3i = 0; l3i < lin2pdpnum(VM_MAX_USER_ADDRESS); l3i++)
#endif /* __x86_64__ */
		{
			pt_entry_t pde = (pt_entry_t ) pdpbase[l3i];
			if (!(pde & INTEL_PTE_VALID))
				continue;
			pt_entry_t *pdebase = (pt_entry_t*) ptetokv(pde);
			for (int l2i = 0; l2i < NPTES; l2i++)
#else /* PAE */
			pt_entry_t *pdebase = p->dirbase;
			for (int l2i = 0; l2i < lin2pdenum(VM_MAX_USER_ADDRESS); l2i++)
#endif /* PAE */
			{
				pt_entry_t pte = (pt_entry_t) pdebase[l2i];
				if (!(pte & INTEL_PTE_VALID))
					continue;

				pa = pte_to_pa(pte);
				ptp = (pt_entry_t *)phystokv(pa);
				eptp = ptp + NPTES*ptes_per_vm_page;

				/*
				 * If the pte page has any wired mappings, we cannot
				 * free it.
				 */
				wired = 0;
				{
				    pt_entry_t *ptep;
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
					vm_offset_t va = pagenum2lin(l4i, l3i, l2i, 0);
					if (p == kernel_pmap)
					    va = lintokv(va);
					pmap_remove_range(p, va, ptp, eptp);
				    }

				    /*
				     * Invalidate the page directory pointer.
				     */
				    {
					int i = ptes_per_vm_page;
					pt_entry_t *pdep = &pdebase[l2i];
					do {
#ifdef	MACH_PV_PAGETABLES
					    unsigned long pte = *pdep;
					    void *ptable = (void*) ptetokv(pte);
					    if (!(hyp_mmu_update_pte(pa_to_ma(kvtophys((vm_offset_t)pdep++)), 0)))
						panic("%s:%d could not clear pde %p\n",__FILE__,__LINE__,pdep-1);
					    if (!hyp_mmuext_op_mfn (MMUEXT_UNPIN_TABLE, kv_to_mfn(ptable)))
						panic("couldn't unpin page %p(%lx)\n", ptable, (vm_offset_t) pa_to_ma(kvtophys((vm_offset_t)ptable)));
					    pmap_set_page_readwrite(ptable);
#else	/* MACH_PV_PAGETABLES */
					    *pdep++ = 0;
#endif	/* MACH_PV_PAGETABLES */
					} while (--i > 0);
				    }

				    PMAP_READ_UNLOCK(p, spl);

				    /*
				     * And free the pte page itself.
				     */
				    kmem_cache_free(&pt_cache, (vm_offset_t)ptetokv(pte));

				    PMAP_READ_LOCK(p, spl);

				}
			}
#if PAE
			// TODO check l2
		}
#ifdef __x86_64__
			// TODO check l3
	}
#endif /* __x86_64__ */
#endif /* PAE */

	PMAP_UPDATE_TLBS(p, VM_MIN_USER_ADDRESS, VM_MAX_USER_ADDRESS);

	PMAP_READ_UNLOCK(p, spl);
	return;

}

#if	MACH_KDB
/*
 *	Routine:	pmap_whatis
 *	Function:
 *		Check whether this address is within a pmap
 *	Usage:
 *		Called from debugger
 */
int pmap_whatis(pmap_t p, vm_offset_t a)
{
	pt_entry_t	        *ptp;
	phys_addr_t		pa;
	int			spl;
	int			ret = 0;

	if (p == PMAP_NULL)
		return 0;

	PMAP_READ_LOCK(p, spl);
#if PAE
#ifdef __x86_64__
	if (a >= (vm_offset_t) p->l4base && a < (vm_offset_t) (&p->l4base[NPTES])) {
		db_printf("L4 for pmap %p\n", p);
		ret = 1;
	}
	for (int l4i = 0; l4i < NPTES; l4i++) {
		pt_entry_t pdp = (pt_entry_t) p->l4base[l4i];
		if (!(pdp & INTEL_PTE_VALID))
			continue;
		pt_entry_t *pdpbase = (pt_entry_t*) ptetokv(pdp);
#else /* __x86_64__ */
		int l4i = 0;
		pt_entry_t *pdpbase = p->pdpbase;
#endif /* __x86_64__ */
		if (a >= (vm_offset_t) pdpbase && a < (vm_offset_t) (&pdpbase[NPTES])) {
			db_printf("PDP %d for pmap %p\n", l4i, p);
			ret = 1;
		}
		for (int l3i = 0; l3i < NPTES; l3i++)
		{
			pt_entry_t pde = (pt_entry_t ) pdpbase[l3i];
			if (!(pde & INTEL_PTE_VALID))
				continue;
			pt_entry_t *pdebase = (pt_entry_t*) ptetokv(pde);
#else /* PAE */
			int l4i = 0, l3i = 0;
			pt_entry_t *pdebase = p->dirbase;
#endif /* PAE */
			if (a >= (vm_offset_t) pdebase && a < (vm_offset_t) (&pdebase[NPTES])) {
				db_printf("PDE %d %d for pmap %p\n", l4i, l3i, p);
				ret = 1;
			}
			for (int l2i = 0; l2i < NPTES; l2i++)
			{
				pt_entry_t pte = (pt_entry_t) pdebase[l2i];
				if (!(pte & INTEL_PTE_VALID))
					continue;

				pa = pte_to_pa(pte);
				ptp = (pt_entry_t *)phystokv(pa);

				if (a >= (vm_offset_t) ptp && a < (vm_offset_t) (&ptp[NPTES*ptes_per_vm_page])) {
					db_printf("PTP %d %d %d for pmap %p\n", l4i, l3i, l2i, p);
					ret = 1;
				}
			}
#if PAE
		}
#ifdef __x86_64__
	}
#endif /* __x86_64__ */
#endif /* PAE */
	PMAP_READ_UNLOCK(p, spl);

	if (p == kernel_pmap) {
		phys_addr_t pa;
		if (DB_VALID_KERN_ADDR(a))
			pa = kvtophys(a);
		else
			pa = pmap_extract(current_task()->map->pmap, a);

		if (valid_page(pa)) {
			unsigned long pai;
			pv_entry_t pv_h;

			pai = pa_index(pa);
			for (pv_h = pai_to_pvh(pai);
				pv_h && pv_h->pmap;
				pv_h = pv_h->next)
				db_printf("pmap %p at %llx\n", pv_h->pmap, pv_h->va);
		}
	}

	return ret;
}
#endif /* MACH_KDB */

/*
 *	Routine:	pmap_activate
 *	Function:
 *		Binds the given physical map to the given
 *		processor, and returns a hardware map description.
 */
#if	0
void pmap_activate(pmap_t my_pmap, thread_t th, int my_cpu)
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
void pmap_deactivate(pmap_t pmap, thread_t th, int which_cpu)
{
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
pmap_zero_page(vm_offset_t phys)
{
	int	i;

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
pmap_copy_page(vm_offset_t src, vm_offset_t dst)
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
pmap_pageable(
	pmap_t		pmap,
	vm_offset_t	start,
	vm_offset_t	end,
	boolean_t	pageable)
{
}

/*
 *	Clear specified attribute bits.
 */
static void
phys_attribute_clear(
	phys_addr_t	phys,
	int		bits)
{
	pv_entry_t		pv_h;
	pv_entry_t		pv_e;
	pt_entry_t		*pte;
	unsigned long		pai;
	pmap_t			pmap;
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
		vm_offset_t va;

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
		assert(*pte & INTEL_PTE_VALID);
		assert(pte_to_pa(*pte) == phys);

		/*
		 * Clear modify or reference bits.
		 */
		{
		    int	i = ptes_per_vm_page;
		    do {
#ifdef	MACH_PV_PAGETABLES
			if (!(hyp_mmu_update_pte(kv_to_ma(pte), *pte & ~bits)))
			    panic("%s:%d could not clear bits %x from pte %p\n",__FILE__,__LINE__,bits,pte);
#else	/* MACH_PV_PAGETABLES */
			*pte &= ~bits;
#endif	/* MACH_PV_PAGETABLES */
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
static boolean_t
phys_attribute_test(
	phys_addr_t	phys,
	int		bits)
{
	pv_entry_t		pv_h;
	pv_entry_t		pv_e;
	pt_entry_t		*pte;
	unsigned long		pai;
	pmap_t			pmap;
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
		    vm_offset_t va;

		    va = pv_e->va;
		    pte = pmap_pte(pmap, va);

		    /*
		     * Consistency checks.
		     */
		    assert(*pte & INTEL_PTE_VALID);
		    assert(pte_to_pa(*pte) == phys);
		}

		/*
		 * Check modify or reference bits.
		 */
		{
		    int	i = ptes_per_vm_page;

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

void pmap_clear_modify(phys_addr_t phys)
{
	phys_attribute_clear(phys, PHYS_MODIFIED);
}

/*
 *	pmap_is_modified:
 *
 *	Return whether or not the specified physical page is modified
 *	by any physical maps.
 */

boolean_t pmap_is_modified(phys_addr_t phys)
{
	return (phys_attribute_test(phys, PHYS_MODIFIED));
}

/*
 *	pmap_clear_reference:
 *
 *	Clear the reference bit on the specified physical page.
 */

void pmap_clear_reference(phys_addr_t phys)
{
	phys_attribute_clear(phys, PHYS_REFERENCED);
}

/*
 *	pmap_is_referenced:
 *
 *	Return whether or not the specified physical page is referenced
 *	by any physical maps.
 */

boolean_t pmap_is_referenced(phys_addr_t phys)
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
void    signal_cpus(
	cpu_set		use_list,
	pmap_t		pmap,
	vm_offset_t	start, 
	vm_offset_t	end)
{
	int			which_cpu, j;
	pmap_update_list_t	update_list_p;

	while ((which_cpu = __builtin_ffs(use_list)) != 0) {
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
		update_list_p->item[UPDATE_LIST_SIZE-1].start = VM_MIN_USER_ADDRESS;
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

	    __sync_synchronize();
	    if (((cpus_idle & (1 << which_cpu)) == 0))
		interrupt_processor(which_cpu);
	    use_list &= ~(1 << which_cpu);
	}
}

/*
 *	This is called at splvm
 */
void process_pmap_updates(pmap_t my_pmap)
{
	int			my_cpu = cpu_number();
	pmap_update_list_t	update_list_p;
	int			j;
	pmap_t			pmap;

	update_list_p = &cpu_update_list[my_cpu];
	assert_splvm();
	simple_lock_nocheck(&update_list_p->lock);

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
	simple_unlock_nocheck(&update_list_p->lock);
}

/*
 *	Interrupt routine for TBIA requested from other processor.
 */
void pmap_update_interrupt(void)
{
	int		my_cpu;
	pmap_t		my_pmap;
	int		s;

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
	    while (my_pmap->lock.lock_data ||
		   kernel_pmap->lock.lock_data)
		cpu_pause();

	    process_pmap_updates(my_pmap);

	    i_bit_set(my_cpu, &cpus_active);

	} while (cpu_update_needed[my_cpu]);

	splx(s);
}
#else	/* NCPUS > 1 */
/*
 *	Dummy routine to satisfy external reference.
 */
void pmap_update_interrupt(void)
{
	/* should never be called. */
}
#endif	/* NCPUS > 1 */

#if defined(__i386__) || defined (__x86_64__)
/* Unmap page 0 to trap NULL references.  */
void
pmap_unmap_page_zero (void)
{
  int *pte;

  printf("Unmapping the zero page.  Some BIOS functions may not be working any more.\n");
  pte = (int *) pmap_pte (kernel_pmap, 0);
  if (!pte)
    return;
  assert (pte);
#ifdef	MACH_PV_PAGETABLES
  if (!hyp_mmu_update_pte(kv_to_ma(pte), 0))
    printf("couldn't unmap page 0\n");
#else	/* MACH_PV_PAGETABLES */
  *pte = 0;
  INVALIDATE_TLB(kernel_pmap, 0, PAGE_SIZE);
#endif	/* MACH_PV_PAGETABLES */
}
#endif /* __i386__ */

void
pmap_make_temporary_mapping(void)
{
	int i;
	/*
	 * We'll have to temporarily install a direct mapping
	 * between physical memory and low linear memory,
	 * until we start using our new kernel segment descriptors.
	 */
#if INIT_VM_MIN_KERNEL_ADDRESS != LINEAR_MIN_KERNEL_ADDRESS
	vm_offset_t delta = INIT_VM_MIN_KERNEL_ADDRESS - LINEAR_MIN_KERNEL_ADDRESS;
	if ((vm_offset_t)(-delta) < delta)
		delta = (vm_offset_t)(-delta);
	int nb_direct = delta >> PDESHIFT;
	for (i = 0; i < nb_direct; i++)
		kernel_page_dir[lin2pdenum_cont(INIT_VM_MIN_KERNEL_ADDRESS) + i] =
			kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS) + i];
#endif

#ifdef LINUX_DEV
	/* We need BIOS memory mapped at 0xc0000 & co for BIOS accesses */
#if VM_MIN_KERNEL_ADDRESS != 0
	kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)] =
		kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS)];
#endif
#endif /* LINUX_DEV */

#ifdef	MACH_PV_PAGETABLES
#ifndef __x86_64__
	const int PDPNUM_KERNEL = PDPNUM;
#endif
	for (i = 0; i < PDPNUM_KERNEL; i++)
		pmap_set_page_readonly_init((void*) kernel_page_dir + i * INTEL_PGBYTES);
#if PAE
#ifndef __x86_64__
	pmap_set_page_readonly_init(kernel_pmap->pdpbase);
#endif
#endif	/* PAE */
#endif	/* MACH_PV_PAGETABLES */

	pmap_set_page_dir();
}

void
pmap_set_page_dir(void)
{
#if PAE
#ifdef __x86_64__
	set_cr3((unsigned long)_kvtophys(kernel_pmap->l4base));
#else
	set_cr3((unsigned long)_kvtophys(kernel_pmap->pdpbase));
#endif
#ifndef	MACH_HYP
	if (!CPU_HAS_FEATURE(CPU_FEATURE_PAE))
		panic("CPU doesn't have support for PAE.");
	set_cr4(get_cr4() | CR4_PAE);
#endif	/* MACH_HYP */
#else
	set_cr3((unsigned long)_kvtophys(kernel_page_dir));
#endif	/* PAE */
}

void
pmap_remove_temporary_mapping(void)
{
#if INIT_VM_MIN_KERNEL_ADDRESS != LINEAR_MIN_KERNEL_ADDRESS
	int i;
	vm_offset_t delta = INIT_VM_MIN_KERNEL_ADDRESS - LINEAR_MIN_KERNEL_ADDRESS;
	if ((vm_offset_t)(-delta) < delta)
		delta = (vm_offset_t)(-delta);
	int nb_direct = delta >> PDESHIFT;
	/* Get rid of the temporary direct mapping and flush it out of the TLB.  */
	for (i = 0 ; i < nb_direct; i++) {
#ifdef	MACH_XEN
#ifdef	MACH_PSEUDO_PHYS
		if (!hyp_mmu_update_pte(kv_to_ma(&kernel_page_dir[lin2pdenum_cont(VM_MIN_KERNEL_ADDRESS) + i]), 0))
#else	/* MACH_PSEUDO_PHYS */
		if (hyp_do_update_va_mapping(VM_MIN_KERNEL_ADDRESS + i * INTEL_PGBYTES, 0, UVMF_INVLPG | UVMF_ALL))
#endif	/* MACH_PSEUDO_PHYS */
			printf("couldn't unmap frame %d\n", i);
#else	/* MACH_XEN */
		kernel_page_dir[lin2pdenum_cont(INIT_VM_MIN_KERNEL_ADDRESS) + i] = 0;
#endif	/* MACH_XEN */
	}
#endif

#ifdef LINUX_DEV
	/* Keep BIOS memory mapped */
#if VM_MIN_KERNEL_ADDRESS != 0
	kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS)] =
		kernel_page_dir[lin2pdenum_cont(LINEAR_MIN_KERNEL_ADDRESS)];
#endif
#endif /* LINUX_DEV */

	/* Not used after boot, better give it back.  */
#ifdef	MACH_XEN
	hyp_free_page(0, (void*) VM_MIN_KERNEL_ADDRESS);
#endif	/* MACH_XEN */

	flush_tlb();
}
