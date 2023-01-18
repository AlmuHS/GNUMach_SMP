/*
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
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

#include <mach/boolean.h>
#include <mach/port.h>
#include <mach/message.h>
#include <mach/thread_status.h>
#include <machine/locore.h>
#include <machine/copy_user.h>
#include <kern/ast.h>
#include <kern/debug.h>
#include <kern/ipc_tt.h>
#include <kern/syscall_subr.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/ipc_kobject.h>
#include <kern/ipc_tt.h>
#include <kern/ipc_mig.h>
#include <vm/vm_map.h>
#include <vm/vm_user.h>
#include <ipc/port.h>
#include <ipc/ipc_kmsg.h>
#include <ipc/ipc_entry.h>
#include <ipc/ipc_object.h>
#include <ipc/ipc_mqueue.h>
#include <ipc/ipc_space.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_pset.h>
#include <ipc/ipc_thread.h>
#include <ipc/mach_port.h>
#include <device/dev_hdr.h>
#include <device/device_types.h>
#include <device/ds_routines.h>

/*
 *	Routine:	mach_msg_send_from_kernel
 *	Purpose:
 *		Send a message from the kernel.
 *
 *		This is used by the client side of KernelUser interfaces
 *		to implement SimpleRoutines.  Currently, this includes
 *		device_reply and memory_object messages.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 *		MACH_MSG_SUCCESS	Sent the message.
 *		MACH_SEND_INVALID_DATA	Bad destination port.
 */

mach_msg_return_t
mach_msg_send_from_kernel(
	mach_msg_header_t	*msg,
	mach_msg_size_t		send_size)
{
	ipc_kmsg_t kmsg;
	mach_msg_return_t mr;

	if (!MACH_PORT_VALID(msg->msgh_remote_port))
		return MACH_SEND_INVALID_DEST;

	mr = ipc_kmsg_get_from_kernel(msg, send_size, &kmsg);
	if (mr != MACH_MSG_SUCCESS)
		panic("mach_msg_send_from_kernel");

	ipc_kmsg_copyin_from_kernel(kmsg);
	ipc_mqueue_send_always(kmsg);

	return MACH_MSG_SUCCESS;
}

mach_msg_return_t
mach_msg_rpc_from_kernel(msg, send_size, reply_size)
	const mach_msg_header_t *msg;
	mach_msg_size_t send_size;
	mach_msg_size_t reply_size;
{
	panic("mach_msg_rpc_from_kernel"); /*XXX*/
}

/*
 *	Routine:	mach_msg_abort_rpc
 *	Purpose:
 *		Destroy the thread's ith_rpc_reply port.
 *		This will interrupt a mach_msg_rpc_from_kernel
 *		with a MACH_RCV_PORT_DIED return code.
 *	Conditions:
 *		Nothing locked.
 */

void
mach_msg_abort_rpc(ipc_thread_t thread)
{
	ipc_port_t reply = IP_NULL;

	ith_lock(thread);
	if (thread->ith_self != IP_NULL) {
		reply = thread->ith_rpc_reply;
		thread->ith_rpc_reply = IP_NULL;
	}
	ith_unlock(thread);

	if (reply != IP_NULL)
		ipc_port_dealloc_reply(reply);
}

/*
 *	Routine:	mach_msg
 *	Purpose:
 *		Like mach_msg_trap except that message buffers
 *		live in kernel space.  Doesn't handle any options.
 *
 *		This is used by in-kernel server threads to make
 *		kernel calls, to receive request messages, and
 *		to send reply messages.
 *	Conditions:
 *		Nothing locked.
 *	Returns:
 */

mach_msg_return_t
mach_msg(
	mach_msg_header_t 	*msg,
	mach_msg_option_t 	option,
	mach_msg_size_t 	send_size,
	mach_msg_size_t 	rcv_size,
	mach_port_name_t 	rcv_name,
	mach_msg_timeout_t 	time_out,
	mach_port_name_t 	notify)
{
	ipc_space_t space = current_space();
	vm_map_t map = current_map();
	ipc_kmsg_t kmsg;
	mach_port_seqno_t seqno;
	mach_msg_return_t mr;

	if (option & MACH_SEND_MSG) {
		mr = ipc_kmsg_get_from_kernel(msg, send_size, &kmsg);
		if (mr != MACH_MSG_SUCCESS)
			panic("mach_msg");

		mr = ipc_kmsg_copyin(kmsg, space, map, MACH_PORT_NULL);
		if (mr != MACH_MSG_SUCCESS) {
			ikm_free(kmsg);
			return mr;
		}

		do
			mr = ipc_mqueue_send(kmsg, MACH_MSG_OPTION_NONE,
					     MACH_MSG_TIMEOUT_NONE);
		while (mr == MACH_SEND_INTERRUPTED);
		assert(mr == MACH_MSG_SUCCESS);
	}

	if (option & MACH_RCV_MSG) {
		do {
			ipc_object_t object;
			ipc_mqueue_t mqueue;

			mr = ipc_mqueue_copyin(space, rcv_name,
					       &mqueue, &object);
			if (mr != MACH_MSG_SUCCESS)
				return mr;
			/* hold ref for object; mqueue is locked */

			mr = ipc_mqueue_receive(mqueue, MACH_MSG_OPTION_NONE,
						MACH_MSG_SIZE_MAX,
						MACH_MSG_TIMEOUT_NONE,
						FALSE, IMQ_NULL_CONTINUE,
						&kmsg, &seqno);
			/* mqueue is unlocked */
			ipc_object_release(object);
		} while (mr == MACH_RCV_INTERRUPTED);
		if (mr != MACH_MSG_SUCCESS)
			return mr;

		kmsg->ikm_header.msgh_seqno = seqno;

		if (rcv_size < kmsg->ikm_header.msgh_size) {
			ipc_kmsg_copyout_dest(kmsg, space);
			ipc_kmsg_put_to_kernel(msg, kmsg, sizeof *msg);
			return MACH_RCV_TOO_LARGE;
		}

		mr = ipc_kmsg_copyout(kmsg, space, map, MACH_PORT_NULL);
		if (mr != MACH_MSG_SUCCESS) {
			if ((mr &~ MACH_MSG_MASK) == MACH_RCV_BODY_ERROR) {
				ipc_kmsg_put_to_kernel(msg, kmsg,
						kmsg->ikm_header.msgh_size);
			} else {
				ipc_kmsg_copyout_dest(kmsg, space);
				ipc_kmsg_put_to_kernel(msg, kmsg, sizeof *msg);
			}

			return mr;
		}

		ipc_kmsg_put_to_kernel(msg, kmsg, kmsg->ikm_header.msgh_size);
	}

	return MACH_MSG_SUCCESS;
}

/*
 *	Routine:	mig_get_reply_port
 *	Purpose:
 *		Called by client side interfaces living in the kernel
 *		to get a reply port.  This port is used for
 *		mach_msg() calls which are kernel calls.
 */

mach_port_name_t
mig_get_reply_port(void)
{
	ipc_thread_t self = current_thread();

	if (self->ith_mig_reply == MACH_PORT_NULL)
		self->ith_mig_reply = mach_reply_port();

	return self->ith_mig_reply;
}

/*
 *	Routine:	mig_dealloc_reply_port
 *	Purpose:
 *		Called by client side interfaces to get rid of a reply port.
 *		Shouldn't ever be called inside the kernel, because
 *		kernel calls shouldn't prompt Mig to call it.
 */

void
mig_dealloc_reply_port(
	mach_port_t	reply_port)
{
	panic("mig_dealloc_reply_port");
}

/*
 *	Routine:	mig_put_reply_port
 *	Purpose:
 *		Called by client side interfaces after each RPC to
 *		let the client recycle the reply port if it wishes.
 */
void
mig_put_reply_port(
	mach_port_t	reply_port)
{
}

/*
 * mig_strncpy.c - by Joshua Block
 *
 * mig_strncpy -- Bounded string copy.  Does what the library routine
 * strncpy does: Copies the (null terminated) string in src into dest,
 * a buffer of length len.  Returns the length of the destination
 * string excluding the terminating null.
 *
 * Parameters:
 *
 *     dest - Pointer to destination buffer.
 *
 *     src - Pointer to source string.
 *
 *     len - Length of destination buffer.
 */
vm_size_t
mig_strncpy(char *dest, const char *src, int len)
{
	char *dest_ = dest;
	int i;

	if (len <= 0)
		return 0;

	for (i = 0; i < len; i++) {
		if (! (*dest = *src))
			break;
		dest++;
		src++;
	}

	return dest - dest_;
}

#define	fast_send_right_lookup(name, port, abort)			\
MACRO_BEGIN								\
	ipc_space_t space = current_space();				\
	ipc_entry_t entry;						\
									\
	is_read_lock(space);						\
	assert(space->is_active);					\
									\
	entry = ipc_entry_lookup (space, name);				\
	if (entry == IE_NULL) {						\
		is_read_unlock (space);					\
		abort;							\
	}								\
									\
	if (IE_BITS_TYPE (entry->ie_bits) != MACH_PORT_TYPE_SEND) {	\
		is_read_unlock (space);					\
		abort;							\
	}								\
									\
	port = (ipc_port_t) entry->ie_object;				\
	assert(port != IP_NULL);					\
									\
	ip_lock(port);							\
	/* can safely unlock space now that port is locked */		\
	is_read_unlock(space);						\
MACRO_END

static device_t
port_name_to_device(mach_port_name_t name)
{
	ipc_port_t port;
	device_t device;

	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */

	/*
	 * Now map the port object to a device object.
	 * This is an inline version of dev_port_lookup().
	 */
	if (ip_active(port) && (ip_kotype(port) == IKOT_DEVICE)) {
		device = (device_t) port->ip_kobject;
		device_reference(device);
		ip_unlock(port);
		return device;
	}

	ip_unlock(port);
	return DEVICE_NULL;

       /*
        * The slow case.  The port wasn't easily accessible.
        */
    abort: {
	    ipc_port_t kern_port;
	    kern_return_t kr;

	    kr = ipc_object_copyin(current_space(), name,
				   MACH_MSG_TYPE_COPY_SEND,
				   (ipc_object_t *) &kern_port);
	    if (kr != KERN_SUCCESS)
		    return DEVICE_NULL;

	    device = dev_port_lookup(kern_port);
	    if (IP_VALID(kern_port))
		    ipc_port_release_send(kern_port);
	    return device;
    }
}

static thread_t
port_name_to_thread(mach_port_name_t name)
{
	ipc_port_t port;

	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */

	if (ip_active(port) &&
	    (ip_kotype(port) == IKOT_THREAD)) {
		thread_t thread;

		thread = (thread_t) port->ip_kobject;
		assert(thread != THREAD_NULL);

		/* thread referencing is a bit complicated,
		   so don't bother to expand inline */
		thread_reference(thread);
		ip_unlock(port);

		return thread;
	}

	ip_unlock(port);
	return THREAD_NULL;

    abort: {
	thread_t thread;
	ipc_port_t kern_port;
	kern_return_t kr;

	kr = ipc_object_copyin(current_space(), name,
			       MACH_MSG_TYPE_COPY_SEND,
			       (ipc_object_t *) &kern_port);
	if (kr != KERN_SUCCESS)
		return THREAD_NULL;

	thread = convert_port_to_thread(kern_port);
	if (IP_VALID(kern_port))
		ipc_port_release_send(kern_port);

	return thread;
    }
}

static task_t
port_name_to_task(mach_port_name_t name)
{
	ipc_port_t port;

	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */

	if (ip_active(port) &&
	    (ip_kotype(port) == IKOT_TASK)) {
		task_t task;

		task = (task_t) port->ip_kobject;
		assert(task != TASK_NULL);

		task_lock(task);
		/* can safely unlock port now that task is locked */
		ip_unlock(port);

		task->ref_count++;
		task_unlock(task);

		return task;
	}

	ip_unlock(port);
	return TASK_NULL;

    abort: {
	task_t task;
	ipc_port_t kern_port;
	kern_return_t kr;

	kr = ipc_object_copyin(current_space(), name,
			       MACH_MSG_TYPE_COPY_SEND,
			       (ipc_object_t *) &kern_port);
	if (kr != KERN_SUCCESS)
		return TASK_NULL;

	task = convert_port_to_task(kern_port);
	if (IP_VALID(kern_port))
		ipc_port_release_send(kern_port);

	return task;
    }
}

static vm_map_t
port_name_to_map(
	mach_port_name_t	name)
{
	ipc_port_t port;

	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */

	if (ip_active(port) &&
	    (ip_kotype(port) == IKOT_TASK)) {
		vm_map_t map;

		map = ((task_t) port->ip_kobject)->map;
		assert(map != VM_MAP_NULL);

		simple_lock(&map->ref_lock);
		/* can safely unlock port now that map is locked */
		ip_unlock(port);

		map->ref_count++;
		simple_unlock(&map->ref_lock);

		return map;
	}

	ip_unlock(port);
	return VM_MAP_NULL;

    abort: {
	vm_map_t map;
	ipc_port_t kern_port;
	kern_return_t kr;

	kr = ipc_object_copyin(current_space(), name,
			       MACH_MSG_TYPE_COPY_SEND,
			       (ipc_object_t *) &kern_port);
	if (kr != KERN_SUCCESS)
		return VM_MAP_NULL;

	map = convert_port_to_map(kern_port);
	if (IP_VALID(kern_port))
		ipc_port_release_send(kern_port);

	return map;
    }
}

static ipc_space_t
port_name_to_space(mach_port_name_t name)
{
	ipc_port_t port;

	fast_send_right_lookup(name, port, goto abort);
	/* port is locked */

	if (ip_active(port) &&
	    (ip_kotype(port) == IKOT_TASK)) {
		ipc_space_t space;

		space = ((task_t) port->ip_kobject)->itk_space;
		assert(space != IS_NULL);

		simple_lock(&space->is_ref_lock_data);
		/* can safely unlock port now that space is locked */
		ip_unlock(port);

		space->is_references++;
		simple_unlock(&space->is_ref_lock_data);

		return space;
	}

	ip_unlock(port);
	return IS_NULL;

    abort: {
	ipc_space_t space;
	ipc_port_t kern_port;
	kern_return_t kr;

	kr = ipc_object_copyin(current_space(), name,
			       MACH_MSG_TYPE_COPY_SEND,
			       (ipc_object_t *) &kern_port);
	if (kr != KERN_SUCCESS)
		return IS_NULL;

	space = convert_port_to_space(kern_port);
	if (IP_VALID(kern_port))
		ipc_port_release_send(kern_port);

	return space;
    }
}

/*
 *	Things to keep in mind:
 *
 *	The idea here is to duplicate the semantics of the true kernel RPC.
 *	The destination port/object should be checked first, before anything
 *	that the user might notice (like ipc_object_copyin).  Return
 *	MACH_SEND_INTERRUPTED if it isn't correct, so that the user stub
 *	knows to fall back on an RPC.  For other return values, it won't
 *	retry with an RPC.  The retry might get a different (incorrect) rc.
 *	Return values are only set (and should only be set, with copyout)
 *	on successful calls.
 */

kern_return_t
syscall_vm_map(
	mach_port_name_t	target_map,
	rpc_vm_offset_t	*address,
	rpc_vm_size_t	size,
	rpc_vm_offset_t	mask,
	boolean_t	anywhere,
	mach_port_name_t	memory_object,
	rpc_vm_offset_t	offset,
	boolean_t	copy,
	vm_prot_t	cur_protection,
	vm_prot_t	max_protection,
	vm_inherit_t	inheritance)
{
	vm_map_t		map;
	ipc_port_t		port;
	vm_offset_t		addr;
	kern_return_t		result;

	map = port_name_to_map(target_map);
	if (map == VM_MAP_NULL)
		return MACH_SEND_INTERRUPTED;

	if (MACH_PORT_VALID(memory_object)) {
		result = ipc_object_copyin(current_space(), memory_object,
					   MACH_MSG_TYPE_COPY_SEND,
					   (ipc_object_t *) &port);
		if (result != KERN_SUCCESS) {
			vm_map_deallocate(map);
			return result;
		}
	} else
		port = (ipc_port_t) memory_object;

	copyin_address(address, &addr);
	result = vm_map(map, &addr, size, mask, anywhere,
			port, offset, copy,
			cur_protection, max_protection,	inheritance);
	if (result == KERN_SUCCESS)
		copyout_address(&addr, address);
	if (IP_VALID(port))
		ipc_port_release_send(port);
	vm_map_deallocate(map);

	return result;
}

kern_return_t syscall_vm_allocate(
	mach_port_name_t	target_map,
	rpc_vm_offset_t		*address,
	rpc_vm_size_t		size,
	boolean_t		anywhere)
{
	vm_map_t		map;
	vm_offset_t		addr;
	kern_return_t		result;

	map = port_name_to_map(target_map);
	if (map == VM_MAP_NULL)
		return MACH_SEND_INTERRUPTED;

	copyin_address(address, &addr);
	result = vm_allocate(map, &addr, size, anywhere);
	if (result == KERN_SUCCESS)
		copyout_address(&addr, address);
	vm_map_deallocate(map);

	return result;
}

kern_return_t syscall_vm_deallocate(
	mach_port_name_t       	target_map,
	rpc_vm_offset_t		start,
	rpc_vm_size_t		size)
{
	vm_map_t		map;
	kern_return_t		result;

	map = port_name_to_map(target_map);
	if (map == VM_MAP_NULL)
		return MACH_SEND_INTERRUPTED;

	result = vm_deallocate(map, start, size);
	vm_map_deallocate(map);

	return result;
}

kern_return_t syscall_task_create(
	mach_port_name_t	parent_task,
	boolean_t			inherit_memory,
	mach_port_name_t	*child_task)		/* OUT */
{
	task_t		t, c;
	ipc_port_t	port;
	mach_port_name_t 	name;
	kern_return_t	result;

	t = port_name_to_task(parent_task);
	if (t == TASK_NULL)
		return MACH_SEND_INTERRUPTED;

	result = task_create(t, inherit_memory, &c);
	if (result == KERN_SUCCESS) {
		port = (ipc_port_t) convert_task_to_port(c);
		/* always returns a name, even for non-success return codes */
		(void) ipc_kmsg_copyout_object(current_space(),
					       (ipc_object_t) port,
					       MACH_MSG_TYPE_PORT_SEND, &name);
		copyout_port(&name, child_task);
	}
	task_deallocate(t);

	return result;
}

kern_return_t syscall_task_terminate(mach_port_name_t task)
{
	task_t		t;
	kern_return_t	result;

	t = port_name_to_task(task);
	if (t == TASK_NULL)
		return MACH_SEND_INTERRUPTED;

	result = task_terminate(t);
	task_deallocate(t);

	return result;
}

kern_return_t syscall_task_suspend(mach_port_name_t task)
{
	task_t		t;
	kern_return_t	result;

	t = port_name_to_task(task);
	if (t == TASK_NULL)
		return MACH_SEND_INTERRUPTED;

	result = task_suspend(t);
	task_deallocate(t);

	return result;
}

kern_return_t syscall_task_set_special_port(
	mach_port_name_t	task,
	int		which_port,
	mach_port_name_t	port_name)
{
	task_t		t;
	ipc_port_t	port;
	kern_return_t	result;

	t = port_name_to_task(task);
	if (t == TASK_NULL)
		return MACH_SEND_INTERRUPTED;

	if (MACH_PORT_VALID(port_name)) {
		result = ipc_object_copyin(current_space(), port_name,
					   MACH_MSG_TYPE_COPY_SEND,
					   (ipc_object_t *) &port);
		if (result != KERN_SUCCESS) {
			task_deallocate(t);
			return result;
		}
	} else
		port = (ipc_port_t) port_name;

	result = task_set_special_port(t, which_port, port);
	if ((result != KERN_SUCCESS) && IP_VALID(port))
		ipc_port_release_send(port);
	task_deallocate(t);

	return result;
}

kern_return_t
syscall_mach_port_allocate(
	mach_port_name_t 	task,
	mach_port_right_t 	right,
	mach_port_name_t 	*namep)
{
	ipc_space_t space;
	mach_port_name_t name;
	kern_return_t kr;

	space = port_name_to_space(task);
	if (space == IS_NULL)
		return MACH_SEND_INTERRUPTED;

	kr = mach_port_allocate(space, right, &name);
	if (kr == KERN_SUCCESS)
	{
		copyout_port(&name, namep);
	}
	is_release(space);

	return kr;
}

kern_return_t
syscall_mach_port_allocate_name(
	mach_port_name_t 	task,
	mach_port_right_t 	right,
	mach_port_name_t 	name)
{
	ipc_space_t space;
	kern_return_t kr;

	space = port_name_to_space(task);
	if (space == IS_NULL)
		return MACH_SEND_INTERRUPTED;

	kr = mach_port_allocate_name(space, right, name);
	is_release(space);

	return kr;
}

kern_return_t
syscall_mach_port_deallocate(
	mach_port_name_t task,
	mach_port_name_t name)
{
	ipc_space_t space;
	kern_return_t kr;

	space = port_name_to_space(task);
	if (space == IS_NULL)
		return MACH_SEND_INTERRUPTED;

	kr = mach_port_deallocate(space, name);
	is_release(space);

	return kr;
}

kern_return_t
syscall_mach_port_insert_right(
	mach_port_name_t 		task,
	mach_port_name_t 		name,
	mach_port_name_t 		right,
	mach_msg_type_name_t 	rightType)
{
	ipc_space_t space;
	ipc_object_t object;
	mach_msg_type_name_t newtype;
	kern_return_t kr;

	space = port_name_to_space(task);
	if (space == IS_NULL)
		return MACH_SEND_INTERRUPTED;

	if (!MACH_MSG_TYPE_PORT_ANY(rightType)) {
		is_release(space);
		return KERN_INVALID_VALUE;
	}

	if (MACH_PORT_VALID(right)) {
		kr = ipc_object_copyin(current_space(), right, rightType,
				       &object);
		if (kr != KERN_SUCCESS) {
			is_release(space);
			return kr;
		}
	} else
		object = (ipc_object_t) right;
	newtype = ipc_object_copyin_type(rightType);

	kr = mach_port_insert_right(space, name, (ipc_port_t) object, newtype);
	if ((kr != KERN_SUCCESS) && IO_VALID(object))
		ipc_object_destroy(object, newtype);
	is_release(space);

	return kr;
}

kern_return_t syscall_thread_depress_abort(mach_port_name_t thread)
{
	thread_t	t;
	kern_return_t	result;

	t = port_name_to_thread(thread);
	if (t == THREAD_NULL)
		return MACH_SEND_INTERRUPTED;

	result = thread_depress_abort(t);
	thread_deallocate(t);

	return result;
}

/*
 * Device traps -- these are way experimental.
 */
io_return_t
syscall_device_write_request(mach_port_name_t	device_name,
			     mach_port_name_t	reply_name,
			     dev_mode_t		mode,
			     rpc_recnum_t	recnum,
			     rpc_vm_offset_t	data,
			     rpc_vm_size_t	data_count)
{
	device_t	dev;
	/*ipc_port_t	reply_port;*/
	io_return_t	res;

	/*
	 * First try to translate the device name.
	 *
	 * If this fails, return KERN_INVALID_CAPABILITY.
	 * Caller knows that this most likely means that
	 * device is not local to node and IPC should be used.
	 *
	 * If kernel doesn't do device traps, kern_invalid()
	 * will be called instead of this function which will
	 * return KERN_INVALID_ARGUMENT.
	 */
	dev = port_name_to_device(device_name);
	if (dev == DEVICE_NULL)
		return KERN_INVALID_CAPABILITY;

	/*
	 * Translate reply port.
	 */
	/*if (reply_name == MACH_PORT_NULL)
		reply_port = IP_NULL;
	*/
	if (reply_name != MACH_PORT_NULL) {
		/* Homey don't play that. */
		device_deallocate(dev);
		return KERN_INVALID_RIGHT;
	}

	/* note: doesn't take reply_port arg yet. */
	res = ds_device_write_trap(dev, /*reply_port,*/
				   mode, recnum,
				   data, data_count);

	/*
	 * Give up reference from port_name_to_device.
	 */
	device_deallocate(dev);
	return res;
}

io_return_t
syscall_device_writev_request(mach_port_name_t	device_name,
			      mach_port_name_t	reply_name,
			      dev_mode_t	mode,
			      rpc_recnum_t	recnum,
			      rpc_io_buf_vec_t	*iovec,
			      rpc_vm_size_t	iocount)
{
	device_t	dev;
	/*ipc_port_t	reply_port;*/
	io_return_t	res;

	/*
	 * First try to translate the device name.
	 *
	 * If this fails, return KERN_INVALID_CAPABILITY.
	 * Caller knows that this most likely means that
	 * device is not local to node and IPC should be used.
	 *
	 * If kernel doesn't do device traps, kern_invalid()
	 * will be called instead of this function which will
	 * return KERN_INVALID_ARGUMENT.
	 */
	dev = port_name_to_device(device_name);
	if (dev == DEVICE_NULL)
		return KERN_INVALID_CAPABILITY;

	/*
	 * Translate reply port.
	 */
	/*if (reply_name == MACH_PORT_NULL)
		reply_port = IP_NULL;
	*/
	if (reply_name != MACH_PORT_NULL) {
		/* Homey don't play that. */
		device_deallocate(dev);
		return KERN_INVALID_RIGHT;
	}

	/* note: doesn't take reply_port arg yet. */
	res = ds_device_writev_trap(dev, /*reply_port,*/
				    mode, recnum,
				    iovec, iocount);

	/*
	 * Give up reference from port_name_to_device.
	 */
	device_deallocate(dev);
	return res;
}
