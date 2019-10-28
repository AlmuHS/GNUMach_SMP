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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _KERN_STARTUP_H_
#define _KERN_STARTUP_H_

#include <kern/thread.h>

extern void setup_main(void);
void cpu_launch_first_thread(thread_t th);
void start_kernel_threads(void);
void notify_real_shutdown(void);

#if	NCPUS > 1
void slave_main();
#endif

#endif /* _KERN_STARTUP_H_ */
