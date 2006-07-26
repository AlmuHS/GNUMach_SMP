#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

#include <linux/posix_types.h>
#include <asm/types.h>

#ifndef __KERNEL_STRICT_NAMES

typedef __kernel_fd_set		fd_set;

#ifndef MACH_INCLUDE
typedef __kernel_dev_t		dev_t;
typedef __kernel_ino_t		ino_t;
typedef __kernel_mode_t		mode_t;
typedef __kernel_nlink_t	nlink_t;
#endif

#ifdef MACH_INCLUDE
#define off_t			long
#else
typedef __kernel_off_t		off_t;
#endif

typedef __kernel_pid_t		pid_t;

#ifdef MACH_INCLUDE
#define uid_t			unsigned short
#define gid_t			unsigned short
#define daddr_t			int
#else
typedef __kernel_uid_t		uid_t;
typedef __kernel_gid_t		gid_t;
typedef __kernel_daddr_t	daddr_t;
#endif

#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
typedef __kernel_loff_t		loff_t;
#endif

/*
 * The following typedefs are also protected by individual ifdefs for
 * historical reasons:
 */
#ifndef _SIZE_T
#define _SIZE_T
#ifndef MACH_INCLUDE
typedef __kernel_size_t		size_t;
#endif
#endif

#ifndef _SSIZE_T
#define _SSIZE_T
#ifndef MACH_INCLUDE
typedef __kernel_ssize_t	ssize_t;
#endif
#endif

#ifndef _PTRDIFF_T
#define _PTRDIFF_T
typedef __kernel_ptrdiff_t	ptrdiff_t;
#endif

#ifndef _TIME_T
#define _TIME_T
#ifdef MACH_INCLUDE
#define time_t			long
#else
typedef __kernel_time_t		time_t;
#endif
#endif

#ifndef _CLOCK_T
#define _CLOCK_T
typedef __kernel_clock_t	clock_t;
#endif

#ifndef _CADDR_T
#define _CADDR_T
#ifndef MACH_INCLUDE
typedef __kernel_caddr_t	caddr_t;
#endif
#endif

#ifndef MACH_INCLUDE
/* bsd */
typedef unsigned char		u_char;
typedef unsigned short		u_short;
typedef unsigned int		u_int;
typedef unsigned long		u_long;
#endif

/* sysv */
typedef unsigned char		unchar;
typedef unsigned short		ushort;
typedef unsigned int		uint;
typedef unsigned long		ulong;

#endif /* __KERNEL_STRICT_NAMES */

/*
 * Below are truly Linux-specific types that should never collide with
 * any application/library that wants linux/types.h.
 */

struct ustat {
	__kernel_daddr_t	f_tfree;
	__kernel_ino_t		f_tinode;
	char			f_fname[6];
	char			f_fpack[6];
};


/* Yes, this is ugly.  But that's why it is called glue code.  */

#define _MACH_SA_SYS_TYPES_H_


#endif /* _LINUX_TYPES_H */
