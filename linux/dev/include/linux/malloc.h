#ifndef _LINUX_MALLOC_H
#define _LINUX_MALLOC_H

#include <linux/mm.h>
#include <asm/cache.h>

#ifndef MACH_INCLUDE
#define kmalloc		linux_kmalloc
#define kfree		linux_kfree
#define kfree_s		linux_kfree_s
#endif

extern void *linux_kmalloc (unsigned int size, int priority);
extern void linux_kfree (void *obj);

#define linux_kfree_s(a,b) linux_kfree(a)

#endif /* _LINUX_MALLOC_H */
