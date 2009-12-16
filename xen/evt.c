/*
 *  Copyright (C) 2007 Samuel Thibault <samuel.thibault@ens-lyon.org>
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

#include <sys/types.h>
#include <string.h>
#include <mach/xen.h>
#include <machine/xen.h>
#include <machine/ipl.h>
#include <machine/gdt.h>
#include <xen/console.h>
#include "evt.h"

#define NEVNT (sizeof(unsigned long) * sizeof(unsigned long) * 8)
int	int_mask[NSPL];

spl_t curr_ipl;

void (*ivect[NEVNT])();
int intpri[NEVNT];
int iunit[NEVNT];

void hyp_c_callback(void *ret_addr, void *regs)
{
	int i, j, n;
	int cpu = 0;
	unsigned long pending_sel;

	hyp_shared_info.vcpu_info[cpu].evtchn_upcall_pending = 0;
	/* no need for a barrier on x86, xchg is already one */
#if !(defined(__i386__) || defined(__x86_64__))
	wmb();
#endif
	while ((pending_sel = xchgl(&hyp_shared_info.vcpu_info[cpu].evtchn_pending_sel, 0))) {

		for (i = 0; pending_sel; i++, pending_sel >>= 1) {
			unsigned long pending;

			if (!(pending_sel & 1))
				continue;

			while ((pending = (hyp_shared_info.evtchn_pending[i] & ~hyp_shared_info.evtchn_mask[i]))) {

				n = i * sizeof(unsigned long);
				for (j = 0; pending; j++, n++, pending >>= 1) {
					if (!(pending & 1))
						continue;

					if (ivect[n]) {
						spl_t spl = splx(intpri[n]);
						asm ("lock; andl %1,%0":"=m"(hyp_shared_info.evtchn_pending[i]):"r"(~(1<<j)));
						ivect[n](iunit[n], spl, ret_addr, regs);
						splx_cli(spl);
					} else {
						printf("warning: lost unbound event %d\n", n);
						asm ("lock; andl %1,%0":"=m"(hyp_shared_info.evtchn_pending[i]):"r"(~(1<<j)));
					}
				}
			}
		}
	}
}

void form_int_mask(void)
{
	unsigned int i, j, bit, mask;

	for (i=SPL0; i < NSPL; i++) {
		for (j=0x00, bit=0x01, mask = 0; j < NEVNT; j++, bit<<=1)
			if (intpri[j] <= i)
				mask |= bit;
		int_mask[i] = mask;
	}
}

extern void hyp_callback(void);
extern void hyp_failsafe_callback(void);

void hyp_intrinit() {
	form_int_mask();
	curr_ipl = SPLHI;
	hyp_shared_info.evtchn_mask[0] = int_mask[SPLHI];
	hyp_set_callbacks(KERNEL_CS, hyp_callback,
			  KERNEL_CS, hyp_failsafe_callback);
}

void hyp_evt_handler(evtchn_port_t port, void (*handler)(), int unit, spl_t spl) {
	if (port > NEVNT)
		panic("event channel port %d > %d not supported\n", port, NEVNT);
	intpri[port] = spl;
	iunit[port] = unit;
	form_int_mask();
	wmb();
	ivect[port] = handler;
}
