/*
 *  Copyright (C) 2023 Free Software Foundation
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

#ifndef COPY_USER_H
#define COPY_USER_H

#include <stdint.h>
#include <sys/types.h>

#include <machine/locore.h>
#include <mach/message.h>

/*
 * The copyin_32to64() and copyout_64to32() routines are meant for data types
 * that have different size in kernel and user space. They should be independent
 * of endianness and hopefully can be reused on all archs.
 * These types are e.g.:
 * - port names vs port pointers, on a 64-bit kernel
 * - memory addresses, on a 64-bit kernel and 32-bit user
 */

static inline int copyin_32to64(const uint32_t *uaddr, uint64_t *kaddr)
{
  uint32_t rkaddr;
  int ret;
  ret = copyin(uaddr, &rkaddr, sizeof(uint32_t));
  if (ret)
    return ret;
  *kaddr = rkaddr;
  return 0;
}

static inline int copyout_64to32(const uint64_t *kaddr, uint32_t *uaddr)
{
  uint32_t rkaddr=*kaddr;
  return copyout(&rkaddr, uaddr, sizeof(uint32_t));
}

static inline int copyin_address(const rpc_vm_offset_t *uaddr, vm_offset_t *kaddr)
{
#ifdef USER32
  return copyin_32to64(uaddr, kaddr);
#else /* USER32 */
  return copyin(uaddr, kaddr, sizeof(*uaddr));
#endif /* USER32 */
}

static inline int copyout_address(const vm_offset_t *kaddr, rpc_vm_offset_t *uaddr)
{
#ifdef USER32
  return copyout_64to32(kaddr, uaddr);
#else /* USER32 */
  return copyout(kaddr, uaddr, sizeof(*kaddr));
#endif /* USER32 */
}

static inline int copyin_port(const mach_port_name_t *uaddr, mach_port_t *kaddr)
{
#ifdef __LP64__
  return copyin_32to64(uaddr, kaddr);
#else /* __LP64__ */
  return copyin(uaddr, kaddr, sizeof(*uaddr));
#endif /* __LP64__ */
}

static inline int copyout_port(const mach_port_t *kaddr, mach_port_name_t *uaddr)
{
#ifdef __LP64__
  return copyout_64to32(kaddr, uaddr);
#else /* __LP64__ */
  return copyout(kaddr, uaddr, sizeof(*kaddr));
#endif /* __LP64__ */
}

#if defined(__LP64__) && defined(USER32)
/* For 32 bit userland, kernel and user land messages are not the same size. */
size_t msg_usize(const mach_msg_header_t *kmsg);
#else
static inline size_t msg_usize(const mach_msg_header_t *kmsg)
{
  return kmsg->msgh_size;
}
#endif /* __LP64__ && USER32 */

#endif /* COPY_USER_H */
