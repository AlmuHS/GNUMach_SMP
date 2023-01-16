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

#include <string.h>

#include <kern/debug.h>
#include <mach/boolean.h>

#include <copy_user.h>


/* Mach field descriptors measure size in bits */
#define descsize_to_bytes(n) (n / 8)
#define bytes_to_descsize(n) (n * 8)


/*
* Helper to unpack the relevant fields of a msg type; the fields are different
* depending on whether is long form or not.
.*/
static inline vm_size_t unpack_msg_type(vm_offset_t addr,
                                        mach_msg_type_name_t *name,
                                        mach_msg_type_size_t *size,
                                        mach_msg_type_number_t *number,
                                        boolean_t *is_inline)
{
  mach_msg_type_t* kmt = (mach_msg_type_t*)addr;
  *is_inline = kmt->msgt_inline;
  if (kmt->msgt_longform)
    {
      mach_msg_type_long_t* kmtl = (mach_msg_type_long_t*)addr;
      *name = kmtl->msgtl_name;
      *size = kmtl->msgtl_size;
      *number = kmtl->msgtl_number;
      return sizeof(mach_msg_type_long_t);
    }
  else
    {
      *name = kmt->msgt_name;
      *size = kmt->msgt_size;
      *number = kmt->msgt_number;
      return sizeof(mach_msg_type_t);
    }
}

/* Optimized version of unpack_msg_type(), including proper copyin() */
static inline int copyin_unpack_msg_type(vm_offset_t uaddr,
                                         vm_offset_t kaddr,
                                         mach_msg_type_name_t *name,
                                         mach_msg_type_size_t *size,
                                         mach_msg_type_number_t *number,
                                         boolean_t *is_inline,
                                         vm_size_t *amount)
{
  mach_msg_type_t *kmt = (mach_msg_type_t*)kaddr;
  if (copyin((void*)uaddr, kmt, sizeof(mach_msg_type_t)))
    return 1;
  *is_inline = kmt->msgt_inline;
  if (kmt->msgt_longform)
    {
      mach_msg_type_long_t* kmtl = (mach_msg_type_long_t*)kaddr;
      if (copyin((void*)uaddr, kmtl, sizeof(mach_msg_type_long_t)))
        return 1;
      *name = kmtl->msgtl_name;
      *size = kmtl->msgtl_size;
      *number = kmtl->msgtl_number;
      *amount = sizeof(mach_msg_type_long_t);
    }
  else
    {
      *name = kmt->msgt_name;
      *size = kmt->msgt_size;
      *number = kmt->msgt_number;
      *amount = sizeof(mach_msg_type_t);
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
          vm_size_t amount;
          mach_msg_type_name_t name;
          mach_msg_type_size_t size;
          mach_msg_type_number_t number;
          boolean_t is_inline;
          amount = unpack_msg_type(saddr, &name, &size, &number, &is_inline);
          saddr += amount;
          usize += amount;

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
                  saddr = msg_align(saddr);
                  usize = msg_align(usize);
                }
            }
          else
            {
              // advance one pointer
              saddr += sizeof(vm_offset_t);
              usize += sizeof(rpc_vm_offset_t);
            }
        }
    }
  return usize;
}

/*
 * Expand the msg header and, if required, the msg body (ports, pointers)
 *
 * To not make the code too compicated, we use the fact that some fields of
 * mach_msg_header have the same size in the kernel and user variant (basically
 * all fields except ports and addresses)
*/
int copyinmsg (const void *userbuf, void *kernelbuf, const size_t usize)
{
  const mach_msg_user_header_t *umsg = userbuf;
  mach_msg_header_t *kmsg = kernelbuf;

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

  vm_offset_t usaddr, ueaddr, ksaddr;
  ksaddr = (vm_offset_t)(kmsg + 1);
  usaddr = (vm_offset_t)(umsg + 1);
  ueaddr = (vm_offset_t)umsg + usize;
  if (usize > sizeof(mach_msg_user_header_t))
    {
      /* check we have at least space for an empty descryptor */
      while (usaddr < (ueaddr - sizeof(mach_msg_type_t)))
        {
          vm_size_t amount;
          mach_msg_type_name_t name;
          mach_msg_type_size_t size;
          mach_msg_type_number_t number;
          boolean_t is_inline;
          usaddr = msg_align(usaddr);
          ksaddr = msg_align(ksaddr);
          if (copyin_unpack_msg_type(usaddr, ksaddr, &name, &size, &number,
                                     &is_inline, &amount))
            return 1;

          // keep a reference to the current field descriptor, we
          // might need to adjust it later depending on the type
          vm_offset_t ktaddr = ksaddr;
          usaddr += amount;
          ksaddr += amount;

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
                  usaddr = msg_align(usaddr);
                  ksaddr = msg_align(ksaddr);
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
              // advance one pointer
              ksaddr += sizeof(vm_offset_t);
              usaddr += sizeof(rpc_vm_offset_t);
            }
        }
    }

  kmsg->msgh_size = sizeof(mach_msg_header_t) + ksaddr - (vm_offset_t)(kmsg + 1);
  kmsg->msgh_size = msg_align(kmsg->msgh_size);
  return 0;
}

int copyoutmsg (const void *kernelbuf, void *userbuf, const size_t ksize)
{
  const mach_msg_header_t *kmsg = kernelbuf;
  mach_msg_user_header_t *umsg = userbuf;

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

  vm_offset_t ksaddr, keaddr, usaddr;
  ksaddr = (vm_offset_t)(kmsg + 1);
  usaddr = (vm_offset_t)(umsg + 1);
  keaddr = ksaddr + ksize - sizeof(mach_msg_header_t);

  if (ksize > sizeof(mach_msg_user_header_t))
    {
      while (ksaddr < keaddr)
        {
          vm_size_t amount;
          mach_msg_type_name_t name;
          mach_msg_type_size_t size;
          mach_msg_type_number_t number;
          boolean_t is_inline;
          usaddr = msg_align(usaddr);
          ksaddr = msg_align(ksaddr);
          amount = unpack_msg_type(ksaddr, &name, &size, &number, &is_inline);
          // TODO: optimize and bring here type adjustment??
          vm_offset_t utaddr=usaddr, ktaddr=ksaddr;
          if (copyout((void*)ksaddr, (void*)usaddr, amount))
            return 1;
          usaddr += amount;
          ksaddr += amount;

          if (is_inline)
            {
              if (MACH_MSG_TYPE_PORT_ANY(name))
                {
                  adjust_msg_type_size(ktaddr, (int)sizeof(mach_port_name_t) - (int)sizeof(mach_port_t));
                  if (copyout((void*)ktaddr, (void*)utaddr, amount))
                    return 1;
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
                  usaddr = msg_align(usaddr);
                  ksaddr = msg_align(ksaddr);
                }
            }
          else
            {
              // out-of-line port arrays are shrinked in ipc_kmsg_copyout_body()
              if (MACH_MSG_TYPE_PORT_ANY(name))
                {
                  adjust_msg_type_size(ktaddr, -4);
                  if (copyout((void*)ktaddr, (void*)utaddr, amount))
                    return 1;
                }

              if (copyout_address((vm_offset_t*)ksaddr, (rpc_vm_offset_t*)usaddr))
                return 1;
              // advance one pointer
              ksaddr += sizeof(vm_offset_t);
              usaddr += sizeof(rpc_vm_offset_t);
            }
        }
    }

  mach_msg_size_t usize;
  usize = sizeof(mach_msg_user_header_t) + usaddr - (vm_offset_t)(umsg + 1);
  usize = msg_align(usize);
  if (copyout(&usize, &umsg->msgh_size, sizeof(kmsg->msgh_size)))
    return 1;

  return 0;

}
