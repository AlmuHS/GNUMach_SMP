/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	3/89
 */

#include <mach/port.h>
#include <mach/vm_param.h>

#include <kern/queue.h>
#include <kern/slab.h>

#include <device/device_types.h>
#include <device/dev_hdr.h>
#include <device/conf.h>
#include <device/param.h>		/* DEV_BSIZE, as default */

#include <ipc/ipc_port.h>
#include <kern/ipc_kobject.h>

#include <device/device_emul.h>
#include <device/ds_routines.h>

/*
 * Device structure routines: reference counting, port->device.
 */

/*
 * Lookup/enter by device number.
 */
#define	NDEVHASH	8
#define	DEV_NUMBER_HASH(dev)	((dev) & (NDEVHASH-1))
queue_head_t	dev_number_hash_table[NDEVHASH];

/*
 * Lock for device-number to device lookup.
 * Must be held before device-ref_count lock.
 */
decl_simple_lock_data(,
		dev_number_lock)

struct kmem_cache	dev_hdr_cache;

/*
 * Enter device in the number lookup table.
 * The number table lock must be held.
 */
static void
dev_number_enter(const mach_device_t device)
{
	queue_t	q;

	q = &dev_number_hash_table[DEV_NUMBER_HASH(device->dev_number)];
	queue_enter(q, device, mach_device_t, number_chain);
}

/*
 * Remove device from the device-number lookup table.
 * The device-number table lock must be held.
 */
static void
dev_number_remove(const mach_device_t device)
{
	queue_t	q;

	q = &dev_number_hash_table[DEV_NUMBER_HASH(device->dev_number)];
	queue_remove(q, device, mach_device_t, number_chain);
}

/*
 * Lookup a device by device operations and minor number.
 * The number table lock must be held.
 */
static mach_device_t
dev_number_lookup(const dev_ops_t ops, int devnum)
{
	queue_t	q;
	mach_device_t	device;

	q = &dev_number_hash_table[DEV_NUMBER_HASH(devnum)];
	queue_iterate(q, device, mach_device_t, number_chain) {
	    if (device->dev_ops == ops && device->dev_number == devnum) {
		return (device);
	    }
	}
	return (MACH_DEVICE_NULL);
}

/*
 * Look up a device by name, and create the device structure
 * if it does not exist.  Enter it in the dev_number lookup
 * table.
 */
mach_device_t
device_lookup(const char *name)
{
	dev_ops_t	dev_ops;
	int		dev_minor;
	mach_device_t	device;
	mach_device_t	new_device;

	/*
	 * Get the device and unit number from the name.
	 */
	if (!dev_name_lookup(name, &dev_ops, &dev_minor))
	    return (MACH_DEVICE_NULL);

	/*
	 * Look up the device in the hash table.  If it is
	 * not there, enter it.
	 */
	new_device = MACH_DEVICE_NULL;
	simple_lock(&dev_number_lock);
	while ((device = dev_number_lookup(dev_ops, dev_minor))
		== MACH_DEVICE_NULL) {
	    /*
	     * Must unlock to allocate the structure.  If
	     * the structure has appeared after we have allocated,
	     * release the new structure.
	     */
	    if (new_device != MACH_DEVICE_NULL)
		break;	/* allocated */

	    simple_unlock(&dev_number_lock);

	    new_device = (mach_device_t) kmem_cache_alloc(&dev_hdr_cache);
	    simple_lock_init(&new_device->ref_lock);
	    new_device->ref_count = 1;
	    simple_lock_init(&new_device->lock);
	    new_device->state = DEV_STATE_INIT;
	    new_device->flag = 0;
	    new_device->open_count = 0;
	    new_device->io_in_progress = 0;
	    new_device->io_wait = FALSE;
	    new_device->port = IP_NULL;
	    new_device->dev_ops = dev_ops;
	    new_device->dev_number = dev_minor;
	    new_device->bsize = DEV_BSIZE;	/* change later */

	    simple_lock(&dev_number_lock);
	}

	if (device == MACH_DEVICE_NULL) {
	    /*
	     * No existing device structure.  Insert the
	     * new one.
	     */
	    assert(new_device != MACH_DEVICE_NULL);
	    device = new_device;

	    dev_number_enter(device);
	    simple_unlock(&dev_number_lock);
	}
	else {
	    /*
	     * Have existing device.
	     */
	    mach_device_reference(device);
	    simple_unlock(&dev_number_lock);

	    if (new_device != MACH_DEVICE_NULL)
		kmem_cache_free(&dev_hdr_cache, (vm_offset_t)new_device);
	}

	return (device);
}

/*
 * Add a reference to the device.
 */
void
mach_device_reference(mach_device_t device)
{
	simple_lock(&device->ref_lock);
	device->ref_count++;
	simple_unlock(&device->ref_lock);
}

/*
 * Remove a reference to the device, and deallocate the
 * structure if no references are left.
 */
void
mach_device_deallocate(mach_device_t device)
{
	simple_lock(&device->ref_lock);
	if (--device->ref_count > 0) {
	    simple_unlock(&device->ref_lock);
	    return;
	}
	device->ref_count = 1;
	simple_unlock(&device->ref_lock);

	simple_lock(&dev_number_lock);
	simple_lock(&device->ref_lock);
	if (--device->ref_count > 0) {
	    simple_unlock(&device->ref_lock);
	    simple_unlock(&dev_number_lock);
	    return;
	}

	dev_number_remove(device);
	simple_unlock(&device->ref_lock);
	simple_unlock(&dev_number_lock);

	kmem_cache_free(&dev_hdr_cache, (vm_offset_t)device);
}

/*

 */
/*
 * port-to-device lookup routines.
 */

/*
 * Enter a port-to-device mapping.
 */
void
dev_port_enter(mach_device_t device)
{
	mach_device_reference(device);

	ipc_kobject_set(device->port,
			(ipc_kobject_t) &device->dev, IKOT_DEVICE);
	device->dev.emul_data = device;
	{
	  extern struct device_emulation_ops mach_device_emulation_ops;

	  device->dev.emul_ops = &mach_device_emulation_ops;
	}
}

/*
 * Remove a port-to-device mapping.
 */
void
dev_port_remove(mach_device_t device)
{
	ipc_kobject_set(device->port, IKO_NULL, IKOT_NONE);
	mach_device_deallocate(device);
}

/*
 * Lookup a device by its port.
 * Doesn't consume the naked send right; produces a device reference.
 */
device_t
dev_port_lookup(ipc_port_t port)
{
	device_t	device;

	if (!IP_VALID(port))
	    return (DEVICE_NULL);

	ip_lock(port);
	if (ip_active(port) && (ip_kotype(port) == IKOT_DEVICE)) {
	    device = (device_t) port->ip_kobject;
	    if (device->emul_ops->reference)
	      (*device->emul_ops->reference)(device->emul_data);
	}
	else
	    device = DEVICE_NULL;

	ip_unlock(port);
	return (device);
}

/*
 * Get the port for a device.
 * Consumes a device reference; produces a naked send right.
 */
ipc_port_t
convert_device_to_port(device)
	const device_t	device;
{
	if (device == DEVICE_NULL)
	    return IP_NULL;

	return (*device->emul_ops->dev_to_port) (device->emul_data);
}

/*
 * Call a supplied routine on each device, passing it
 * the port as an argument.  If the routine returns TRUE,
 * stop the search and return TRUE.  If none returns TRUE,
 * return FALSE.
 */
boolean_t
dev_map(
	dev_map_fn	routine,
	mach_port_t	port)
{
	int		i;
	queue_t		q;
	mach_device_t	dev, prev_dev;

	for (i = 0, q = &dev_number_hash_table[0];
	     i < NDEVHASH;
	     i++, q++) {
	    prev_dev = MACH_DEVICE_NULL;
	    simple_lock(&dev_number_lock);
	    queue_iterate(q, dev, mach_device_t, number_chain) {
		mach_device_reference(dev);
		simple_unlock(&dev_number_lock);
		if (prev_dev != MACH_DEVICE_NULL)
		    mach_device_deallocate(prev_dev);

		if ((*routine)(dev, port)) {
		    /*
		     * Done
		     */
		    mach_device_deallocate(dev);
		    return (TRUE);
		}

		simple_lock(&dev_number_lock);
		prev_dev = dev;
	    }
	    simple_unlock(&dev_number_lock);
	    if (prev_dev != MACH_DEVICE_NULL)
		mach_device_deallocate(prev_dev);
	}
	return (FALSE);
}

/*
 * Initialization
 */
void
dev_lookup_init(void)
{
	int	i;

	simple_lock_init(&dev_number_lock);

	for (i = 0; i < NDEVHASH; i++)
	    queue_init(&dev_number_hash_table[i]);

	kmem_cache_init(&dev_hdr_cache, "mach_device",
			sizeof(struct mach_device), 0, NULL, 0);
}
