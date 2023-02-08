/*
 * Mach Operating System
 * Copyright (c) 1993-1989 Carnegie Mellon University
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
 *
 * 	Device pager.
 */

#include <string.h>

#include <mach/boolean.h>
#include <mach/port.h>
#include <mach/message.h>
#include <mach/std_types.h>
#include <mach/mach_types.h>

#include <ipc/ipc_port.h>
#include <ipc/ipc_space.h>

#include <kern/debug.h>
#include <kern/printf.h>
#include <kern/queue.h>
#include <kern/slab.h>

#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/vm_user.h>

#include <device/device_pager.server.h>
#include <device/device_types.h>
#include <device/ds_routines.h>
#include <device/dev_hdr.h>
#include <device/io_req.h>
#include <device/memory_object_reply.user.h>
#include <device/dev_pager.h>
#include <device/blkio.h>
#include <device/conf.h>

/*
 *	The device pager routines are called directly from the message
 *	system (via mach_msg), and thus run in the kernel-internal
 *	environment.  All ports are in internal form (ipc_port_t),
 *	and must be correctly reference-counted in order to be saved
 *	in other data structures.  Kernel routines may be called
 *	directly.  Kernel types are used for data objects (tasks,
 *	memory objects, ports).  The only IPC routines that may be
 *	called are ones that masquerade as the kernel task (via
 *	msg_send_from_kernel).
 *
 *	Port rights and references are maintained as follows:
 *		Memory object port:
 *			The device_pager task has all rights.
 *		Memory object control port:
 *			The device_pager task has only send rights.
 *		Memory object name port:
 *			The device_pager task has only send rights.
 *			The name port is not even recorded.
 *	Regardless how the object is created, the control and name
 *	ports are created by the kernel and passed through the memory
 *	management interface.
 *
 *	The device_pager assumes that access to its memory objects
 *	will not be propagated to more that one host, and therefore
 *	provides no consistency guarantees beyond those made by the
 *	kernel.
 *
 *	In the event that more than one host attempts to use a device
 *	memory object, the device_pager will only record the last set
 *	of port names.  [This can happen with only one host if a new
 *	mapping is being established while termination of all previous
 *	mappings is taking place.]  Currently, the device_pager assumes
 *	that its clients adhere to the initialization and termination
 *	protocols in the memory management interface; otherwise, port
 *	rights or out-of-line memory from erroneous messages may be
 *	allowed to accumulate.
 *
 *	[The phrase "currently" has been used above to denote aspects of
 *	the implementation that could be altered without changing the rest
 *	of the basic documentation.]
 */

/*
 * Basic device pager structure.
 */
struct dev_pager {
	decl_simple_lock_data(, lock)	/* lock for reference count */
	int		ref_count;	/* reference count */
	int		client_count;	/* How many memory_object_create
					 * calls have we received */
	ipc_port_t	pager;		/* pager port */
	ipc_port_t	pager_request;	/* Known request port */
	ipc_port_t	pager_name;	/* Known name port */
	mach_device_t	device;		/* Device handle */
	vm_offset_t	offset;		/* offset within the pager, in bytes*/
	int		type;		/* to distinguish */
#define DEV_PAGER_TYPE	0
#define CHAR_PAGER_TYPE	1
	/* char pager specifics */
	int		prot;
	vm_size_t	size;
};
typedef struct dev_pager *dev_pager_t;
#define	DEV_PAGER_NULL	((dev_pager_t)0)


struct kmem_cache	dev_pager_cache;

static void dev_pager_reference(dev_pager_t ds)
{
	simple_lock(&ds->lock);
	ds->ref_count++;
	simple_unlock(&ds->lock);
}

static void dev_pager_deallocate(dev_pager_t ds)
{
	simple_lock(&ds->lock);
	if (--ds->ref_count > 0) {
	    simple_unlock(&ds->lock);
	    return;
	}

	simple_unlock(&ds->lock);
	kmem_cache_free(&dev_pager_cache, (vm_offset_t)ds);
}

/*
 * A hash table of ports for device_pager backed objects.
 */

#define	DEV_HASH_COUNT		127

struct dev_pager_entry {
	queue_chain_t	links;
	ipc_port_t	name;
	dev_pager_t	pager_rec;
};
typedef struct dev_pager_entry *dev_pager_entry_t;

/*
 * Indexed by port name, each element contains a queue of all dev_pager_entry_t
 * which name shares the same hash
 */
queue_head_t	dev_pager_hashtable[DEV_HASH_COUNT];
struct kmem_cache	dev_pager_hash_cache;
def_simple_lock_data(static, dev_pager_hash_lock)

struct dev_device_entry {
	queue_chain_t	links;
	mach_device_t	device;
	vm_offset_t	offset;
	dev_pager_t	pager_rec;
};
typedef struct dev_device_entry *dev_device_entry_t;

/*
 * Indexed by device + offset, each element contains a queue of all
 * dev_device_entry_t which device + offset shares the same hash
 */
queue_head_t	dev_device_hashtable[DEV_HASH_COUNT];
struct kmem_cache	dev_device_hash_cache;
def_simple_lock_data(static, dev_device_hash_lock)

#define	dev_hash(name_port) \
		(((vm_offset_t)(name_port) & 0xffffff) % DEV_HASH_COUNT)

static void dev_pager_hash_init(void)
{
	int		i;
	vm_size_t	size;

	size = sizeof(struct dev_pager_entry);
	kmem_cache_init(&dev_pager_hash_cache, "dev_pager_entry", size, 0,
			NULL, 0);
	for (i = 0; i < DEV_HASH_COUNT; i++)
	    queue_init(&dev_pager_hashtable[i]);
	simple_lock_init(&dev_pager_hash_lock);
}

static void dev_pager_hash_insert(
	const ipc_port_t	name_port,
	const dev_pager_t	rec)
{
	dev_pager_entry_t new_entry;

	new_entry = (dev_pager_entry_t) kmem_cache_alloc(&dev_pager_hash_cache);
	new_entry->name = name_port;
	new_entry->pager_rec = rec;

	simple_lock(&dev_pager_hash_lock);
	queue_enter(&dev_pager_hashtable[dev_hash(name_port)],
			new_entry, dev_pager_entry_t, links);
	simple_unlock(&dev_pager_hash_lock);
}

static void dev_pager_hash_delete(const ipc_port_t name_port)
{
	queue_t			bucket;
	dev_pager_entry_t	entry;

	bucket = &dev_pager_hashtable[dev_hash(name_port)];

	simple_lock(&dev_pager_hash_lock);
	for (entry = (dev_pager_entry_t)queue_first(bucket);
	     !queue_end(bucket, &entry->links);
	     entry = (dev_pager_entry_t)queue_next(&entry->links)) {
	    if (entry->name == name_port) {
		queue_remove(bucket, entry, dev_pager_entry_t, links);
		break;
	    }
	}
	simple_unlock(&dev_pager_hash_lock);
	if (!queue_end(bucket, &entry->links))
	    kmem_cache_free(&dev_pager_hash_cache, (vm_offset_t)entry);
}

static dev_pager_t dev_pager_hash_lookup(const ipc_port_t name_port)
{
	queue_t			bucket;
	dev_pager_entry_t	entry;
	dev_pager_t		pager;

	bucket = &dev_pager_hashtable[dev_hash(name_port)];

	simple_lock(&dev_pager_hash_lock);
	for (entry = (dev_pager_entry_t)queue_first(bucket);
	     !queue_end(bucket, &entry->links);
	     entry = (dev_pager_entry_t)queue_next(&entry->links)) {
	    if (entry->name == name_port) {
		pager = entry->pager_rec;
		dev_pager_reference(pager);
		simple_unlock(&dev_pager_hash_lock);
		return (pager);
	    }
	}
	simple_unlock(&dev_pager_hash_lock);
	return (DEV_PAGER_NULL);
}

static void dev_device_hash_init(void)
{
	int		i;
	vm_size_t	size;

	size = sizeof(struct dev_device_entry);
	kmem_cache_init(&dev_device_hash_cache, "dev_device_entry", size, 0,
			NULL, 0);
	for (i = 0; i < DEV_HASH_COUNT; i++) {
	    queue_init(&dev_device_hashtable[i]);
	}
	simple_lock_init(&dev_device_hash_lock);
}

static void dev_device_hash_insert(
	const mach_device_t	device,
	const vm_offset_t	offset,
	const dev_pager_t	rec)
{
	dev_device_entry_t new_entry;

	new_entry = (dev_device_entry_t) kmem_cache_alloc(&dev_device_hash_cache);
	new_entry->device = device;
	new_entry->offset = offset;
	new_entry->pager_rec = rec;

	simple_lock(&dev_device_hash_lock);
	queue_enter(&dev_device_hashtable[dev_hash(device + offset)],
			new_entry, dev_device_entry_t, links);
	simple_unlock(&dev_device_hash_lock);
}

static void dev_device_hash_delete(
	const mach_device_t	device,
	const vm_offset_t	offset)
{
	queue_t			bucket;
	dev_device_entry_t	entry;

	bucket = &dev_device_hashtable[dev_hash(device + offset)];

	simple_lock(&dev_device_hash_lock);
	for (entry = (dev_device_entry_t)queue_first(bucket);
	     !queue_end(bucket, &entry->links);
	     entry = (dev_device_entry_t)queue_next(&entry->links)) {
	    if (entry->device == device && entry->offset == offset) {
		queue_remove(bucket, entry, dev_device_entry_t, links);
		break;
	    }
	}
	simple_unlock(&dev_device_hash_lock);
	if (!queue_end(bucket, &entry->links))
	    kmem_cache_free(&dev_device_hash_cache, (vm_offset_t)entry);
}

static dev_pager_t dev_device_hash_lookup(
	const mach_device_t	device,
	const vm_offset_t	offset)
{
	queue_t			bucket;
	dev_device_entry_t	entry;
	dev_pager_t		pager;

	bucket = &dev_device_hashtable[dev_hash(device + offset)];

	simple_lock(&dev_device_hash_lock);
	for (entry = (dev_device_entry_t)queue_first(bucket);
	     !queue_end(bucket, &entry->links);
	     entry = (dev_device_entry_t)queue_next(&entry->links)) {
	    if (entry->device == device && entry->offset == offset) {
		pager = entry->pager_rec;
		dev_pager_reference(pager);
		simple_unlock(&dev_device_hash_lock);
		return (pager);
	    }
	}
	simple_unlock(&dev_device_hash_lock);
	return (DEV_PAGER_NULL);
}

kern_return_t	device_pager_setup(
	const mach_device_t	device,
	int			prot,
	vm_offset_t		offset,
	vm_size_t		size,
	mach_port_t		*pager)
{
	dev_pager_t	d;

	/*
	 *	Verify the device is indeed mappable
	 */
	if (!device->dev_ops->d_mmap || (device->dev_ops->d_mmap == nomap))
		return (D_INVALID_OPERATION);

	/*
	 *	Allocate a structure to hold the arguments
	 *	and port to represent this object.
	 */

	d = dev_device_hash_lookup(device, offset);
	if (d != DEV_PAGER_NULL) {
		*pager = (mach_port_t) ipc_port_make_send(d->pager);
		dev_pager_deallocate(d);
		return (D_SUCCESS);
	}

	d = (dev_pager_t) kmem_cache_alloc(&dev_pager_cache);
	if (d == DEV_PAGER_NULL)
		return (KERN_RESOURCE_SHORTAGE);

	simple_lock_init(&d->lock);
	d->ref_count = 1;

	/*
	 * Allocate the pager port.
	 */
	d->pager = ipc_port_alloc_kernel();
	if (d->pager == IP_NULL) {
		dev_pager_deallocate(d);
		return (KERN_RESOURCE_SHORTAGE);
	}

	d->client_count = 0;
	d->pager_request = IP_NULL;
	d->pager_name = IP_NULL;
	d->device = device;
	mach_device_reference(device);
	d->offset = offset;
	d->prot = prot;
	d->size = round_page(size);
	if (device->dev_ops->d_mmap == block_io_mmap) {
		d->type = DEV_PAGER_TYPE;
	} else {
		d->type = CHAR_PAGER_TYPE;
	}

	dev_pager_hash_insert(d->pager, d);
	dev_device_hash_insert(d->device, d->offset, d);

	*pager = (mach_port_t) ipc_port_make_send(d->pager);
	return (KERN_SUCCESS);
}

boolean_t	device_pager_debug = FALSE;

kern_return_t	device_pager_data_request(
	const ipc_port_t	pager,
	const ipc_port_t	pager_request,
	vm_offset_t		offset,
	vm_size_t		length,
	vm_prot_t		protection_required)
{
	dev_pager_t	ds;
	kern_return_t	ret;

	if (device_pager_debug)
		printf("(device_pager)data_request: pager=%p, offset=0x%lx, length=0x%lx\n",
			pager, (unsigned long) offset, (unsigned long) length);

	ds = dev_pager_hash_lookup(pager);
	if (ds == DEV_PAGER_NULL)
		panic("(device_pager)data_request: lookup failed");

	if (ds->pager_request != pager_request)
		panic("(device_pager)data_request: bad pager_request");

	if (ds->type == CHAR_PAGER_TYPE) {
	    vm_object_t			object;

	    object = vm_object_lookup(pager_request);
	    if (object == VM_OBJECT_NULL) {
		    (void) r_memory_object_data_error(pager_request,
						      offset, length,
						      KERN_FAILURE);
		    dev_pager_deallocate(ds);
		    return (KERN_SUCCESS);
	    }

	    ret = vm_object_page_map(object,
				     offset, length,
				     device_map_page, (void *)ds);

	    if (ret != KERN_SUCCESS) {
		    (void) r_memory_object_data_error(pager_request,
						      offset, length,
						      ret);
		    vm_object_deallocate(object);
		    dev_pager_deallocate(ds);
		    return (KERN_SUCCESS);
	    }

	    vm_object_deallocate(object);
	}
	else {
	    panic("(device_pager)data_request: dev pager");
	}

	dev_pager_deallocate(ds);

	return (KERN_SUCCESS);
}

kern_return_t device_pager_copy(
	const ipc_port_t	pager,
	const ipc_port_t	pager_request,
	vm_offset_t		offset,
	vm_size_t		length,
	const ipc_port_t	new_pager)
{
	panic("(device_pager)copy: called");
}

kern_return_t
device_pager_supply_completed(
	const ipc_port_t pager,
	const ipc_port_t pager_request,
	vm_offset_t offset,
	vm_size_t length,
	kern_return_t result,
	vm_offset_t error_offset)
{
	panic("(device_pager)supply_completed: called");
}

kern_return_t
device_pager_data_return(
	const ipc_port_t	pager,
	const ipc_port_t	pager_request,
	vm_offset_t		offset,
	pointer_t		addr,
	mach_msg_type_number_t	data_cnt,
	boolean_t		dirty,
	boolean_t		kernel_copy)
{
	panic("(device_pager)data_return: called");
}

kern_return_t
device_pager_change_completed(
	const ipc_port_t pager,
	boolean_t may_cache,
	memory_object_copy_strategy_t copy_strategy)
{
	panic("(device_pager)change_completed: called");
}

/*
 *	The mapping function takes a byte offset, but returns
 *	a machine-dependent page frame number.  We convert
 *	that into something that the pmap module will
 *	accept later.
 */
phys_addr_t device_map_page(
	void		*dsp,
	vm_offset_t	offset)
{
	dev_pager_t	ds = (dev_pager_t) dsp;
	vm_offset_t	pagenum =
		   (*(ds->device->dev_ops->d_mmap))
			(ds->device->dev_number,
			ds->offset + offset,
			ds->prot);

	if (pagenum == -1)
		return vm_page_fictitious_addr;

	return pmap_phys_address(pagenum);
}

kern_return_t device_pager_init_pager(
	const ipc_port_t	pager,
	const ipc_port_t	pager_request,
	const ipc_port_t	pager_name,
	vm_size_t		pager_page_size)
{
	dev_pager_t	ds;

	if (device_pager_debug)
		printf("(device_pager)init: pager=%p, request=%p, name=%p\n",
		       pager, pager_request, pager_name);

	assert(pager_page_size == PAGE_SIZE);
	assert(IP_VALID(pager_request));
	assert(IP_VALID(pager_name));

	ds = dev_pager_hash_lookup(pager);
	assert(ds != DEV_PAGER_NULL);

	assert(ds->client_count == 0);
	assert(ds->pager_request == IP_NULL);
	assert(ds->pager_name == IP_NULL);

	ds->client_count = 1;

	/*
	 * We save the send rights for the request and name ports.
	 */

	ds->pager_request = pager_request;
	ds->pager_name = pager_name;

	if (ds->type == CHAR_PAGER_TYPE) {
	    /*
	     * Reply that the object is ready
	     */
	    (void) r_memory_object_ready(pager_request,
					 FALSE,	/* do not cache */
					 MEMORY_OBJECT_COPY_NONE);
	} else {
	    (void) r_memory_object_ready(pager_request,
					 TRUE,	/* cache */
					 MEMORY_OBJECT_COPY_DELAY);
	}

	dev_pager_deallocate(ds);
	return (KERN_SUCCESS);
}

kern_return_t device_pager_terminate(
	const ipc_port_t	pager,
	const ipc_port_t	pager_request,
	const ipc_port_t	pager_name)
{
	dev_pager_t	ds;

	assert(IP_VALID(pager_request));
	assert(IP_VALID(pager_name));

	ds = dev_pager_hash_lookup(pager);
	assert(ds != DEV_PAGER_NULL);

	assert(ds->client_count == 1);
	assert(ds->pager_request == pager_request);
	assert(ds->pager_name == pager_name);

	dev_pager_hash_delete(ds->pager);
	dev_device_hash_delete(ds->device, ds->offset);
	mach_device_deallocate(ds->device);

	/* release the send rights we have saved from the init call */

	ipc_port_release_send(pager_request);
	ipc_port_release_send(pager_name);

	/* release the naked receive rights we just acquired */

	ipc_port_release_receive(pager_request);
	ipc_port_release_receive(pager_name);

	/* release the kernel's receive right for the pager port */

	ipc_port_dealloc_kernel(pager);

	/* once for ref from lookup, once to make it go away */
	dev_pager_deallocate(ds);
	dev_pager_deallocate(ds);

	return (KERN_SUCCESS);
}

kern_return_t device_pager_data_unlock(
	const ipc_port_t memory_object,
	const ipc_port_t memory_control_port,
	vm_offset_t offset,
	vm_size_t length,
	vm_prot_t desired_access)
{
	panic("(device_pager)data_unlock: called");
	return (KERN_FAILURE);
}

kern_return_t device_pager_lock_completed(
	const ipc_port_t	memory_object,
	const ipc_port_t	pager_request_port,
	vm_offset_t		offset,
	vm_size_t		length)
{
	panic("(device_pager)lock_completed: called");
	return (KERN_FAILURE);
}

void device_pager_init(void)
{
	vm_size_t	size;

	/*
	 * Initialize cache of paging structures.
	 */
	size = sizeof(struct dev_pager);
	kmem_cache_init(&dev_pager_cache, "dev_pager", size, 0,
			NULL, 0);

	/*
	 *	Initialize the name port hashing stuff.
	 */
	dev_pager_hash_init();
	dev_device_hash_init();
}
