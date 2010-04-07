
/*
 *  Copyright (C) 2006 Samuel Thibault <samuel.thibault@ens-lyon.org>
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef _MACH_XEN_H
#define _MACH_XEN_H
#ifdef	MACH_XEN
#include <sys/types.h>
#include <xen/public/xen.h>
#include <i386/vm_param.h>

extern struct start_info boot_info;

extern volatile struct shared_info hyp_shared_info;

/* Memory translations */

/* pa are physical addresses, from 0 to size of memory */
/* ma are machine addresses, i.e. _real_ hardware adresses */
/* la are linear addresses, i.e. without segmentation */

/* This might also be useful out of Xen */
#if VM_MIN_KERNEL_ADDRESS != LINEAR_MIN_KERNEL_ADDRESS
extern unsigned long la_shift;
#else
#define la_shift LINEAR_MIN_KERNEL_ADDRESS
#endif
#define la_to_pa(a)		((vm_offset_t)(((vm_offset_t)(a)) - la_shift))
#define pa_to_la(a)		((vm_offset_t)(((vm_offset_t)(a)) + la_shift))

#define kv_to_la(a)		pa_to_la(_kvtophys(a))
#define la_to_kv(a)		phystokv(la_to_pa(a))

#ifdef	MACH_PSEUDO_PHYS
#if PAE
#define PFN_LIST MACH2PHYS_VIRT_START_PAE
#else
#define PFN_LIST MACH2PHYS_VIRT_START_NONPAE
#endif
#if VM_MIN_KERNEL_ADDRESS != LINEAR_MIN_KERNEL_ADDRESS
extern unsigned long *pfn_list;
#else
#define pfn_list ((unsigned long *) PFN_LIST)
#endif
#define mfn_to_pfn(n)		(pfn_list[n])

extern unsigned long *mfn_list;
#define pfn_to_mfn(n)		(mfn_list[n])
#else
#define mfn_to_pfn(n)		(n)
#define pfn_to_mfn(n)		(n)
#endif	/* MACH_PSEUDO_PHYS */

#define pa_to_mfn(a)		(pfn_to_mfn(atop(a)))
#ifdef PAE
#define pa_to_ma(a)		({ vm_offset_t __a = (vm_offset_t) (a); (((pt_entry_t) pa_to_mfn(__a)) << PAGE_SHIFT) | (__a & PAGE_MASK); })
#define ma_to_pa(a)		({ pt_entry_t __a = (pt_entry_t) (a); (mfn_to_pfn(__a >> PAGE_SHIFT) << PAGE_SHIFT) | (__a & PAGE_MASK); })
#else
#define pa_to_ma(a)		({ vm_offset_t __a = (vm_offset_t) (a); ptoa(pa_to_mfn(__a)) | (__a & PAGE_MASK); })
#define ma_to_pa(a)		({ vm_offset_t __a = (vm_offset_t) (a); (mfn_to_pfn(atop((__a))) << PAGE_SHIFT) | (__a & PAGE_MASK); })
#endif

#define kv_to_mfn(a)		pa_to_mfn(_kvtophys(a))
#define kv_to_ma(a)		pa_to_ma(_kvtophys(a))
#define mfn_to_kv(mfn)		phystokv(ptoa(mfn_to_pfn(mfn)))

#include <machine/xen.h>

#endif	/* MACH_XEN */
#endif /* _MACH_XEN_H */
