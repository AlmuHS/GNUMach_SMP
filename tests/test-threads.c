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

#include <stdint.h>
#include <mach/machine/thread_status.h>

#include <syscalls.h>
#include <testlib.h>

#include <mach.user.h>

void sleeping_thread(void* arg)
{
  printf("starting thread %d\n", arg);
  for (int i=0; i<100; i++)
      msleep(50);
  printf("stopping thread %d\n", arg);
  thread_terminate(mach_thread_self());
  FAILURE("thread_terminate");
}

void test_many(void)
{
  for (long tid=0; tid<10; tid++)
    {
      test_thread_start(mach_task_self(), sleeping_thread, (void*)tid);
    }
  // TODO: wait for thread end notifications
  msleep(6000);
}

#ifdef __x86_64__
void test_fsgs_base_thread(void* tid)
{
  int err;
#if defined(__SEG_FS) && defined(__SEG_GS)
  long __seg_fs *fs_ptr;
  long __seg_gs *gs_ptr;
  long fs_value;
  long gs_value;

  struct i386_fsgs_base_state state;
  state.fs_base = (unsigned long)&fs_value;
  state.gs_base = (unsigned long)&gs_value;
  err = thread_set_state(mach_thread_self(), i386_FSGS_BASE_STATE,
                         (thread_state_t) &state, i386_FSGS_BASE_STATE_COUNT);
  ASSERT_RET(err, "thread_set_state");

  fs_value = 0x100 + (long)tid;
  gs_value = 0x200 + (long)tid;

  msleep(50);  // allow the others to set their segment base

  fs_ptr = 0;
  gs_ptr = 0;
  long rdvalue = *fs_ptr;
  printf("FS expected %lx read %lx\n", fs_value, rdvalue);
  ASSERT(fs_value == rdvalue, "FS base error\n");
  rdvalue = *gs_ptr;
  printf("GS expected %lx read %lx\n", gs_value, rdvalue);
  ASSERT(gs_value == rdvalue, "GS base error\n");
#else
#error " missing __SEG_FS and __SEG_GS"
#endif

  thread_terminate(mach_thread_self());
  FAILURE("thread_terminate");
}
#endif

void test_fsgs_base(void)
{
#ifdef __x86_64__
  int err;
  for (long tid=0; tid<10; tid++)
    {
      test_thread_start(mach_task_self(), test_fsgs_base_thread, (void*)tid);
    }
  msleep(1000);  // TODO: wait for threads
#endif
}


int main(int argc, char *argv[], int envc, char *envp[])
{
  test_fsgs_base();
  test_many();
  return 0;
}
