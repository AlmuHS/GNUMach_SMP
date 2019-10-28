#ifndef _KERN_RAMDISK_H_
#define _KERN_RAMDISK_H_

#include <vm/pmap.h>
#include <device/io_req.h>
#include <device/conf.h>

/* Maximum number of ramdisk devices */
#define RAMDISK_MAX 4

/* The block size used (userspace requires 512) */
#define RAMDISK_BLOCKSZ 512

/* Name associated to the ramdisk major */
#define RAMDISK_NAME "rd"
#define RAMDISK_NAMESZ (sizeof RAMDISK_NAME + sizeof (int) * 3 + 1)

/* Create a new ramdisk of the given size. On success, if out_no and/or out_ptr
 * are not NULL, the device number and pointer to the ramdisk's data are stored
 * there. Returns D_SUCCESS or D_NO_MEMORY.  */
int ramdisk_create(vm_size_t size, const void *initdata, int *out_no);

/* Device operations */
int ramdisk_open(dev_t, int, io_req_t);
int ramdisk_getstat(dev_t, dev_flavor_t, dev_status_t, mach_msg_type_number_t *);
int ramdisk_read(dev_t, io_req_t);
int ramdisk_write(dev_t, io_req_t);
vm_offset_t ramdisk_mmap(dev_t, vm_offset_t, vm_prot_t);

/* dev_ops initializer to be used from <machine>/conf.c */
#define RAMDISK_DEV_OPS { \
		.d_name = RAMDISK_NAME, \
		.d_open = ramdisk_open, \
		.d_close = nulldev_close, \
		.d_read = ramdisk_read, \
		.d_write = ramdisk_write, \
		.d_getstat = ramdisk_getstat, \
		.d_setstat = nulldev_setstat, \
		.d_mmap = ramdisk_mmap, \
		.d_async_in = nodev, \
		.d_reset = nulldev, \
		.d_port_death = nulldev_portdeath, \
		.d_subdev = 0, \
		.d_dev_info = nodev, \
	}

#endif
