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
#include <mach_port.user.h>
#include <mach_host.user.h>

#ifdef PAGE_SIZE
vm_size_t vm_page_size = PAGE_SIZE;
#else
vm_size_t vm_page_size;
#endif

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

void mach_msg_destroy(mach_msg_header_t *msg)
{
	mach_port_t	tmp;

	tmp = mach_reply_port();

	msg->msgh_local_port = msg->msgh_remote_port;
	msg->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND,
					MACH_MSGH_BITS_REMOTE(msg->msgh_bits))
			 | MACH_MSGH_BITS_OTHER(msg->msgh_bits);

	mach_msg(msg, MACH_SEND_MSG, msg->msgh_size, 0, MACH_PORT_NULL,
		 MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

	mach_port_mod_refs(mach_task_self(), tmp, MACH_PORT_RIGHT_RECEIVE, -1);
}

mach_msg_return_t mach_msg_server(
	boolean_t		(*demux) (mach_msg_header_t *request,
					  mach_msg_header_t *reply),
	mach_msg_size_t		max_size,
	mach_port_t		rcv_name,
	mach_msg_option_t	options)
{
	mach_msg_return_t	mr;
	mig_reply_header_t	*request;
	mig_reply_header_t	*reply;
	mig_reply_header_t	*tmp;
	boolean_t		handled;

	request = __builtin_alloca(max_size);
	reply = __builtin_alloca(max_size);

GetRequest:
	mr = mach_msg(&request->Head, MACH_RCV_MSG|options,
		      0, max_size, rcv_name,
		      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (mr)
		return mr;

Handle:
	handled = demux(&request->Head, &reply->Head);
	if (!handled)
		reply->RetCode = MIG_BAD_ID;

	if (reply->RetCode == MIG_NO_REPLY)
		goto GetRequest;
	else if (reply->RetCode != KERN_SUCCESS) {
		request->Head.msgh_remote_port = MACH_PORT_NULL;
		mach_msg_destroy(&request->Head);
	}

	if (!MACH_PORT_VALID(reply->Head.msgh_remote_port)) {
		mach_msg_destroy(&reply->Head);
		goto GetRequest;
	}

	tmp = request;
	request = reply;
	reply = tmp;

	mr = mach_msg(&request->Head, MACH_SEND_MSG|MACH_RCV_MSG|options,
		      request->Head.msgh_size, max_size, rcv_name,
		      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

	if (mr == MACH_MSG_SUCCESS)
		goto Handle;
	else if (mr == MACH_SEND_INVALID_DEST) {
		mach_msg_destroy(&request->Head);
		goto GetRequest;
	}

	return mr;
}

mach_msg_return_t mach_msg_server_once(
	boolean_t		(*demux) (mach_msg_header_t *request,
					  mach_msg_header_t *reply),
	mach_msg_size_t		max_size,
	mach_port_t		rcv_name,
	mach_msg_option_t	options)
{
	mach_msg_return_t	mr;
	mig_reply_header_t	*request;
	mig_reply_header_t	*reply;
	boolean_t		handled;

	request = __builtin_alloca(max_size);
	reply = __builtin_alloca(max_size);

	mr = mach_msg(&request->Head, MACH_RCV_MSG|options,
		      0, max_size, rcv_name,
		      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
	if (mr)
		return mr;

	handled = demux(&request->Head, &reply->Head);
	if (!handled)
		reply->RetCode = MIG_BAD_ID;

	if (reply->RetCode == MIG_NO_REPLY)
		return MACH_MSG_SUCCESS;
	else if (reply->RetCode != KERN_SUCCESS) {
		request->Head.msgh_remote_port = MACH_PORT_NULL;
		mach_msg_destroy(&request->Head);
	}

	if (!MACH_PORT_VALID(reply->Head.msgh_remote_port)) {
		mach_msg_destroy(&reply->Head);
		return MACH_MSG_SUCCESS;
	}

	mr = mach_msg(&reply->Head, MACH_SEND_MSG|options,
		      reply->Head.msgh_size, 0, MACH_PORT_NULL,
		      MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);

	if (mr == MACH_SEND_INVALID_DEST)
		mach_msg_destroy(&reply->Head);

	return mr;
}

/*
 * Minimal _start() for test modules, we just take the arguments from the
 * kernel, call main() and reboot. As in glibc, we expect the argument pointer
 * as a first asrgument.
 */
void __attribute__((used, retain))
c_start(void **argptr)
{
  kern_return_t kr;
  intptr_t* argcptr = (intptr_t*)argptr;
  argc = argcptr[0];
  argv = (char **) &argcptr[1];
  envp = &argv[argc + 1];
  envc = 0;

  while (envp[envc])
    ++envc;

  mach_atoi(argv[1], &host_priv_port);
  mach_atoi(argv[2], &device_master_port);

#ifndef PAGE_SIZE
  vm_statistics_data_t stats;
  kr = vm_statistics (mach_task_self(), &stats);
  ASSERT_RET(kr, "can't get page size");
  vm_page_size = stats.pagesize;
#endif

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
