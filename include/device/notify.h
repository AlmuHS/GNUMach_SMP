/*
 * Copyright (c) 2010 Free Software Foundation, Inc.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * THE FREE SOFTWARE FOUNDATIONALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  THE FREE SOFTWARE FOUNDATION DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 */

/*
 * Device notification definitions.
 */

#ifndef	_MACH_DEVICE_NOTIFY_H_
#define _MACH_DEVICE_NOTIFY_H_

#include <mach/port.h>
#include <mach/message.h>

typedef struct
{
  mach_msg_header_t intr_header;
  mach_msg_type_t   intr_type;
  int		    id;
} device_intr_notification_t;

#define DEVICE_INTR_NOTIFY 100

#endif	/* _MACH_DEVICE_NOTIFY_H_ */
