#ifndef _DEVICE_KMSG_H_
#define _DEVICE_KMSG_H_	1

#ifdef MACH_KERNEL

#include <sys/types.h>

#include <device/device_types.h>
#include <device/io_req.h>

io_return_t kmsgopen (dev_t dev, int flag, io_req_t ior);
io_return_t kmsgclose (dev_t dev, int flag);
io_return_t kmsgread (dev_t dev, io_req_t ior);
io_return_t kmsggetstat (dev_t dev, int flavor,
			 int *data, unsigned int *count);
void kmsg_putchar (int c);

#endif /* MACH_KERNEL */

#endif /* !_DEVICE_KMSG_H_ */
