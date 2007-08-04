#ifndef _I386_SMP_H
#define _I386_SMP_H

#include <machine/cpu_number.h>

#define smp_processor_id() cpu_number()

#endif /* _I386_SMP_H */
