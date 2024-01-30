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

#include <syscalls.h>
#include <testlib.h>

#include <mach/machine/vm_param.h>
#include <mach/std_types.h>
#include <mach/mach_types.h>
#include <mach/vm_wire.h>

#include <mach.user.h>
#include <gnumach.user.h>

/* Gsync flags.  */
#ifndef GSYNC_SHARED
# define GSYNC_SHARED      0x01
# define GSYNC_QUAD        0x02
# define GSYNC_TIMED       0x04
# define GSYNC_BROADCAST   0x08
# define GSYNC_MUTATE      0x10
#endif

static uint32_t single_shared;
static struct {
  uint32_t val1;
  uint32_t val2;
} single_shared_quad;

static void test_single()
{
  int err;
  single_shared = 0;
  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 0, 0, 100, GSYNC_TIMED);
  ASSERT(err == KERN_TIMEDOUT, "gsync_wait did not timeout");

  single_shared = 1;
  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 0, 0, 100, GSYNC_TIMED);
  ASSERT(err == KERN_INVALID_ARGUMENT, "gsync_wait on wrong value");
  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 1, 0, 100, GSYNC_TIMED);
  ASSERT(err == KERN_TIMEDOUT, "gsync_wait again on correct value did not timeout");

  single_shared_quad.val1 = 1;
  single_shared_quad.val2 = 2;
  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared_quad, 99, 88,
                   100, GSYNC_TIMED | GSYNC_QUAD);
  ASSERT(err == KERN_INVALID_ARGUMENT, "gsync_wait on wrong quad value");
  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared_quad, 1, 2,
                   100, GSYNC_TIMED | GSYNC_QUAD);
  ASSERT(err == KERN_TIMEDOUT, "gsync_wait again on correct value did not timeout");

  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, 0);
  ASSERT_RET(err, "gsync_wake with nobody waiting");

  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 1, 0, 100, GSYNC_TIMED);
  ASSERT(err == KERN_TIMEDOUT, "gsync_wait not affected by previous gsync_wake");

  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, GSYNC_BROADCAST);
  ASSERT_RET(err, "gsync_wake broadcast with nobody waiting");

  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 1, 0, 100, GSYNC_TIMED);
  ASSERT(err == KERN_TIMEDOUT, "gsync_wait not affected by previous gsync_wake");

  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 2, GSYNC_MUTATE);
  ASSERT_RET(err, "gsync_wake nobody + mutate");
  ASSERT(single_shared == 2, "gsync_wake single_shared did not mutate");

  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, 0);
  ASSERT_RET(err, "gsync_wake nobody without mutate");
  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, 0, 0);
  ASSERT_RET(err, "gsync_wake 3a");
}

static void single_thread_setter(void *arg)
{
  int err;
  int val = (long)arg;

  /* It should be enough to sleep a bit for our creator to call
     gsync_wait(). To verify that the test is performed with the
     correct sequence, we also change the value so if the wait is
     called after our wake it will fail with KERN_INVALID_ARGUMENT */
  msleep(100);

  err = gsync_wake(mach_task_self(), (vm_offset_t)&single_shared, val, GSYNC_MUTATE);
  ASSERT_RET(err, "gsync_wake from thread + mutate");

  thread_terminate(mach_thread_self());
  FAILURE("thread_terminate");
}

static void test_single_from_thread()
{
  int err;
  single_shared = 10;
  test_thread_start(mach_task_self(), single_thread_setter, (void*)11);
  err = gsync_wait(mach_task_self(), (vm_offset_t)&single_shared, 10, 0, 0, 0);
  ASSERT_RET(err, "gsync_wait without timeout for wake from another thread");
  ASSERT(single_shared == 11, "wake didn't mutate");
}

int main(int argc, char *argv[], int envc, char *envp[])
{
  test_single_from_thread();
  test_single();
  return 0;
}
