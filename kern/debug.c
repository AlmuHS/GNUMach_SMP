/* 
 * Mach Operating System
 * Copyright (c) 1993 Carnegie Mellon University
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

#include <mach/xen.h>

#include <kern/printf.h>
#include <stdarg.h>

#include "cpu_number.h"
#include <kern/lock.h>
#include <kern/thread.h>

#include <kern/debug.h>

#include <machine/loose_ends.h>
#include <machine/model_dep.h>

extern void cnputc();

#if	MACH_KDB
extern int db_breakpoints_inserted;
#endif

#if NCPUS>1
simple_lock_data_t Assert_print_lock; /* uninited, we take our chances */
#endif

static void
do_cnputc(char c, vm_offset_t offset)
{
	cnputc(c);
}

void
Assert(char *exp, char *file, int line)
{
#if NCPUS > 1
  	simple_lock(&Assert_print_lock);
	printf("{%d} Assertion failed: file \"%s\", line %d\n", 
	       cpu_number(), file, line);
	simple_unlock(&Assert_print_lock);
#else
	printf("Assertion `%s' failed in file \"%s\", line %d\n",
		exp, file, line);
#endif

#if	MACH_KDB
	if (db_breakpoints_inserted)
#endif
	Debugger("assertion failure");
}

void SoftDebugger(message)
	char *	message;
{
	printf("Debugger invoked: %s\n", message);

#if	!MACH_KDB
	printf("But no debugger, continuing.\n");
	return;
#endif

#if	defined(vax) || defined(PC532)
	asm("bpt");
#endif	/* vax */

#ifdef	sun3
	current_thread()->pcb->flag |= TRACE_KDB;
	asm("orw  #0x00008000,sr");
#endif	/* sun3 */
#ifdef	sun4
	current_thread()->pcb->pcb_flag |= TRACE_KDB;
	asm("ta 0x81");
#endif	/* sun4 */

#if	defined(mips ) || defined(luna88k) || defined(i860) || defined(alpha)
	gimmeabreak();
#endif

#ifdef	i386
	asm("int3");
#endif
}

void Debugger(message)
	char *	message;
{
#if	!MACH_KDB
	panic("Debugger invoked, but there isn't one!");
#endif

	SoftDebugger(message);

	panic("Debugger returned!");
}

/* Be prepared to panic anytime,
   even before panic_init() gets called from the "normal" place in kern/startup.c.
   (panic_init() still needs to be called from there
   to make sure we get initialized before starting multiple processors.)  */
boolean_t		panic_lock_initialized = FALSE;
decl_simple_lock_data(,	panic_lock)

const char     		*panicstr;
int			paniccpu;

void
panic_init(void)
{
	if (!panic_lock_initialized)
	{
		panic_lock_initialized = TRUE;
		simple_lock_init(&panic_lock);
	}
}

#if ! MACH_KBD
extern boolean_t reboot_on_panic;
#endif

/*VARARGS1*/
void
panic(const char *s, ...)
{
	va_list	listp;

	panic_init();

	simple_lock(&panic_lock);
	if (panicstr) {
	    if (cpu_number() != paniccpu) {
		simple_unlock(&panic_lock);
		halt_cpu();
		/* NOTREACHED */
	    }
	}
	else {
	    panicstr = s;
	    paniccpu = cpu_number();
	}
	simple_unlock(&panic_lock);
	printf("panic");
#if	NCPUS > 1
	printf("(cpu %U)", paniccpu);
#endif
	printf(": ");
	va_start(listp, s);
	_doprnt(s, &listp, do_cnputc, 0, 0);
	va_end(listp);
	printf("\n");

#if	MACH_KDB
	Debugger("panic");
#else
# ifdef	MACH_HYP
	hyp_crash();
# else
	/* Give the user time to see the message */
	{
	  int i = 1000;		/* seconds */
	  while (i--)
	    delay (1000000);	/* microseconds */
	}

	halt_all_cpus (reboot_on_panic);
# endif	/* MACH_HYP */
#endif
}

/*
 * We'd like to use BSD's log routines here...
 */
/*VARARGS2*/
void
log(int level, const char *fmt, ...)
{
	va_list	listp;

#ifdef lint
	level++;
#endif
	va_start(listp, fmt);
	_doprnt(fmt, &listp, do_cnputc, 0, 0);
	va_end(listp);
}

unsigned char __stack_chk_guard [ sizeof (vm_offset_t) ] =
{
	[ sizeof (vm_offset_t) - 3 ] = '\r',
	[ sizeof (vm_offset_t) - 2 ] = '\n',
	[ sizeof (vm_offset_t) - 1 ] = 0xff,
};

void
__stack_chk_fail (void)
{
	panic("stack smashing detected");
}
