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
#include <string.h>
#include "ring.h"

/* dest is ring */
void hyp_ring_store(void *dest, const void *src, size_t size, void *start, void *end)
{
	if (dest + size > end) {
		size_t first_size = end - dest;
		memcpy(dest, src, first_size);
		src += first_size;
		dest = start;
		size -= first_size;
	}
	memcpy(dest, src, size);
}

/* src is ring */
void hyp_ring_fetch(void *dest, const void *src, size_t size, void *start, void *end)
{
	if (src + size > end) {
		size_t first_size = end - src;
		memcpy(dest, src, first_size);
		dest += first_size;
		src = start;
		size -= first_size;
	}
	memcpy(dest, src, size);
}

size_t hyp_ring_next_word(char **c, void *start, void *end)
{
	size_t n = 0;

	while (**c) {
		n++;
		if (++(*c) == end)
			*c = start;
	}
	(*c)++;

	return n;
}
