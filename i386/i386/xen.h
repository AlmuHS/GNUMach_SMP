/*
 *  Copyright (C) 2006-2011 Free Software Foundation
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

#ifndef XEN_HYPCALL_H
#define XEN_HYPCALL_H

#ifdef	MACH_XEN
#ifndef	__ASSEMBLER__
#include <kern/macros.h>
#include <kern/printf.h>
#include <mach/machine/vm_types.h>
#include <mach/vm_param.h>
#include <mach/inline.h>
#include <mach/xen.h>
#include <machine/vm_param.h>
#include <intel/pmap.h>
#include <kern/debug.h>
#include <xen/public/xen.h>

/* TODO: this should be moved in appropriate non-Xen place.  */
#define mb() __asm__ __volatile__("lock; addl $0,0(%%esp)":::"memory")
#define rmb() mb()
#define wmb() mb()
static inline unsigned long xchgl(volatile unsigned long *ptr, unsigned long x)
{
	__asm__ __volatile__("xchg %0, %1"
			   : "=r" (x)
			   : "m" (*(ptr)), "0" (x): "memory");
	return x;
}
#define _TOSTR(x) #x
#define TOSTR(x) _TOSTR (x)

#ifdef __i386__
#define _hypcall_ret "=a"
#define _hypcall_arg1 "ebx"
#define _hypcall_arg2 "ecx"
#define _hypcall_arg3 "edx"
#define _hypcall_arg4 "esi"
#define _hypcall_arg5 "edi"
#endif
#ifdef __x86_64__
#define _hypcall_ret "=a"
#define _hypcall_arg1 "rdi"
#define _hypcall_arg2 "rsi"
#define _hypcall_arg3 "rdx"
#define _hypcall_arg4 "r10"
#define _hypcall_arg5 "r8"
#endif


/* x86-specific hypercall interface.  */
#define _hypcall0(type, name) \
static inline type hyp_##name(void) \
{ \
	unsigned long __ret; \
	asm volatile ("call hypcalls+("TOSTR(__HYPERVISOR_##name)"*32)" \
		: "=a" (__ret) \
		: : "memory"); \
	return __ret; \
}

#define _hypcall1(type, name, type1, arg1) \
static inline type hyp_##name(type1 arg1) \
{ \
	unsigned long __ret; \
	register unsigned long __arg1 asm(_hypcall_arg1) = (unsigned long) arg1; \
	asm volatile ("call hypcalls+("TOSTR(__HYPERVISOR_##name)"*32)" \
		: "=a" (__ret), \
		  "+r" (__arg1) \
		: : "memory"); \
	return __ret; \
}

#define _hypcall2(type, name, type1, arg1, type2, arg2) \
static inline type hyp_##name(type1 arg1, type2 arg2) \
{ \
	unsigned long __ret; \
	register unsigned long __arg1 asm(_hypcall_arg1) = (unsigned long) arg1; \
	register unsigned long __arg2 asm(_hypcall_arg2) = (unsigned long) arg2; \
	asm volatile ("call hypcalls+("TOSTR(__HYPERVISOR_##name)"*32)" \
		: "=a" (__ret), \
		  "+r" (__arg1), \
		  "+r" (__arg2) \
		: : "memory"); \
	return __ret; \
}

#define _hypcall3(type, name, type1, arg1, type2, arg2, type3, arg3) \
static inline type hyp_##name(type1 arg1, type2 arg2, type3 arg3) \
{ \
	unsigned long __ret; \
	register unsigned long __arg1 asm(_hypcall_arg1) = (unsigned long) arg1; \
	register unsigned long __arg2 asm(_hypcall_arg2) = (unsigned long) arg2; \
	register unsigned long __arg3 asm(_hypcall_arg3) = (unsigned long) arg3; \
	asm volatile ("call hypcalls+("TOSTR(__HYPERVISOR_##name)"*32)" \
		: "=a" (__ret), \
		  "+r" (__arg1), \
		  "+r" (__arg2), \
		  "+r" (__arg3) \
		: : "memory"); \
	return __ret; \
}

#define _hypcall4(type, name, type1, arg1, type2, arg2, type3, arg3, type4, arg4) \
static inline type hyp_##name(type1 arg1, type2 arg2, type3 arg3, type4 arg4) \
{ \
	unsigned long __ret; \
	register unsigned long __arg1 asm(_hypcall_arg1) = (unsigned long) arg1; \
	register unsigned long __arg2 asm(_hypcall_arg2) = (unsigned long) arg2; \
	register unsigned long __arg3 asm(_hypcall_arg3) = (unsigned long) arg3; \
	register unsigned long __arg4 asm(_hypcall_arg4) = (unsigned long) arg4; \
	asm volatile ("call hypcalls+("TOSTR(__HYPERVISOR_##name)"*32)" \
		: "=a" (__ret), \
		  "+r" (__arg1), \
		  "+r" (__arg2), \
		  "+r" (__arg3), \
		  "+r" (__arg4) \
		: : "memory"); \
	return __ret; \
}

#define _hypcall5(type, name, type1, arg1, type2, arg2, type3, arg3, type4, arg4, type5, arg5) \
static inline type hyp_##name(type1 arg1, type2 arg2, type3 arg3, type4 arg4, type5 arg5) \
{ \
	unsigned long __ret; \
	register unsigned long __arg1 asm(_hypcall_arg1) = (unsigned long) arg1; \
	register unsigned long __arg2 asm(_hypcall_arg2) = (unsigned long) arg2; \
	register unsigned long __arg3 asm(_hypcall_arg3) = (unsigned long) arg3; \
	register unsigned long __arg4 asm(_hypcall_arg4) = (unsigned long) arg4; \
	register unsigned long __arg5 asm(_hypcall_arg5) = (unsigned long) arg5; \
	asm volatile ("call hypcalls+("TOSTR(__HYPERVISOR_##name)"*32)" \
		: "=a" (__ret), \
		  "+r" (__arg1), \
		  "+r" (__arg2), \
		  "+r" (__arg3), \
		  "+r" (__arg4), \
		  "+r" (__arg5) \
		: : "memory"); \
	return __ret; \
}

/* x86 Hypercalls */

/* Note: since Hypervisor uses flat memory model, remember to always use
 * kvtolin when giving pointers as parameters for the hypercall to read data
 * at. Use kv_to_la when they may be used before GDT got set up. */

_hypcall1(long, set_trap_table, vm_offset_t /* struct trap_info * */, traps);

#ifdef MACH_PV_PAGETABLES
_hypcall4(int, mmu_update, vm_offset_t /* struct mmu_update * */, req, int, count, vm_offset_t /* int * */, success_count, domid_t, domid)
static inline int hyp_mmu_update_pte(pt_entry_t pte, pt_entry_t val)
{
	struct mmu_update update =
	{
		.ptr = pte,
		.val = val,
	};
	int count;
	hyp_mmu_update(kv_to_la(&update), 1, kv_to_la(&count), DOMID_SELF);
	return count;
}
/* Note: make sure this fits in KERNEL_STACK_SIZE */
#define HYP_BATCH_MMU_UPDATES 256

#define hyp_mmu_update_la(la, val) hyp_mmu_update_pte( \
	(kernel_page_dir[lin2pdenum_cont((vm_offset_t)(la))] & INTEL_PTE_PFN) \
		+ ptenum((vm_offset_t)(la)) * sizeof(pt_entry_t), val)
#endif

_hypcall2(long, set_gdt, vm_offset_t /* unsigned long * */, frame_list, unsigned int, entries)

_hypcall2(long, stack_switch, unsigned long, ss, unsigned long, esp);

#ifdef __i386__
_hypcall4(long, set_callbacks,  unsigned long, es,  void *, ea,
				unsigned long, fss, void *, fsa);
#endif
#ifdef __x86_64__
_hypcall3(long, set_callbacks,  void *, ea, void *, fsa, void *, sc);
#endif
_hypcall1(long, fpu_taskswitch, int, set);

#ifdef PAE
#define hyp_high(pte) ((pte) >> 32)
#else
#define hyp_high(pte) 0
#endif
#ifdef __i386__
_hypcall4(long, update_descriptor, unsigned long, ma_lo, unsigned long, ma_hi, unsigned long, desc_lo, unsigned long, desc_hi);
#define hyp_do_update_descriptor(ma, desc) ({ \
	pt_entry_t __ma = (ma); \
	uint64_t __desc = (desc); \
	hyp_update_descriptor(__ma & 0xffffffffU, hyp_high(__ma), __desc & 0xffffffffU, __desc >> 32); \
})
#endif
#ifdef __x86_64__
_hypcall2(long, update_descriptor, unsigned long, ma, unsigned long, desc);
#define hyp_do_update_descriptor(ma, desc) hyp_update_descriptor(ma, desc)
#endif

#ifdef __x86_64__
_hypcall2(long, set_segment_base, int, reg, unsigned long, value);
#endif

#include <xen/public/memory.h>
_hypcall2(long, memory_op, unsigned long, cmd, vm_offset_t /* void * */, arg);
static inline void hyp_free_mfn(unsigned long mfn)
{
	struct xen_memory_reservation reservation;
	reservation.extent_start = (void*) kvtolin(&mfn);
	reservation.nr_extents = 1;
	reservation.extent_order = 0;
	reservation.address_bits = 0;
	reservation.domid = DOMID_SELF;
	if (hyp_memory_op(XENMEM_decrease_reservation, kvtolin(&reservation)) != 1)
		panic("couldn't free page %lu\n", mfn);
}

#ifdef __i386__
_hypcall4(int, update_va_mapping, unsigned long, va, unsigned long, val_lo, unsigned long, val_hi, unsigned long, flags);
#define hyp_do_update_va_mapping(va, val, flags) ({ \
	pt_entry_t __val = (val); \
	hyp_update_va_mapping(va, __val & 0xffffffffU, hyp_high(__val), flags); \
})
#endif
#ifdef __x86_64__
_hypcall3(int, update_va_mapping, unsigned long, va, unsigned long, val, unsigned long, flags);
#define hyp_do_update_va_mapping(va, val, flags) hyp_update_va_mapping(va, val, flags)
#endif

static inline void hyp_free_page(unsigned long pfn, void *va)
{
    /* save mfn */
    unsigned long mfn = pfn_to_mfn(pfn);

#ifdef MACH_PV_PAGETABLES
    /* remove from mappings */
    if (hyp_do_update_va_mapping(kvtolin(va), 0, UVMF_INVLPG|UVMF_ALL))
	    panic("couldn't clear page %lu at %p\n", pfn, va);

#ifdef  MACH_PSEUDO_PHYS
    /* drop machine page */
    mfn_list[pfn] = ~0;
#endif  /* MACH_PSEUDO_PHYS */
#endif

    /* and free from Xen  */
    hyp_free_mfn(mfn);
}

#ifdef MACH_PV_PAGETABLES
_hypcall4(int, mmuext_op, vm_offset_t /* struct mmuext_op * */, op, int, count, vm_offset_t /* int * */, success_count, domid_t, domid);
static inline int hyp_mmuext_op_void(unsigned int cmd)
{
	struct mmuext_op op = {
		.cmd = cmd,
	};
	int count;
	hyp_mmuext_op(kv_to_la(&op), 1, kv_to_la(&count), DOMID_SELF);
	return count;
}
static inline int hyp_mmuext_op_mfn(unsigned int cmd, unsigned long mfn)
{
	struct mmuext_op op = {
		.cmd = cmd,
		.arg1.mfn = mfn,
	};
	int count;
	hyp_mmuext_op(kv_to_la(&op), 1, kv_to_la(&count), DOMID_SELF);
	return count;
}
static inline void hyp_set_ldt(void *ldt, unsigned long nbentries) {
	struct mmuext_op op = {
		.cmd = MMUEXT_SET_LDT,
		.arg1.linear_addr = kvtolin(ldt),
		.arg2.nr_ents = nbentries,
	};
	unsigned long count;
	if (((unsigned long)ldt) & PAGE_MASK)
		panic("ldt %p is not aligned on a page\n", ldt);
	for (count=0; count<nbentries; count+= PAGE_SIZE/8)
		pmap_set_page_readonly(ldt+count*8);
	hyp_mmuext_op(kvtolin(&op), 1, kvtolin(&count), DOMID_SELF);
	if (!count)
		panic("couldn't set LDT\n");
}
#define hyp_set_cr3(value) hyp_mmuext_op_mfn(MMUEXT_NEW_BASEPTR, pa_to_mfn(value))
#define hyp_set_user_cr3(value) hyp_mmuext_op_mfn(MMUEXT_NEW_USER_BASEPTR, pa_to_mfn(value))
static inline void hyp_invlpg(vm_offset_t lin) {
	struct mmuext_op ops;
	int n;
	ops.cmd = MMUEXT_INVLPG_ALL;
	ops.arg1.linear_addr = lin;
	hyp_mmuext_op(kvtolin(&ops), 1, kvtolin(&n), DOMID_SELF);
	if (n < 1)
		panic("couldn't invlpg\n");
}
#endif

#ifdef __i386__
_hypcall2(long, set_timer_op, unsigned long, absolute_lo, unsigned long, absolute_hi);
#define hyp_do_set_timer_op(absolute_nsec) ({ \
	uint64_t __absolute = (absolute_nsec); \
	hyp_set_timer_op(__absolute & 0xffffffffU, __absolute >> 32); \
})
#endif
#ifdef __x86_64__
_hypcall1(long, set_timer_op, unsigned long, absolute);
#define hyp_do_set_timer_op(absolute_nsec) hyp_set_timer_op(absolute_nsec)
#endif

#include <xen/public/event_channel.h>
_hypcall1(int, event_channel_op, vm_offset_t /* evtchn_op_t * */, op);
static inline int hyp_event_channel_send(evtchn_port_t port) {
	evtchn_op_t op = {
		.cmd = EVTCHNOP_send,
		.u.send.port = port,
	};
	return hyp_event_channel_op(kvtolin(&op));
}
static inline evtchn_port_t hyp_event_channel_alloc(domid_t domid) {
	evtchn_op_t op  = {
		.cmd = EVTCHNOP_alloc_unbound,
		.u.alloc_unbound.dom = DOMID_SELF,
		.u.alloc_unbound.remote_dom = domid,
	};
	if (hyp_event_channel_op(kvtolin(&op)))
		panic("couldn't allocate event channel");
	return op.u.alloc_unbound.port;
}
static inline evtchn_port_t hyp_event_channel_bind_virq(uint32_t virq, uint32_t vcpu) {
	evtchn_op_t op = { .cmd = EVTCHNOP_bind_virq, .u.bind_virq = { .virq = virq, .vcpu = vcpu }};
	if (hyp_event_channel_op(kvtolin(&op)))
		panic("can't bind virq %d\n",virq);
	return op.u.bind_virq.port;
}

_hypcall3(int, console_io, int, cmd, int, count, vm_offset_t /* const char * */, buffer);

_hypcall3(long, grant_table_op, unsigned int, cmd, vm_offset_t /* void * */, uop, unsigned int, count);

_hypcall2(long, vm_assist, unsigned int, cmd, unsigned int, type);

_hypcall0(long, iret);

#include <xen/public/sched.h>
_hypcall2(long, sched_op, int, cmd, vm_offset_t /* void* */, arg)
#define hyp_yield() hyp_sched_op(SCHEDOP_yield, 0)
#define hyp_block() hyp_sched_op(SCHEDOP_block, 0)
static inline void __attribute__((noreturn)) hyp_crash(void)
{
	unsigned int shut = SHUTDOWN_crash;
	hyp_sched_op(SCHEDOP_shutdown, kvtolin(&shut));
	/* really shouldn't return */
	printf("uh, shutdown returned?!\n");
	for(;;);
}

static inline void __attribute__((noreturn)) hyp_halt(void)
{
	unsigned int shut = SHUTDOWN_poweroff;
	hyp_sched_op(SCHEDOP_shutdown, kvtolin(&shut));
	/* really shouldn't return */
	printf("uh, shutdown returned?!\n");
	for(;;);
}

static inline void __attribute__((noreturn)) hyp_reboot(void)
{
	unsigned int shut = SHUTDOWN_reboot;
	hyp_sched_op(SCHEDOP_shutdown, kvtolin(&shut));
	/* really shouldn't return */
	printf("uh, reboot returned?!\n");
	for(;;);
}

_hypcall2(int, set_debugreg, int, reg, unsigned long, value);
_hypcall1(unsigned long, get_debugreg, int, reg);

/* x86-specific */
static inline uint64_t hyp_cpu_clock(void) {
	uint32_t hi, lo;
	asm volatile("rdtsc" : "=d"(hi), "=a"(lo));
	return (((uint64_t) hi) << 32) | lo;
}

#else	/* __ASSEMBLER__ */
/* TODO: SMP */
#define cli movb $0xff,hyp_shared_info+CPU_CLI
#define sti call hyp_sti
#define iretq jmp hyp_iretq
#endif	/* ASSEMBLER */
#endif	/* MACH_XEN */

#endif /* XEN_HYPCALL_H */
