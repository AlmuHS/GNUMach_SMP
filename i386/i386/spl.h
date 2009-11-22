/*
 * Copyright (c) 1995, 1994, 1993, 1992, 1991, 1990  
 * Open Software Foundation, Inc. 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation, and that the name of ("OSF") or Open Software 
 * Foundation not be used in advertising or publicity pertaining to 
 * distribution of the software without specific, written prior permission. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL OSF BE LIABLE FOR ANY 
 * SPECIAL, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES 
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN 
 * ACTION OF CONTRACT, NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING 
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE 
 */
/*
 * OSF Research Institute MK6.1 (unencumbered) 1/31/1995
 */

#ifndef	_MACHINE_SPL_H_
#define	_MACHINE_SPL_H_

/*
 *	This file defines the interrupt priority levels used by
 *	machine-dependent code.
 */

typedef int		spl_t;

extern spl_t	(splhi)(void);

extern spl_t	(spl0)(void);

extern spl_t	(spl1)(void);
extern spl_t	(splsoftclock)(void);

extern spl_t	(spl2)(void);

extern spl_t	(spl3)(void);

extern spl_t	(spl4)(void);
extern spl_t	(splnet)(void);
extern spl_t	(splhdw)(void);

extern spl_t	(spl5)(void);
extern spl_t	(splbio)(void);
extern spl_t	(spldcm)(void);

extern spl_t	(spl6)(void);
extern spl_t	(spltty)(void);
extern spl_t	(splimp)(void);

extern spl_t	(spl7)(void);
extern spl_t	(splclock)(void);
extern spl_t	(splsched)(void);
extern spl_t	(splhigh)(void);

extern spl_t	(splx)(spl_t n);
extern spl_t	(splx_cli)(spl_t n);

extern void splon (unsigned long n);

extern unsigned long sploff (void);

extern void setsoftclock (void);

/* XXX Include each other... */
#include <i386/ipl.h>

#endif	/* _MACHINE_SPL_H_ */
