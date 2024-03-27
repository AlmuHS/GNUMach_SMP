/*
 * MIT License
 *
 * Copyright (c) 2017 Luc Chabassier
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* This small helper was started from
 * https://github.com/dwarfmaster/mach-ipc/blob/master/minimal_threads/main.c
 * and then reworked. */

#include <testlib.h>
#include <mach/vm_param.h>
#include <mach.user.h>

thread_t test_thread_start(task_t task, void(*routine)(void*), void* arg) {
  const vm_size_t stack_size = vm_page_size * 16;
  kern_return_t ret;
  vm_address_t stack, local_stack;

  ret = vm_allocate(mach_task_self(), &local_stack, vm_page_size, TRUE);
  ASSERT_RET(ret, "can't allocate local stack");

  ret = vm_allocate(task, &stack, stack_size, TRUE);
  ASSERT_RET(ret, "can't allocate the stack for a new thread");

  ret = vm_protect(task, stack, vm_page_size, FALSE, VM_PROT_NONE);
  ASSERT_RET(ret, "can't protect the stack from overflows");

  long *top = (long*)(local_stack + vm_page_size) - 1;
#ifdef __i386__
  *top = (long)arg; /* The argument is passed on the stack on x86_32 */
  *(top - 1) = 0;   /* The return address */
#elif defined(__x86_64__)
  *top = 0;         /* The return address */
#endif
  ret = vm_write(task, stack + stack_size - vm_page_size, local_stack, vm_page_size);
  ASSERT_RET(ret, "can't initialize the stack for the new thread");

  ret = vm_deallocate(mach_task_self(), local_stack, vm_page_size);
  ASSERT_RET(ret, "can't deallocate local stack");

  thread_t thread;
  ret = thread_create(task, &thread);
  ASSERT_RET(ret, "thread_create()");

  struct i386_thread_state state;
  unsigned int count;
  count = i386_THREAD_STATE_COUNT;
  ret = thread_get_state(thread, i386_REGS_SEGS_STATE,
                         (thread_state_t) &state, &count);
  ASSERT_RET(ret, "thread_get_state()");

#ifdef __i386__
  state.eip = (long) routine;
  state.uesp = (long) (stack + stack_size - sizeof(long) * 2);
  state.ebp = 0;
#elif defined(__x86_64__)
  state.rip = (long) routine;
  state.ursp = (long) (stack + stack_size - sizeof(long) * 1);
  state.rbp = 0;
  state.rdi = (long)arg;
#endif
  ret = thread_set_state(thread, i386_REGS_SEGS_STATE,
                         (thread_state_t) &state, i386_THREAD_STATE_COUNT);
  ASSERT_RET(ret, "thread_set_state");

  ret = thread_resume(thread);
  ASSERT_RET(ret, "thread_resume");

  return thread;
}
