/*
 * Mach Operating System
 * Copyright (c) 1992-1987 Carnegie Mellon University
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
 *	File:	mach/message.h
 *
 *	Mach IPC message and primitive function definitions.
 */

#ifndef	_MACH_MESSAGE_H_
#define _MACH_MESSAGE_H_

#include <mach/kern_return.h>
#include <mach/port.h>


/*
 *  The timeout mechanism uses mach_msg_timeout_t values,
 *  passed by value.  The timeout units are milliseconds.
 *  It is controlled with the MACH_SEND_TIMEOUT
 *  and MACH_RCV_TIMEOUT options.
 */

typedef natural_t mach_msg_timeout_t;

/*
 *  The value to be used when there is no timeout.
 *  (No MACH_SEND_TIMEOUT/MACH_RCV_TIMEOUT option.)
 */

#define MACH_MSG_TIMEOUT_NONE		((mach_msg_timeout_t) 0)

/*
 *  The kernel uses MACH_MSGH_BITS_COMPLEX as a hint.  It it isn't on, it
 *  assumes the body of the message doesn't contain port rights or OOL
 *  data.  The field is set in received messages.  A user task must
 *  use caution in interpreting the body of a message if the bit isn't
 *  on, because the mach_msg_type's in the body might "lie" about the
 *  contents.  If the bit isn't on, but the mach_msg_types
 *  in the body specify rights or OOL data, the behaviour is undefined.
 *  (Ie, an error may or may not be produced.)
 *
 *  The value of MACH_MSGH_BITS_REMOTE determines the interpretation
 *  of the msgh_remote_port field.  It is handled like a msgt_name.
 *
 *  The value of MACH_MSGH_BITS_LOCAL determines the interpretation
 *  of the msgh_local_port field.  It is handled like a msgt_name.
 *
 *  MACH_MSGH_BITS() combines two MACH_MSG_TYPE_* values, for the remote
 *  and local fields, into a single value suitable for msgh_bits.
 *
 *  MACH_MSGH_BITS_COMPLEX_PORTS, MACH_MSGH_BITS_COMPLEX_DATA, and
 *  MACH_MSGH_BITS_CIRCULAR should be zero; they are used internally.
 *
 *  The unused bits should be zero.
 */

#define MACH_MSGH_BITS_ZERO		0x00000000
#define MACH_MSGH_BITS_REMOTE_MASK	0x000000ff
#define MACH_MSGH_BITS_LOCAL_MASK	0x0000ff00
#define MACH_MSGH_BITS_COMPLEX		0x80000000U
#define	MACH_MSGH_BITS_CIRCULAR		0x40000000	/* internal use only */
#define	MACH_MSGH_BITS_COMPLEX_PORTS	0x20000000	/* internal use only */
#define	MACH_MSGH_BITS_COMPLEX_DATA	0x10000000	/* internal use only */
#define	MACH_MSGH_BITS_MIGRATED		0x08000000	/* internal use only */
#define	MACH_MSGH_BITS_UNUSED		0x07ff0000

#define	MACH_MSGH_BITS_PORTS_MASK				\
		(MACH_MSGH_BITS_REMOTE_MASK|MACH_MSGH_BITS_LOCAL_MASK)

#define MACH_MSGH_BITS(remote, local)				\
		((remote) | ((local) << 8))
#define	MACH_MSGH_BITS_REMOTE(bits)				\
		((bits) & MACH_MSGH_BITS_REMOTE_MASK)
#define	MACH_MSGH_BITS_LOCAL(bits)				\
		(((bits) & MACH_MSGH_BITS_LOCAL_MASK) >> 8)
#define	MACH_MSGH_BITS_PORTS(bits)				\
		((bits) & MACH_MSGH_BITS_PORTS_MASK)
#define	MACH_MSGH_BITS_OTHER(bits)				\
		((bits) &~ MACH_MSGH_BITS_PORTS_MASK)

/*
 *  Every message starts with a message header.
 *  Following the message header are zero or more pairs of
 *  type descriptors (mach_msg_type_t/mach_msg_type_long_t) and
 *  data values.  The size of the message must be specified in bytes,
 *  and includes the message header, type descriptors, inline
 *  data, and inline pointer for out-of-line data.
 *
 *  The msgh_remote_port field specifies the destination of the message.
 *  It must specify a valid send or send-once right for a port.
 *
 *  The msgh_local_port field specifies a "reply port".  Normally,
 *  This field carries a send-once right that the receiver will use
 *  to reply to the message.  It may carry the values MACH_PORT_NULL,
 *  MACH_PORT_DEAD, a send-once right, or a send right.
 *
 *  The msgh_seqno field carries a sequence number associated with the
 *  received-from port.  A port's sequence number is incremented every
 *  time a message is received from it.  In sent messages, the field's
 *  value is ignored.
 *
 *  The msgh_id field is uninterpreted by the message primitives.
 *  It normally carries information specifying the format
 *  or meaning of the message.
 */

typedef unsigned int mach_msg_bits_t;
typedef	unsigned int mach_msg_size_t;
typedef natural_t mach_msg_seqno_t;
typedef integer_t mach_msg_id_t;

/* full header structure, may have different size in user/kernel spaces */
typedef	struct mach_msg_header {
    mach_msg_bits_t	msgh_bits;
    mach_msg_size_t	msgh_size;
    union {
        mach_port_t		msgh_remote_port;
        /*
         * Ensure msgh_remote_port is wide enough to hold a kernel pointer
         * to avoid message resizing for the 64 bits case. This field should
         * not be used since it is here just for padding purposes.
         */
        rpc_uintptr_t   msgh_remote_port_do_not_use;
    };
    union {
        mach_port_t	msgh_local_port;
        rpc_uintptr_t	msgh_protected_payload;
    };
    mach_port_seqno_t	msgh_seqno;
    mach_msg_id_t	msgh_id;
} mach_msg_header_t;

#ifdef KERNEL
/* user-side header format, needed in the kernel */
typedef	struct {
    mach_msg_bits_t	msgh_bits;
    mach_msg_size_t	msgh_size;
    union {
        mach_port_name_t	msgh_remote_port;
        rpc_uintptr_t       msgh_remote_port_do_not_use;
    };
    union {
        mach_port_name_t	msgh_local_port;
        rpc_uintptr_t msgh_protected_payload;
    };
    mach_port_seqno_t	msgh_seqno;
    mach_msg_id_t	msgh_id;
} mach_msg_user_header_t;
#else
typedef mach_msg_header_t mach_msg_user_header_t;
#endif

/*
 *  There is no fixed upper bound to the size of Mach messages.
 */

#define	MACH_MSG_SIZE_MAX	((mach_msg_size_t) ~0)

/*
 *  Compatibility definitions, for code written
 *  when there was a msgh_kind instead of msgh_seqno.
 */

#define MACH_MSGH_KIND_NORMAL		0x00000000
#if	0
/* code using this is likely to break, so better not to have it defined */
#define MACH_MSGH_KIND_NOTIFICATION	0x00000001
#endif
#define	msgh_kind			msgh_seqno
#define mach_msg_kind_t			mach_port_seqno_t

/*
 *  The msgt_number field specifies the number of data elements.
 *  The msgt_size field specifies the size of each data element, in bits.
 *  The msgt_name field specifies the type of each data element.
 *  If msgt_inline is TRUE, the data follows the type descriptor
 *  in the body of the message.  If msgt_inline is FALSE, then a pointer
 *  to the data should follow the type descriptor, and the data is
 *  sent out-of-line.  In this case, if msgt_deallocate is TRUE,
 *  then the out-of-line data is moved (instead of copied) into the message.
 *  If msgt_longform is TRUE, then the type descriptor is actually
 *  a mach_msg_type_long_t.
 *
 *  The actual amount of inline data following the descriptor must
 *  a multiple of the word size.  For out-of-line data, this is a
 *  pointer.  For inline data, the supplied data size (calculated
 *  from msgt_number/msgt_size) is rounded up.  This guarantees
 *  that type descriptors always fall on word boundaries.
 *
 *  For port rights, msgt_size must be 8*sizeof(mach_port_t).
 *  If the data is inline, msgt_deallocate should be FALSE.
 *  The msgt_unused bit should be zero.
 *  The msgt_name, msgt_size, msgt_number fields in
 *  a mach_msg_type_long_t should be zero.
 */

typedef unsigned int mach_msg_type_name_t;
typedef unsigned int mach_msg_type_size_t;
typedef natural_t  mach_msg_type_number_t;

/**
 * Structure used for inlined port rights in messages.
 *
 * We use this to avoid having to perform message resizing in the kernel
 * since userspace port rights might be smaller than kernel ports in 64 bit
 * architectures.
 */
typedef struct {
    union {
        mach_port_name_t name;
#ifdef KERNEL
        mach_port_t kernel_port;
#else
        uintptr_t kernel_port_do_not_use;
#endif  /* KERNEL */
    };
} mach_port_name_inlined_t;

typedef struct  {
#ifdef __LP64__
    /*
     * For 64 bits, this struct is 8 bytes long so we
     * can pack the same amount of information as mach_msg_type_long_t.
     * Note that for 64 bit userland, msgt_size only needs to be 8 bits long
     * but for kernel compatibility with 32 bit userland we allow it to be
     * 16 bits long.
     *
     * Effectively, we don't need mach_msg_type_long_t but we are keeping it
     * for a while to make the code similar between 32 and 64 bits.
     *
     * We also keep the msgt_longform bit around simply because it makes it
     * very easy to convert messages from a 32 bit userland into a 64 bit
     * kernel. Otherwise, we would have to replicate some of the MiG logic
     * internally in the kernel.
     */
    unsigned int	msgt_name : 8,
			msgt_size : 16,
			msgt_unused : 5,
			msgt_inline : 1,
			msgt_longform : 1,
			msgt_deallocate : 1;
    mach_msg_type_number_t   msgt_number;
#else
    unsigned int	msgt_name : 8,
			msgt_size : 8,
			msgt_number : 12,
			msgt_inline : 1,
			msgt_longform : 1,
			msgt_deallocate : 1,
			msgt_unused : 1;
#endif
} __attribute__ ((aligned (__alignof__ (uintptr_t)))) mach_msg_type_t;

typedef struct {
#ifdef __LP64__
    union {
        /* On 64-bit this is equivalent to mach_msg_type_t so use
         * union to overlay with the old field names.  */
        mach_msg_type_t	msgtl_header;
        struct {
            unsigned int	msgtl_name : 8,
                    msgtl_size : 16,
                    msgtl_unused : 5,
                    msgtl_inline : 1,
                    msgtl_longform : 1,
                    msgtl_deallocate : 1;
            mach_msg_type_number_t   msgtl_number;
        };
    };
#else
    mach_msg_type_t	msgtl_header;
    unsigned short	msgtl_name;
    unsigned short	msgtl_size;
    natural_t		msgtl_number;
#endif
} __attribute__ ((aligned (__alignof__ (uintptr_t)))) mach_msg_type_long_t;

#ifdef __LP64__
#ifdef __cplusplus
#if __cplusplus >= 201103L
static_assert (sizeof (mach_msg_type_t) == sizeof (mach_msg_type_long_t),
                "mach_msg_type_t and mach_msg_type_long_t need to have the same size.");
#endif
#else
_Static_assert (sizeof (mach_msg_type_t) == sizeof (mach_msg_type_long_t),
                "mach_msg_type_t and mach_msg_type_long_t need to have the same size.");
#endif
#endif

/*
 *	Known values for the msgt_name field.
 *
 *	The only types known to the Mach kernel are
 *	the port types, and those types used in the
 *	kernel RPC interface.
 */

#define MACH_MSG_TYPE_UNSTRUCTURED	0
#define MACH_MSG_TYPE_BIT		0
#define MACH_MSG_TYPE_BOOLEAN		0
#define MACH_MSG_TYPE_INTEGER_16	1
#define MACH_MSG_TYPE_INTEGER_32	2
#define MACH_MSG_TYPE_CHAR		8
#define MACH_MSG_TYPE_BYTE		9
#define MACH_MSG_TYPE_INTEGER_8		9
#define MACH_MSG_TYPE_REAL		10
#define MACH_MSG_TYPE_INTEGER_64	11
#define MACH_MSG_TYPE_STRING		12
#define MACH_MSG_TYPE_STRING_C		12

/*
 *  Values used when sending a port right.
 */

#define MACH_MSG_TYPE_MOVE_RECEIVE	16	/* Must hold receive rights */
#define MACH_MSG_TYPE_MOVE_SEND		17	/* Must hold send rights */
#define MACH_MSG_TYPE_MOVE_SEND_ONCE	18	/* Must hold sendonce rights */
#define MACH_MSG_TYPE_COPY_SEND		19	/* Must hold send rights */
#define MACH_MSG_TYPE_MAKE_SEND		20	/* Must hold receive rights */
#define MACH_MSG_TYPE_MAKE_SEND_ONCE	21	/* Must hold receive rights */

/*
 *  Values received/carried in messages.  Tells the receiver what
 *  sort of port right he now has.
 *
 *  MACH_MSG_TYPE_PORT_NAME is used to transfer a port name
 *  which should remain uninterpreted by the kernel.  (Port rights
 *  are not transferred, just the port name.)
 */

#define MACH_MSG_TYPE_PORT_NAME		15
#define MACH_MSG_TYPE_PORT_RECEIVE	MACH_MSG_TYPE_MOVE_RECEIVE
#define MACH_MSG_TYPE_PORT_SEND		MACH_MSG_TYPE_MOVE_SEND
#define MACH_MSG_TYPE_PORT_SEND_ONCE	MACH_MSG_TYPE_MOVE_SEND_ONCE

#define MACH_MSG_TYPE_PROTECTED_PAYLOAD	23

#define MACH_MSG_TYPE_LAST		23		/* Last assigned */

/*
 *  A dummy value.  Mostly used to indicate that the actual value
 *  will be filled in later, dynamically.
 */

#define MACH_MSG_TYPE_POLYMORPHIC	((mach_msg_type_name_t) -1)

/*
 *	Is a given item a port type?
 */

#define MACH_MSG_TYPE_PORT_ANY(x)			\
	(((x) >= MACH_MSG_TYPE_MOVE_RECEIVE) &&		\
	 ((x) <= MACH_MSG_TYPE_MAKE_SEND_ONCE))

#define	MACH_MSG_TYPE_PORT_ANY_SEND(x)			\
	(((x) >= MACH_MSG_TYPE_MOVE_SEND) &&		\
	 ((x) <= MACH_MSG_TYPE_MAKE_SEND_ONCE))

#define	MACH_MSG_TYPE_PORT_ANY_RIGHT(x)			\
	(((x) >= MACH_MSG_TYPE_MOVE_RECEIVE) &&		\
	 ((x) <= MACH_MSG_TYPE_MOVE_SEND_ONCE))

typedef integer_t mach_msg_option_t;

#define MACH_MSG_OPTION_NONE	0x00000000

#define	MACH_SEND_MSG		0x00000001
#define	MACH_RCV_MSG		0x00000002

#define MACH_SEND_TIMEOUT	0x00000010
#define MACH_SEND_NOTIFY	0x00000020
#define MACH_SEND_INTERRUPT	0x00000040	/* libmach implements */
#define MACH_SEND_CANCEL	0x00000080
#define MACH_RCV_TIMEOUT	0x00000100
#define MACH_RCV_NOTIFY		0x00000200
#define MACH_RCV_INTERRUPT	0x00000400	/* libmach implements */
#define MACH_RCV_LARGE		0x00000800

#define MACH_SEND_ALWAYS	0x00010000	/* internal use only */

#ifdef __LP64__
#if defined(KERNEL) && defined(USER32)
#define MACH_MSG_USER_ALIGNMENT 4
#else
#define MACH_MSG_USER_ALIGNMENT 8
#endif
#else
#define MACH_MSG_USER_ALIGNMENT 4
#endif

#ifdef KERNEL
/* This is the alignment of msg descriptors and the actual data
 * for both in kernel messages and user land messages.
 *
 * We have two types of alignment because for specific configurations
 * (in particular a 64 bit kernel with 32 bit userland) we transform
 * 4-byte aligned user messages into 8-byte aligned messages (and vice-versa)
 * so that kernel messages are correctly aligned.
 */
#define MACH_MSG_KERNEL_ALIGNMENT sizeof(uintptr_t)

#define mach_msg_align(x, alignment)	\
	( ( ((vm_offset_t)(x)) + ((alignment)-1) ) & ~((alignment)-1) )
#define mach_msg_user_align(x) mach_msg_align(x, MACH_MSG_USER_ALIGNMENT)
#define mach_msg_kernel_align(x) mach_msg_align(x, MACH_MSG_KERNEL_ALIGNMENT)
#define mach_msg_user_is_misaligned(x) ((x) & ((MACH_MSG_USER_ALIGNMENT)-1))
#define mach_msg_kernel_is_misaligned(x) ((x) & ((MACH_MSG_KERNEL_ALIGNMENT)-1))
#endif /* KERNEL */

/*
 *  Much code assumes that mach_msg_return_t == kern_return_t.
 *  This definition is useful for descriptive purposes.
 *
 *  See <mach/error.h> for the format of error codes.
 *  IPC errors are system 4.  Send errors are subsystem 0;
 *  receive errors are subsystem 1.  The code field is always non-zero.
 *  The high bits of the code field communicate extra information
 *  for some error codes.  MACH_MSG_MASK masks off these special bits.
 */

typedef kern_return_t mach_msg_return_t;

#define MACH_MSG_SUCCESS		0x00000000

#define	MACH_MSG_MASK			0x00003c00
		/* All special error code bits defined below. */
#define	MACH_MSG_IPC_SPACE		0x00002000
		/* No room in IPC name space for another capability name. */
#define	MACH_MSG_VM_SPACE		0x00001000
		/* No room in VM address space for out-of-line memory. */
#define	MACH_MSG_IPC_KERNEL		0x00000800
		/* Kernel resource shortage handling an IPC capability. */
#define	MACH_MSG_VM_KERNEL		0x00000400
		/* Kernel resource shortage handling out-of-line memory. */

#define MACH_SEND_IN_PROGRESS		0x10000001
		/* Thread is waiting to send.  (Internal use only.) */
#define MACH_SEND_INVALID_DATA		0x10000002
		/* Bogus in-line data. */
#define MACH_SEND_INVALID_DEST		0x10000003
		/* Bogus destination port. */
#define MACH_SEND_TIMED_OUT		0x10000004
		/* Message not sent before timeout expired. */
#define MACH_SEND_WILL_NOTIFY		0x10000005
		/* Msg-accepted notification will be generated. */
#define MACH_SEND_NOTIFY_IN_PROGRESS	0x10000006
		/* Msg-accepted notification already pending. */
#define MACH_SEND_INTERRUPTED		0x10000007
		/* Software interrupt. */
#define MACH_SEND_MSG_TOO_SMALL		0x10000008
		/* Data doesn't contain a complete message. */
#define MACH_SEND_INVALID_REPLY		0x10000009
		/* Bogus reply port. */
#define MACH_SEND_INVALID_RIGHT		0x1000000a
		/* Bogus port rights in the message body. */
#define MACH_SEND_INVALID_NOTIFY	0x1000000b
		/* Bogus notify port argument. */
#define MACH_SEND_INVALID_MEMORY	0x1000000c
		/* Invalid out-of-line memory pointer. */
#define MACH_SEND_NO_BUFFER		0x1000000d
		/* No message buffer is available. */
#define MACH_SEND_NO_NOTIFY		0x1000000e
		/* Resource shortage; can't request msg-accepted notif. */
#define MACH_SEND_INVALID_TYPE		0x1000000f
		/* Invalid msg-type specification. */
#define MACH_SEND_INVALID_HEADER	0x10000010
		/* A field in the header had a bad value. */

#define MACH_RCV_IN_PROGRESS		0x10004001
		/* Thread is waiting for receive.  (Internal use only.) */
#define MACH_RCV_INVALID_NAME		0x10004002
		/* Bogus name for receive port/port-set. */
#define MACH_RCV_TIMED_OUT		0x10004003
		/* Didn't get a message within the timeout value. */
#define MACH_RCV_TOO_LARGE		0x10004004
		/* Message buffer is not large enough for inline data. */
#define MACH_RCV_INTERRUPTED		0x10004005
		/* Software interrupt. */
#define MACH_RCV_PORT_CHANGED		0x10004006
		/* Port moved into a set during the receive. */
#define MACH_RCV_INVALID_NOTIFY		0x10004007
		/* Bogus notify port argument. */
#define MACH_RCV_INVALID_DATA		0x10004008
		/* Bogus message buffer for inline data. */
#define MACH_RCV_PORT_DIED		0x10004009
		/* Port/set was sent away/died during receive. */
#define	MACH_RCV_IN_SET			0x1000400a
		/* Port is a member of a port set. */
#define	MACH_RCV_HEADER_ERROR		0x1000400b
		/* Error receiving message header.  See special bits. */
#define	MACH_RCV_BODY_ERROR		0x1000400c
		/* Error receiving message body.  See special bits. */

extern mach_msg_return_t
mach_msg_trap
   (mach_msg_user_header_t *msg,
    mach_msg_option_t option,
    mach_msg_size_t send_size,
    mach_msg_size_t rcv_size,
    mach_port_name_t rcv_name,
    mach_msg_timeout_t timeout,
    mach_port_name_t notify);

extern mach_msg_return_t
mach_msg
   (mach_msg_header_t *msg,
    mach_msg_option_t option,
    mach_msg_size_t send_size,
    mach_msg_size_t rcv_size,
    mach_port_name_t rcv_name,
    mach_msg_timeout_t timeout,
    mach_port_name_t notify);

extern __typeof (mach_msg) __mach_msg;
extern __typeof (mach_msg_trap) __mach_msg_trap;

#endif	/* _MACH_MESSAGE_H_ */
