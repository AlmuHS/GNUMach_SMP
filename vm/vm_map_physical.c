#include <vm/vm_map_physical.h>
#include <vm/vm_map.h>
#include <vm/vm_kern.h>

// map a physical region
// output: *virt: virtual address
// input: phys: physical address to map (may be unaligned)
// input: size: number of bytes to map
// input: flags: TODO: FIX: might be usable to specify page attributes
//               like PCD (Page Cache Disable), etc.
// returns 0 if ok

long vm_map_physical(vm_offset_t *virt, phys_addr_t phys,
                     unsigned long size, unsigned long flags)
{
  // unused by now
  (void)flags;

  if (size == 0)
  {
    *virt = 0;
    return 0;
  }

  // pad and align to a page boundary
  unsigned long offset = phys & (PAGE_SIZE - 1);
  phys = phys & ~(phys_addr_t)(PAGE_SIZE - 1);
  size = (size + offset + PAGE_SIZE - 1) & ~(unsigned long)(PAGE_SIZE - 1);

  // allocate virtual address
  vm_offset_t virtaddr = 0;
  kern_return_t err = vm_map_enter(kernel_map, &virtaddr, size, 0, TRUE,
                        NULL, 0, FALSE, VM_PROT_READ|VM_PROT_WRITE,
                        VM_PROT_READ|VM_PROT_WRITE, VM_INHERIT_NONE);
  if (err != KERN_SUCCESS)
    return -1;

  phys_addr_t vstart = virtaddr;
  phys_addr_t vend = vstart + size;
  phys_addr_t pstart = phys;
//  phys_addr_t pend = pstart + size;

  // map virtual pages
  while (vstart < vend)
  {
    // map virtual page
    pmap_enter(kernel_pmap, vstart, pstart, VM_PROT_READ|VM_PROT_WRITE, TRUE);
    vstart += PAGE_SIZE;
    pstart += PAGE_SIZE;
  }

  // allow virt to be not in a page-boundary
  *virt = virtaddr + offset;

  return 0;
}
