/*
 * Communication functions
 * Copyright (C) 2008 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Barry deFreese.
 */
/*
 *     Communication functions.
 *
 */

#ifndef _COM_H_
#define _COM_H_

#include <mach/std_types.h>

/*
 * Set receive modem state from modem status register.
 */
extern void fix_modem_state(int unit, int modem_stat);

extern void comtimer(void * param);

/*
 * Modem change (input signals)
 */
extern void commodem_intr(int unit, int stat);

extern int comgetc(int unit);

#endif /* _COM_H_ */
