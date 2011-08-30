#ifndef _COMPAT_INIT_H
#define _COMPAT_INIT_H

#include <linux/compiler.h>

#ifdef MODULE
#define __exitused
#else
#define __exitused  __used
#endif

#define __init
#define __initdata
#define __exit          __exitused __cold notrace
#define __exitdata
#define __devinit
#define __devinitdata
#define __devexit
#define __devexitdata

#ifndef module_init
#define module_init(x)
#define module_exit(x)
#endif

#ifndef __devexit_p
#define __devexit_p(x) (x)
#endif

#endif /* _COMPAT_INIT_H */
