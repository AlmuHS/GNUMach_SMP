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

#ifndef XEN_GRANT_H
#define XEN_GRANT_H
#include <sys/types.h>
#include <machine/xen.h>
#include <xen/public/xen.h>
#include <xen/public/grant_table.h>

void hyp_grant_init(void);
grant_ref_t hyp_grant_give(domid_t domid, unsigned long frame_nr, int readonly);
void hyp_grant_takeback(grant_ref_t grant);
grant_ref_t hyp_grant_accept_transfer(domid_t domid, unsigned long frame_nr);
unsigned long hyp_grant_finish_transfer(grant_ref_t grant);
void *hyp_grant_address(grant_ref_t grant);

#endif /* XEN_GRANT_H */
