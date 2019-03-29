#ifndef _VM_VM_MAP_PHYSICAL_H_
#define _VM_VM_MAP_PHYSICAL_H_

#include <mach/machine/vm_types.h>

long vm_map_physical(vm_offset_t *virt, phys_addr_t phys,
                     unsigned long size, unsigned long flags);

#endif
