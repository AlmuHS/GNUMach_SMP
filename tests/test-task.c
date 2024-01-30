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
#include <mach/vm_param.h>

#include <gnumach.user.h>
#include <mach.user.h>


void test_task()
{
  mach_port_t ourtask = mach_task_self();
  mach_msg_type_number_t count;
  int err;

  struct task_basic_info binfo;
  count = TASK_BASIC_INFO_COUNT;
  err = task_info(ourtask, TASK_BASIC_INFO, (task_info_t)&binfo, &count);
  ASSERT_RET(err, "TASK_BASIC_INFO");
  ASSERT(binfo.virtual_size > binfo.resident_size, "wrong memory counters");

  struct task_events_info einfo;
  count = TASK_EVENTS_INFO_COUNT;
  err = task_info(ourtask, TASK_EVENTS_INFO, (task_info_t)&einfo, &count);
  ASSERT_RET(err, "TASK_EVENTS_INFO");
  printf("msgs sent %llu received %llu\n",
         einfo.messages_sent, einfo.messages_received);

  struct task_thread_times_info ttinfo;
  count = TASK_THREAD_TIMES_INFO_COUNT;
  err = task_info(ourtask, TASK_THREAD_TIMES_INFO, (task_info_t)&ttinfo, &count);
  ASSERT_RET(err, "TASK_THREAD_TIMES_INFO");
  printf("run user %lld system %lld\n",
         ttinfo.user_time64.seconds, ttinfo.user_time64.nanoseconds);
}


void dummy_thread(void *arg)
{
  printf("started dummy thread\n");
  while (1)
    ;
}

void check_threads(thread_t *threads, mach_msg_type_number_t nthreads)
{  
  for (int tid=0; tid<nthreads; tid++)
    {
      struct thread_basic_info tinfo;
      mach_msg_type_number_t thcount = THREAD_BASIC_INFO_COUNT;
      int err = thread_info(threads[tid], THREAD_BASIC_INFO, (thread_info_t)&tinfo, &thcount);
      ASSERT_RET(err, "thread_info");
      ASSERT(thcount == THREAD_BASIC_INFO_COUNT, "thcount");
      printf("th%d (port %d):\n", tid, threads[tid]);
      printf(" user time %d.%06d\n", tinfo.user_time.seconds, tinfo.user_time.microseconds);
      printf(" system time %d.%06d\n", tinfo.system_time.seconds, tinfo.system_time.microseconds);
      printf(" cpu usage %d\n", tinfo.cpu_usage);
      printf(" creation time %d.%06d\n", tinfo.creation_time.seconds, tinfo.creation_time.microseconds);
    }
}

static void test_task_threads()
{
  thread_t *threads;
  mach_msg_type_number_t nthreads;
  int err;

  err = task_threads(mach_task_self(), &threads, &nthreads);
  ASSERT_RET(err, "task_threads");
  ASSERT(nthreads == 1, "nthreads");
  check_threads(threads, nthreads);

  thread_t t1 = test_thread_start(mach_task_self(), dummy_thread, 0);

  thread_t t2 = test_thread_start(mach_task_self(), dummy_thread, 0);

  // let the threads run
  msleep(100);

  err = task_threads(mach_task_self(), &threads, &nthreads);
  ASSERT_RET(err, "task_threads");
  ASSERT(nthreads == 3, "nthreads");
  check_threads(threads, nthreads);

  err = thread_terminate(t1);
  ASSERT_RET(err, "thread_terminate");
  err = thread_terminate(t2);
  ASSERT_RET(err, "thread_terminate");

  err = task_threads(mach_task_self(), &threads, &nthreads);
  ASSERT_RET(err, "task_threads");
  ASSERT(nthreads == 1, "nthreads");
  check_threads(threads, nthreads);
}

void test_new_task()
{
  int err;
  task_t newtask;
  err = task_create(mach_task_self(), 1, &newtask);
  ASSERT_RET(err, "task_create");

  err = task_suspend(newtask);
  ASSERT_RET(err, "task_suspend");

  err = task_set_name(newtask, "newtask");
  ASSERT_RET(err, "task_set_name");

  thread_t *threads;
  mach_msg_type_number_t nthreads;
  err = task_threads(newtask, &threads, &nthreads);
  ASSERT_RET(err, "task_threads");
  ASSERT(nthreads == 0, "nthreads 0");

  test_thread_start(newtask, dummy_thread, 0);

  err = task_resume(newtask);
  ASSERT_RET(err, "task_resume");

  msleep(100);  // let the thread run a bit

  err = task_threads(newtask, &threads, &nthreads);
  ASSERT_RET(err, "task_threads");
  ASSERT(nthreads == 1, "nthreads 1");
  check_threads(threads, nthreads);

  err = thread_terminate(threads[0]);
  ASSERT_RET(err, "thread_terminate");

  err = task_terminate(newtask);
  ASSERT_RET(err, "task_terminate");
}

int test_errors()
{
    int err;
    err = task_resume(MACH_PORT_NAME_DEAD);
    ASSERT(err == MACH_SEND_INVALID_DEST, "task DEAD");
}


int main(int argc, char *argv[], int envc, char *envp[])
{
  test_task();
  test_task_threads();
  test_new_task();
  test_errors();
  return 0;
}
