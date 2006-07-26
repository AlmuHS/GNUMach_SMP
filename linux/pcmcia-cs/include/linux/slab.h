#ifndef _COMPAT_SLAB_H
#define _COMPAT_SLAB_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,2,0))
#include_next <linux/malloc.h>
#else
#include_next <linux/slab.h>
#endif

#endif /* _COMPAT_SLAB_H */
