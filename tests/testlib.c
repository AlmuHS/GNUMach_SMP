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

#include <testlib.h>

#include <device/cons.h>
#include <mach/kern_return.h>
#include <mach/message.h>
#include <mach/mig_errors.h>
#include <mach/vm_param.h>
#include <sys/reboot.h>

#include <mach.user.h>
#include <mach_host.user.h>


static int argc = 0;
static char *argv_unknown[] = {"unknown", "m1", "123", "456"};
static char **argv = argv_unknown;
static char **envp = NULL;
static int envc = 0;

static mach_port_t host_priv_port = 1;
static mach_port_t device_master_port = 2;

void cnputc(char c, vm_offset_t cookie)
{
    char buf[2] = {c, 0};
    mach_print(buf);
}

mach_port_t host_priv(void)
{
    return host_priv_port;
}

mach_port_t device_priv(void)
{
    return device_master_port;
}

void halt()
{
  int ret = host_reboot(host_priv_port, RB_HALT);
  ASSERT_RET(ret, "host_reboot() failed!");
  while (1)
    ;
}

int msleep(uint32_t timeout)
{
  mach_port_t recv = mach_reply_port();
  return mach_msg(NULL, MACH_RCV_MSG|MACH_RCV_TIMEOUT|MACH_RCV_INTERRUPT,
                  0, 0, recv, timeout, MACH_PORT_NULL);
}

const char* e2s(int err)
{
  const char* s = e2s_gnumach(err);
  if (s != NULL)
    return s;
  else
    switch (err)
      {
      default: return "unknown";
      }
}

/*
 * Minimal _start() for test modules, we just take the arguments from the
 * kernel, call main() and reboot. As in glibc, we expect the argument pointer
 * as a first asrgument.
 */
void __attribute__((used, retain))
c_start(void **argptr)
{
  intptr_t* argcptr = (intptr_t*)argptr;
  argc = argcptr[0];
  argv = (char **) &argcptr[1];
  envp = &argv[argc + 1];
  envc = 0;

  while (envp[envc])
    ++envc;

  mach_atoi(argv[1], &host_priv_port);
  mach_atoi(argv[2], &device_master_port);

  printf("started %s", argv[0]);
  for (int i=1; i<argc; i++)
  {
      printf(" %s", argv[i]);
  }
  printf("\n");

  int ret = main(argc, argv, envc, envp);

  printf("%s: test %s exit code %x\n", TEST_SUCCESS_MARKER, argv[0], ret);
  halt();
}
