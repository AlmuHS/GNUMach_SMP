/* Declarations for the immediate console.

   Copyright (C) 2015 Free Software Foundation, Inc.

   This file is part of the GNU Mach.

   The GNU Mach is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2, or (at
   your option) any later version.

   The GNU Mach is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with the GNU Mach.  If not, see <http://www.gnu.org/licenses/>.  */

#ifndef	_IMMC_H_
#define	_IMMC_H_

#include <sys/types.h>

int immc_cnprobe(struct consdev *cp);
int immc_cninit(struct consdev *cp);
int immc_cngetc(dev_t dev, int wait);
int immc_cnputc(dev_t dev, int c);
void immc_romputc(char c);

#endif	/* _IMMC_H_ */
