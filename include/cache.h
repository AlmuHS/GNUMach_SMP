/*
 * Copyright (c) 2013 Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MACH_CACHE_H_
#define _MACH_CACHE_H_

/* This macro can be used to align statically allocated objects so
   that they start at a cache line. */
#define  __cacheline_aligned __attribute__((aligned(1 << CPU_L1_SHIFT)))

#endif /* _MACH_CACHE_H_ */
