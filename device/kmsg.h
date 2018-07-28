#ifndef _DEVICE_KMSG_H_
#define _DEVICE_KMSG_H_	1


#include <sys/types.h>

#include <device/device_types.h>
#include <device/io_req.h>

io_return_t kmsgopen (dev_t dev, int flag, io_req_t ior);
void kmsgclose (dev_t dev, int flag);
io_return_t kmsgread (dev_t dev, io_req_t ior);
io_return_t kmsggetstat (dev_t dev, dev_flavor_t flavor,
			 dev_status_t data, mach_msg_type_number_t *count);
void kmsg_putchar (int c);


#endif /* !_DEVICE_KMSG_H_ */
