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

#include <kern/printf.h>
#include <kern/debug.h>

#include <mach/machine/eflags.h>
#include <machine/thread.h>
#include <machine/ipl.h>

#include <machine/model_dep.h>

unsigned long cr3;

struct failsafe_callback_regs {
	unsigned int ds;
	unsigned int es;
	unsigned int fs;
	unsigned int gs;
	unsigned int ip;
	unsigned int cs_and_mask;
	unsigned int flags;
};

void hyp_failsafe_c_callback(struct failsafe_callback_regs *regs) {
	printf("Fail-Safe callback!\n");
	printf("IP: %08X CS: %4X DS: %4X ES: %4X FS: %4X GS: %4X FLAGS %08X MASK %04X\n", regs->ip, regs->cs_and_mask & 0xffff, regs->ds, regs->es, regs->fs, regs->gs, regs->flags, regs->cs_and_mask >> 16);
	panic("failsafe");
}

extern void clock_interrupt();
extern void return_to_iret;

void hypclock_machine_intr(int old_ipl, void *ret_addr, struct i386_interrupt_state *regs, unsigned64_t delta) {
	if (ret_addr == &return_to_iret) {
		clock_interrupt(delta/1000,		/* usec per tick */
			(regs->efl & EFL_VM) ||		/* user mode */ 
			((regs->cs & 0x02) != 0),	/* user mode */ 
			old_ipl == SPL0);		/* base priority */
	} else
		clock_interrupt(delta/1000, FALSE, FALSE);
}

void hyp_p2m_init(void) {
	unsigned long nb_pfns = atop(phys_last_addr);
#ifdef MACH_PSEUDO_PHYS
#define P2M_PAGE_ENTRIES (PAGE_SIZE / sizeof(unsigned long))
	unsigned long *l3 = (unsigned long *)phystokv(pmap_grab_page()), *l2 = NULL;
	unsigned long i;

	for (i = 0; i < (nb_pfns + P2M_PAGE_ENTRIES) / P2M_PAGE_ENTRIES; i++) {
		if (!(i % P2M_PAGE_ENTRIES)) {
			l2 = (unsigned long *) phystokv(pmap_grab_page());
			l3[i / P2M_PAGE_ENTRIES] = kv_to_mfn(l2);
		}
		l2[i % P2M_PAGE_ENTRIES] = kv_to_mfn(&mfn_list[i * P2M_PAGE_ENTRIES]);
	}

	hyp_shared_info.arch.pfn_to_mfn_frame_list_list = kv_to_mfn(l3);
#endif
	hyp_shared_info.arch.max_pfn = nb_pfns;
}
