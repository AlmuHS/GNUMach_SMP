#ifndef __INTR_H__

#define __INTR_H__

#include <device/device_types.h>

typedef struct
{
  mach_msg_header_t intr_header;
  mach_msg_type_t   intr_type;
  int		    line;
} mach_intr_notification_t;

#define INTR_NOTIFY_MSGH_SEQNO 0
#define MACH_INTR_NOTIFY 424242

#endif
