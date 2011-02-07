/*
 *  Copyright (C) 2007-2011 Samuel Thibault <samuel.thibault@ens-lyon.org>
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
#include <xen/block.h>
#include <xen/console.h>
#include <xen/grant.h>
#include <xen/net.h>
#include <xen/store.h>
#include <xen/time.h>
#include "xen.h"
#include "evt.h"

void hyp_invalidate_pte(pt_entry_t *pte)
{
	if (!hyp_mmu_update_pte(kv_to_ma(pte), (*pte) & ~INTEL_PTE_VALID))
		panic("%s:%d could not set pte %p(%p) to %p(%p)\n",__FILE__,__LINE__,pte,(vm_offset_t) kv_to_ma(pte),*pte,ma_to_pa(*pte));
	hyp_mmuext_op_void(MMUEXT_TLB_FLUSH_LOCAL);
}

void hyp_debug()
{
	panic("debug");
}

void hyp_init(void)
{ 
        hyp_grant_init(); 
        hyp_store_init(); 
	evtchn_port_t port = hyp_event_channel_bind_virq(VIRQ_DEBUG, 0);
	hyp_evt_handler(port, hyp_debug, 0, SPL7);
} 

void hyp_dev_init(void)
{
	/* these depend on hyp_init() and working threads */
	hyp_block_init(); 
	hyp_net_init(); 
}

void _hyp_halt(void)
{
	hyp_halt();
}

void _hyp_todo(unsigned long from)
{
	printf("TODO: at %lx\n",from);
	hyp_halt();
}

extern int int_mask[];
void hyp_idle(void)
{
	int cpu = 0;
	hyp_shared_info.vcpu_info[cpu].evtchn_upcall_mask = 0xff;
	barrier();
	/* Avoid blocking if there are pending events */
	if (!hyp_shared_info.vcpu_info[cpu].evtchn_upcall_pending &&
		!hyp_shared_info.evtchn_pending[cpu])
		hyp_block();
	while (1) {
		hyp_shared_info.vcpu_info[cpu].evtchn_upcall_mask = 0x00;
		barrier();
		if (!hyp_shared_info.vcpu_info[cpu].evtchn_upcall_pending &&
			!hyp_shared_info.evtchn_pending[cpu])
			/* Didn't miss any event, can return to threads.  */
			break;
		hyp_shared_info.vcpu_info[cpu].evtchn_upcall_mask = 0xff;
		hyp_c_callback(NULL,NULL);
	}
}
