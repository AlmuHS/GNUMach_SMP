#ifndef VM_PRINT_H
#define	VM_PRINT_H

#include <vm/vm_map.h>
#include <machine/db_machdep.h>

/* Debugging: print a map */
extern void vm_map_print(vm_map_t);

/* Pretty-print a copy object for ddb. */
extern void vm_map_copy_print(vm_map_copy_t);

#include <vm/vm_object.h>

extern void vm_object_print(vm_object_t);

#include <vm/vm_page.h>

extern void vm_page_print(vm_page_t);

#endif	/* VM_PRINT_H */

