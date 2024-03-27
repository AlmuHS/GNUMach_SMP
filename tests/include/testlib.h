/*
 *  Copyright (C) 2024 Free Software Foundation
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

#ifndef TESTLIB_H
#define TESTLIB_H

// in freestanding we can only use a few standard headers
//   float.h iso646.h limits.h stdarg.h stdbool.h stddef.h stdint.h

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include <string.h> // we shouldn't include this from gcc, but it seems to be ok

#include <mach/mach_types.h>
#include <kern/printf.h>
#include <util/atoi.h>
#include <syscalls.h>

#define ASSERT(cond, msg) do {                                          \
    if (!(cond))                                                        \
      {                                                                 \
        printf("%s: " #cond " failed: %s\n",                            \
               TEST_FAILURE_MARKER, (msg));                             \
        halt();                                                         \
      }                                                                 \
  } while (0)

#define ASSERT_RET(ret, msg) do {                                       \
    if ((ret) != KERN_SUCCESS)                                          \
      {                                                                 \
        printf("%s %s (0x%x): %s\n",                                    \
               TEST_FAILURE_MARKER, e2s((ret)), (ret), (msg));          \
        halt();                                                         \
      }                                                                 \
  } while (0)

#define FAILURE(msg) do {                                \
    printf("%s: %s\n", TEST_FAILURE_MARKER, (msg));      \
    halt();                                              \
  } while (0)


extern const char* TEST_SUCCESS_MARKER;
extern const char* TEST_FAILURE_MARKER;

const char* e2s(int err);
const char* e2s_gnumach(int err);
extern void __attribute__((noreturn)) halt();
int msleep(uint32_t timeout);
thread_t test_thread_start(task_t task, void(*routine)(void*), void* arg);

mach_port_t host_priv(void);
mach_port_t device_priv(void);

extern vm_size_t vm_page_size;

extern void mach_msg_destroy(mach_msg_header_t *msg);

extern mach_msg_return_t mach_msg_server(
	boolean_t		(*demux) (mach_msg_header_t *request,
					  mach_msg_header_t *reply),
	mach_msg_size_t		max_size,
	mach_port_t		rcv_name,
	mach_msg_option_t	options);

extern mach_msg_return_t mach_msg_server_once(
	boolean_t		(*demux) (mach_msg_header_t *request,
					  mach_msg_header_t *reply),
	mach_msg_size_t		max_size,
	mach_port_t		rcv_name,
	mach_msg_option_t	options);

int main(int argc, char *argv[], int envc, char *envp[]);

#endif /* TESTLIB_H */
