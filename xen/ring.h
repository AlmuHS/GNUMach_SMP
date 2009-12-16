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

#ifndef	XEN_RING_H
#define	XEN_RING_H

typedef unsigned32_t hyp_ring_pos_t;

#define hyp_ring_idx(ring, pos) (((unsigned)(pos)) & (sizeof(ring)-1))
#define hyp_ring_cell(ring, pos) (ring)[hyp_ring_idx((ring), (pos))]
#define hyp_ring_smash(ring, prod, cons) (hyp_ring_idx((ring), (prod) + 1) == \
				 hyp_ring_idx((ring), (cons)))
#define hyp_ring_available(ring, prod, cons) hyp_ring_idx((ring), (cons)-(prod)-1)

void hyp_ring_store(void *dest, const void *src, size_t size, void *start, void *end);
void hyp_ring_fetch(void *dest, const void *src, size_t size, void *start, void *end);
size_t hyp_ring_next_word(char **c, void *start, void *end);

#endif	/* XEN_RING_H */
