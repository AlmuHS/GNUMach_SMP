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

#include <stddef.h>
#include <string.h>

#include <kern/debug.h>
#include <mach/boolean.h>

#include <copy_user.h>


/* Mach field descriptors measure size in bits */
#define descsize_to_bytes(n) (n / 8)
#define bytes_to_descsize(n) (n * 8)

#ifdef USER32
/* Versions of mach_msg_type_t and mach_msg_type_long that are expected from the 32 bit userland. */
typedef struct {
  unsigned int msgt_name : 8,
               msgt_size : 8,
               msgt_number : 12,
               msgt_inline : 1,
               msgt_longform : 1,
               msgt_deallocate : 1,
               msgt_unused : 1;
} mach_msg_user_type_t;
_Static_assert(sizeof(mach_msg_user_type_t) == 4);

typedef struct {
  mach_msg_user_type_t msgtl_header;
  unsigned short msgtl_name;
  unsigned short msgtl_size;
  natural_t msgtl_number;
} mach_msg_user_type_long_t;
_Static_assert(sizeof(mach_msg_user_type_long_t) == 12);
#else
typedef mach_msg_type_t mach_msg_user_type_t;
typedef mach_msg_type_long_t mach_msg_user_type_long_t;
#endif  /* USER32 */

/*
* Helper to unpack the relevant fields of a msg type; the fields are different
* depending on whether is long form or not.
.*/
static inline void unpack_msg_type(vm_offset_t addr,
                                   mach_msg_type_name_t *name,
                                   mach_msg_type_size_t *size,
                                   mach_msg_type_number_t *number,
                                   boolean_t *is_inline,
                                   vm_size_t *user_amount,
                                   vm_size_t *kernel_amount)
{
  mach_msg_type_t* kmt = (mach_msg_type_t*)addr;
  *is_inline = kmt->msgt_inline;
  if (kmt->msgt_longform)
    {
      mach_msg_type_long_t* kmtl = (mach_msg_type_long_t*)addr;
      *name = kmtl->msgtl_name;
      *size = kmtl->msgtl_size;
      *number = kmtl->msgtl_number;
      *kernel_amount = sizeof(mach_msg_type_long_t);
      *user_amount = sizeof(mach_msg_user_type_long_t);
    }
  else
    {
      *name = kmt->msgt_name;
      *size = kmt->msgt_size;
      *number = kmt->msgt_number;
      *kernel_amount = sizeof(mach_msg_type_t);
      *user_amount = sizeof(mach_msg_user_type_t);
    }
}

#ifdef USER32
static inline void mach_msg_user_type_to_kernel(const mach_msg_user_type_t *u,
    mach_msg_type_t* k) {
  k->msgt_name = u->msgt_name;
  k->msgt_size = u->msgt_size;
  k->msgt_number = u->msgt_number;
  k->msgt_inline = u->msgt_inline;
  k->msgt_longform = u->msgt_longform;
  k->msgt_deallocate = u->msgt_deallocate;
  k->msgt_unused = 0;
}

static inline void mach_msg_user_type_to_kernel_long(const mach_msg_user_type_long_t *u,
    mach_msg_type_long_t* k) {
  const mach_msg_type_long_t kernel = {
    .msgtl_header = {
      .msgt_name = u->msgtl_name,
      .msgt_size = u->msgtl_size,
      .msgt_number = u->msgtl_number,
      .msgt_inline = u->msgtl_header.msgt_inline,
      .msgt_longform = u->msgtl_header.msgt_longform,
      .msgt_deallocate = u->msgtl_header.msgt_deallocate,
      .msgt_unused = 0
    }
  };
  *k = kernel;
}

static inline void mach_msg_kernel_type_to_user(const mach_msg_type_t *k,
    mach_msg_user_type_t *u) {
  u->msgt_name = k->msgt_name;
  u->msgt_size = k->msgt_size;
  u->msgt_number = k->msgt_number;
  u->msgt_inline = k->msgt_inline;
  u->msgt_longform = k->msgt_longform;
  u->msgt_deallocate = k->msgt_deallocate;
  u->msgt_unused = 0;
}

static inline void mach_msg_kernel_type_to_user_long(const mach_msg_type_long_t *k,
    mach_msg_user_type_long_t *u) {
  const mach_msg_user_type_long_t user = {
    .msgtl_header = {
      .msgt_name = 0,
      .msgt_size = 0,
      .msgt_number = 0,
      .msgt_inline = k->msgtl_header.msgt_inline,
      .msgt_longform = k->msgtl_header.msgt_longform,
      .msgt_deallocate = k->msgtl_header.msgt_deallocate,
      .msgt_unused = 0
    },
    .msgtl_name = k->msgtl_header.msgt_name,
    .msgtl_size = k->msgtl_header.msgt_size,
    .msgtl_number = k->msgtl_header.msgt_number
  };
  *u = user;
}
#endif

static inline int copyin_mach_msg_type(const rpc_vm_offset_t *uaddr, mach_msg_type_t *kaddr) {
#ifdef USER32
  mach_msg_user_type_t user;
  int ret = copyin(uaddr, &user, sizeof(mach_msg_user_type_t));
  if (ret) {
    return ret;
  }
  mach_msg_user_type_to_kernel(&user, kaddr);
  return 0;
#else
  return copyin(uaddr, kaddr, sizeof(mach_msg_type_t));
#endif
}

static inline int copyout_mach_msg_type(const mach_msg_type_t *kaddr, rpc_vm_offset_t  *uaddr) {
#ifdef USER32
  mach_msg_user_type_t user;
  mach_msg_kernel_type_to_user(kaddr, &user);
  return copyout(&user, uaddr, sizeof(mach_msg_user_type_t));
#else
  return copyout(kaddr, uaddr, sizeof(mach_msg_type_t));
#endif
}

static inline int copyin_mach_msg_type_long(const rpc_vm_offset_t *uaddr, mach_msg_type_long_t *kaddr) {
#ifdef USER32
  mach_msg_user_type_long_t user;
  int ret = copyin(uaddr, &user, sizeof(mach_msg_user_type_long_t));
  if (ret)
    return ret;
  mach_msg_user_type_to_kernel_long(&user, kaddr);
  return 0;
#else
  return copyin(uaddr, kaddr, sizeof(mach_msg_type_long_t));
#endif
}

static inline int copyout_mach_msg_type_long(const mach_msg_type_long_t *kaddr, rpc_vm_offset_t *uaddr) {
#ifdef USER32
  mach_msg_user_type_long_t user;
  mach_msg_kernel_type_to_user_long(kaddr, &user);
  return copyout(&user, uaddr, sizeof(mach_msg_user_type_long_t));
#else
  return copyout(kaddr, uaddr, sizeof(mach_msg_type_long_t));
#endif
}

/* Optimized version of unpack_msg_type(), including proper copyin() */
static inline int copyin_unpack_msg_type(vm_offset_t uaddr,
                                         vm_offset_t kaddr,
                                         mach_msg_type_name_t *name,
                                         mach_msg_type_size_t *size,
                                         mach_msg_type_number_t *number,
                                         boolean_t *is_inline,
                                         vm_size_t *user_amount,
                                         vm_size_t *kernel_amount)
{
  mach_msg_type_t *kmt = (mach_msg_type_t*)kaddr;
  if (copyin_mach_msg_type((void *)uaddr, kmt))
    return 1;
  *is_inline = kmt->msgt_inline;
  if (kmt->msgt_longform)
    {
      mach_msg_type_long_t* kmtl = (mach_msg_type_long_t*)kaddr;
      if (copyin_mach_msg_type_long((void *)uaddr, kmtl))
        return 1;
      *name = kmtl->msgtl_name;
      *size = kmtl->msgtl_size;
      *number = kmtl->msgtl_number;
      *user_amount = sizeof(mach_msg_user_type_long_t);
      *kernel_amount = sizeof(mach_msg_type_long_t);
    }
  else
    {
      *name = kmt->msgt_name;
      *size = kmt->msgt_size;
      *number = kmt->msgt_number;
      *user_amount = sizeof(mach_msg_user_type_t);
      *kernel_amount = sizeof(mach_msg_type_t);
    }
  return 0;
}

/*
 * The msg type has a different size field depending on whether is long or not,
 * and we also need to convert from bytes to bits
 */
static inline void adjust_msg_type_size(vm_offset_t addr, int amount)
{
  mach_msg_type_t* kmt = (mach_msg_type_t*)addr;
  if (kmt->msgt_longform)
    {
      mach_msg_type_long_t* kmtl = (mach_msg_type_long_t*)addr;
      kmtl->msgtl_size += bytes_to_descsize(amount);
    }
  else
    {
      kmt->msgt_size += bytes_to_descsize(amount);
    }
}

/* Optimized version of unpack_msg_type(), including proper copyout() */
static inline int copyout_unpack_msg_type(vm_offset_t kaddr,
                                          vm_offset_t uaddr,
                                          mach_msg_type_name_t *name,
                                          mach_msg_type_size_t *size,
                                          mach_msg_type_number_t *number,
                                          boolean_t *is_inline,
                                          vm_size_t *user_amount,
                                          vm_size_t *kernel_amount)
{
  mach_msg_type_t *kmt = (mach_msg_type_t*)kaddr;
  *is_inline = kmt->msgt_inline;
  if (kmt->msgt_longform)
    {
      mach_msg_type_long_t* kmtl = (mach_msg_type_long_t*)kaddr;
      mach_msg_type_size_t orig_size = kmtl->msgtl_size;
      int ret;

      if (MACH_MSG_TYPE_PORT_ANY(kmtl->msgtl_name))
        kmtl->msgtl_size = bytes_to_descsize(sizeof(mach_port_name_t));
      ret = copyout_mach_msg_type_long(kmtl, (void*)uaddr);
      kmtl->msgtl_size = orig_size;
      if (ret)
        return 1;

      *name = kmtl->msgtl_name;
      *size = kmtl->msgtl_size;
      *number = kmtl->msgtl_number;
      *user_amount = sizeof(mach_msg_user_type_long_t);
      *kernel_amount = sizeof(mach_msg_type_long_t);
    }
  else
    {
      mach_msg_type_size_t orig_size = kmt->msgt_size;
      int ret;

      if (MACH_MSG_TYPE_PORT_ANY(kmt->msgt_name))
        kmt->msgt_size = bytes_to_descsize(sizeof(mach_port_name_t));
      ret = copyout_mach_msg_type(kmt, (void *)uaddr);
      kmt->msgt_size = orig_size;
      if (ret)
        return 1;

      *name = kmt->msgt_name;
      *size = kmt->msgt_size;
      *number = kmt->msgt_number;
      *user_amount = sizeof(mach_msg_user_type_t);
      *kernel_amount = sizeof(mach_msg_type_t);
    }
  return 0;
}

/*
 * Compute the user-space size of a message still in the kernel.
 * The message may be originating from userspace (in which case we could
 * optimize this by keeping the usize around) or from kernel space (we could
 * optimize if the message structure is fixed and known in advance).
 * For now just handle the most general case, iterating over the msg body.
 */
size_t msg_usize(const mach_msg_header_t *kmsg)
{
  size_t ksize = kmsg->msgh_size;
  size_t usize = sizeof(mach_msg_user_header_t);
  if (ksize > sizeof(mach_msg_header_t))
    {
      // iterate over body compute the user-space message size
      vm_offset_t saddr, eaddr;
      saddr = (vm_offset_t)(kmsg + 1);
      eaddr = saddr + ksize - sizeof(mach_msg_header_t);
      while (saddr < (eaddr - sizeof(mach_msg_type_t)))
        {
          vm_size_t user_amount, kernel_amount;
          mach_msg_type_name_t name;
          mach_msg_type_size_t size;
          mach_msg_type_number_t number;
          boolean_t is_inline;
          unpack_msg_type(saddr, &name, &size, &number, &is_inline, &user_amount, &kernel_amount);
          saddr += kernel_amount;
          saddr = mach_msg_kernel_align(saddr);
          usize += user_amount;
          usize = mach_msg_user_align(usize);

          if (is_inline)
            {
              if (MACH_MSG_TYPE_PORT_ANY(name))
                {
                  saddr += sizeof(mach_port_t) * number;
                  usize += sizeof(mach_port_name_t) * number;
                }
              else
                {
                  size_t n = descsize_to_bytes(size);
                  saddr += n*number;
                  usize += n*number;
                }
            }
          else
            {
              // advance one pointer
              saddr += sizeof(vm_offset_t);
              usize += sizeof(rpc_vm_offset_t);
            }
          saddr = mach_msg_kernel_align(saddr);
          usize = mach_msg_user_align(usize);
        }
    }
  return usize;
}

/*
 * Expand the msg header and, if required, the msg body (ports, pointers)
 *
 * To not make the code too complicated, we use the fact that some fields of
 * mach_msg_header have the same size in the kernel and user variant (basically
 * all fields except ports and addresses)
*/
int copyinmsg (const void *userbuf, void *kernelbuf, const size_t usize, const size_t ksize)
{
  const mach_msg_user_header_t *umsg = userbuf;
  mach_msg_header_t *kmsg = kernelbuf;

#ifdef USER32
  if (copyin(&umsg->msgh_bits, &kmsg->msgh_bits, sizeof(kmsg->msgh_bits)))
    return 1;
  /* kmsg->msgh_size is filled in later */
  if (copyin_port(&umsg->msgh_remote_port, &kmsg->msgh_remote_port))
    return 1;
  if (copyin_port(&umsg->msgh_local_port, &kmsg->msgh_local_port))
    return 1;
  if (copyin(&umsg->msgh_seqno, &kmsg->msgh_seqno,
             sizeof(kmsg->msgh_seqno) + sizeof(kmsg->msgh_id)))
    return 1;
#else
  /* The 64 bit interface ensures the header is the same size, so it does not need any resizing. */
  _Static_assert(sizeof(mach_msg_header_t) == sizeof(mach_msg_user_header_t),
		 "mach_msg_header_t and mach_msg_user_header_t expected to be of the same size");
  if (copyin(umsg, kmsg, sizeof(mach_msg_header_t)))
    return 1;
  kmsg->msgh_remote_port &= 0xFFFFFFFF; // FIXME: still have port names here
  kmsg->msgh_local_port &= 0xFFFFFFFF;  // also, this assumes little-endian
#endif

  vm_offset_t usaddr, ueaddr, ksaddr;
  ksaddr = (vm_offset_t)(kmsg + 1);
  usaddr = (vm_offset_t)(umsg + 1);
  ueaddr = (vm_offset_t)umsg + usize;

  _Static_assert(!mach_msg_user_is_misaligned(sizeof(mach_msg_user_header_t)),
                 "mach_msg_user_header_t needs to be MACH_MSG_USER_ALIGNMENT aligned.");

  if (usize > sizeof(mach_msg_user_header_t))
    {
      /* check we have at least space for an empty descryptor */
      while (usaddr <= (ueaddr - sizeof(mach_msg_user_type_t)))
        {
          vm_size_t user_amount, kernel_amount;
          mach_msg_type_name_t name;
          mach_msg_type_size_t size;
          mach_msg_type_number_t number;
          boolean_t is_inline;
          if (copyin_unpack_msg_type(usaddr, ksaddr, &name, &size, &number,
                                     &is_inline, &user_amount, &kernel_amount))
            return 1;

          // keep a reference to the current field descriptor, we
          // might need to adjust it later depending on the type
          vm_offset_t ktaddr = ksaddr;
          usaddr += user_amount;
          usaddr = mach_msg_user_align(usaddr);
          ksaddr += kernel_amount;
          ksaddr = mach_msg_kernel_align(ksaddr);

          if (is_inline)
            {
              if (MACH_MSG_TYPE_PORT_ANY(name))
                {
                  if ((usaddr + sizeof(mach_port_name_t)*number) > ueaddr)
                    return 1;
                  adjust_msg_type_size(ktaddr, sizeof(mach_port_t) - sizeof(mach_port_name_t));
                  for (int i=0; i<number; i++)
                    {
                      if (copyin_port((mach_port_name_t*)usaddr, (mach_port_t*)ksaddr))
                        return 1;
                      ksaddr += sizeof(mach_port_t);
                      usaddr += sizeof(mach_port_name_t);
                    }
                }
              else
                {
                  // type that doesn't need change
                  size_t n = descsize_to_bytes(size);
                  if ((usaddr + n*number) > ueaddr)
                    return 1;
                  if (copyin((void*)usaddr, (void*)ksaddr, n*number))
                    return 1;
                  usaddr += n*number;
                  ksaddr += n*number;
                }
            }
          else
            {
              if ((usaddr + sizeof(rpc_vm_offset_t)) > ueaddr)
                return 1;

              // out-of-line port arrays are expanded in ipc_kmsg_copyin_body()
              if (MACH_MSG_TYPE_PORT_ANY(name))
                adjust_msg_type_size(ktaddr, sizeof(mach_port_t) - sizeof(mach_port_name_t));

              if (copyin_address((rpc_vm_offset_t*)usaddr, (vm_offset_t*)ksaddr))
                return 1;
              // Advance one pointer.
              ksaddr += sizeof(vm_offset_t);
              usaddr += sizeof(rpc_vm_offset_t);
            }
          // Note that we have to align because mach_port_name_t might not align
          // with the required user alignment.
          usaddr = mach_msg_user_align(usaddr);
          ksaddr = mach_msg_kernel_align(ksaddr);
        }
    }

  kmsg->msgh_size = sizeof(mach_msg_header_t) + ksaddr - (vm_offset_t)(kmsg + 1);
  assert(kmsg->msgh_size <= ksize);
  return 0;
}

int copyoutmsg (const void *kernelbuf, void *userbuf, const size_t ksize)
{
  const mach_msg_header_t *kmsg = kernelbuf;
  mach_msg_user_header_t *umsg = userbuf;
#ifdef USER32
  if (copyout(&kmsg->msgh_bits, &umsg->msgh_bits, sizeof(kmsg->msgh_bits)))
    return 1;
  /* umsg->msgh_size is filled in later */
  if (copyout_port(&kmsg->msgh_remote_port, &umsg->msgh_remote_port))
    return 1;
  if (copyout_port(&kmsg->msgh_local_port, &umsg->msgh_local_port))
    return 1;
  if (copyout(&kmsg->msgh_seqno, &umsg->msgh_seqno,
             sizeof(kmsg->msgh_seqno) + sizeof(kmsg->msgh_id)))
    return 1;
#else
  if (copyout(kmsg, umsg, sizeof(mach_msg_header_t)))
    return 1;
#endif  /* USER32 */

  vm_offset_t ksaddr, keaddr, usaddr;
  ksaddr = (vm_offset_t)(kmsg + 1);
  usaddr = (vm_offset_t)(umsg + 1);
  keaddr = ksaddr + ksize - sizeof(mach_msg_header_t);

  if (ksize > sizeof(mach_msg_header_t))
    {
      while (ksaddr < keaddr)
        {
          vm_size_t user_amount, kernel_amount;
          mach_msg_type_name_t name;
          mach_msg_type_size_t size;
          mach_msg_type_number_t number;
          boolean_t is_inline;
          if (copyout_unpack_msg_type(ksaddr, usaddr, &name, &size, &number,
                                      &is_inline, &user_amount, &kernel_amount))
            return 1;
          usaddr += user_amount;
          usaddr = mach_msg_user_align(usaddr);
          ksaddr += kernel_amount;
          ksaddr = mach_msg_kernel_align(ksaddr);

          if (is_inline)
            {
              if (MACH_MSG_TYPE_PORT_ANY(name))
                {
                  for (int i=0; i<number; i++)
                    {
                      if (copyout_port((mach_port_t*)ksaddr, (mach_port_name_t*)usaddr))
                        return 1;
                      ksaddr += sizeof(mach_port_t);
                      usaddr += sizeof(mach_port_name_t);
                    }
                }
              else
                {
                  // type that doesn't need change
                  size_t n = descsize_to_bytes(size);
                  if (copyout((void*)ksaddr, (void*)usaddr, n*number))
                    return 1;
                  usaddr += n*number;
                  ksaddr += n*number;
                }
            }
          else
            {
              if (copyout_address((vm_offset_t*)ksaddr, (rpc_vm_offset_t*)usaddr))
                return 1;
              // advance one pointer
              ksaddr += sizeof(vm_offset_t);
              usaddr += sizeof(rpc_vm_offset_t);
            }
          usaddr = mach_msg_user_align(usaddr);
          ksaddr = mach_msg_kernel_align(ksaddr);
        }
    }

  mach_msg_size_t usize;
  usize = sizeof(mach_msg_user_header_t) + usaddr - (vm_offset_t)(umsg + 1);
  if (copyout(&usize, &umsg->msgh_size, sizeof(umsg->msgh_size)))
    return 1;

  return 0;

}
