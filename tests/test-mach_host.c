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

#include <mach_host.user.h>

void test_kernel_version()
{
  int err;
  kernel_version_t kver;
  err = host_get_kernel_version(mach_host_self(), kver);
  ASSERT_RET(err, "host_kernel_info");
  printf("kernel version: %s\n", kver);
}

void test_host_info()
{
  int err;
  mach_msg_type_number_t count;
  mach_port_t thishost = mach_host_self();

  host_basic_info_data_t binfo;
  count = HOST_BASIC_INFO_COUNT;
  err = host_info(thishost, HOST_BASIC_INFO, (host_info_t)&binfo, &count);
  ASSERT_RET(err, "host_basic_info");
  ASSERT(count == HOST_BASIC_INFO_COUNT, "");
  ASSERT(binfo.max_cpus > 0, "no cpu?");
  ASSERT(binfo.avail_cpus > 0, "no cpu available?");
  ASSERT(binfo.memory_size > (1024 * 1024), "memory too low");

  const int maxcpus = 255;
  int proc_slots[maxcpus];
  count = maxcpus;
  err = host_info(thishost, HOST_PROCESSOR_SLOTS, (host_info_t)&proc_slots, &count);
  ASSERT_RET(err, "host_processor_slots");
  ASSERT((1 <= count) && (count <= maxcpus), "");

  host_sched_info_data_t sinfo;
  count = HOST_SCHED_INFO_COUNT;
  err = host_info(thishost, HOST_SCHED_INFO, (host_info_t)&sinfo, &count);
  ASSERT_RET(err, "host_sched_info");
  ASSERT(count == HOST_SCHED_INFO_COUNT, "");
  ASSERT(sinfo.min_timeout < 1000, "timeout unexpectedly high");
  ASSERT(sinfo.min_quantum < 1000, "quantum unexpectedly high");

  host_load_info_data_t linfo;
  count = HOST_LOAD_INFO_COUNT;
  err = host_info(thishost, HOST_LOAD_INFO, (host_info_t)&linfo, &count);
  ASSERT_RET(err, "host_load_info");
  ASSERT(count == HOST_LOAD_INFO_COUNT, "");
  for (int i=0; i<3; i++)
  {
      printf("avenrun %d\n", linfo.avenrun[i]);
      printf("mach_factor %d\n", linfo.mach_factor[i]);
  }
}

// TODO processor sets

int main(int argc, char *argv[], int envc, char *envp[])
{
  test_kernel_version();
  test_host_info();
  return 0;
}
