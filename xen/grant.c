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

#include <sys/types.h>
#include <mach/vm_param.h>
#include <machine/spl.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>
#include "grant.h"

#define NR_RESERVED_ENTRIES 8
#define NR_GRANT_PAGES 4

decl_simple_lock_data(static,lock);
static struct grant_entry *grants;
static vm_map_entry_t grants_map_entry;
static int last_grant = NR_RESERVED_ENTRIES;

static grant_ref_t free_grants = -1;

static grant_ref_t grant_alloc(void) {
	grant_ref_t grant;
	if (free_grants != -1) {
		grant = free_grants;
		free_grants = grants[grant].frame;
	} else {
		grant = last_grant++;
		if (grant == (NR_GRANT_PAGES * PAGE_SIZE)/sizeof(*grants))
			panic("not enough grant entries, increase NR_GRANT_PAGES");
	}
	return grant;
}

static void grant_free(grant_ref_t grant) {
	grants[grant].frame = free_grants;
	free_grants = grant;
}

static grant_ref_t grant_set(domid_t domid, unsigned long mfn, uint16_t flags) {
	spl_t spl = splhigh();
	simple_lock(&lock);

	grant_ref_t grant = grant_alloc();
	grants[grant].domid = domid;
	grants[grant].frame = mfn;
	wmb();
	grants[grant].flags = flags;

	simple_unlock(&lock);
	splx(spl);
	return grant;
}

grant_ref_t hyp_grant_give(domid_t domid, unsigned long frame, int readonly) {
	return grant_set(domid, pfn_to_mfn(frame),
		GTF_permit_access | (readonly ? GTF_readonly : 0));
}

grant_ref_t hyp_grant_accept_transfer(domid_t domid, unsigned long frame) {
	return grant_set(domid, frame, GTF_accept_transfer);
}

unsigned long hyp_grant_finish_transfer(grant_ref_t grant) {
	unsigned long frame;
	spl_t spl = splhigh();
	simple_lock(&lock);

	if (!(grants[grant].flags & GTF_transfer_committed))
		panic("grant transfer %x not committed\n", grant);
	while (!(grants[grant].flags & GTF_transfer_completed))
		machine_relax();
	rmb();
	frame = grants[grant].frame;
	grant_free(grant);

	simple_unlock(&lock);
	splx(spl);
	return frame;
}

void hyp_grant_takeback(grant_ref_t grant) {
	spl_t spl = splhigh();
	simple_lock(&lock);

	if (grants[grant].flags & (GTF_reading|GTF_writing))
		panic("grant %d still in use (%lx)\n", grant, grants[grant].flags);

	/* Note: this is not safe, a cmpxchg is needed, see grant_table.h */
	grants[grant].flags = 0;
	wmb();

	grant_free(grant);

	simple_unlock(&lock);
	splx(spl);
}

void *hyp_grant_address(grant_ref_t grant) {
	return &grants[grant];
}

void hyp_grant_init(void) {
	struct gnttab_setup_table setup;
	unsigned long frame[NR_GRANT_PAGES];
	long ret;
	int i;
	vm_offset_t addr;

	setup.dom = DOMID_SELF;
	setup.nr_frames = NR_GRANT_PAGES;
	setup.frame_list = (void*) kvtolin(frame);

	ret = hyp_grant_table_op(GNTTABOP_setup_table, kvtolin(&setup), 1);
	if (ret)
		panic("setup grant table error %d", ret);
	if (setup.status)
		panic("setup grant table: %d\n", setup.status);
	
	simple_lock_init(&lock);
	vm_map_find_entry(kernel_map, &addr, NR_GRANT_PAGES * PAGE_SIZE,
			  (vm_offset_t) 0, kernel_object, &grants_map_entry);
	grants = (void*) addr;

	for (i = 0; i < NR_GRANT_PAGES; i++)
		pmap_map_mfn((void *)grants + i * PAGE_SIZE, frame[i]);
}
