/* smp.c - Template for generic SMP controller for Mach.
   Copyright (C) 2020 Free Software Foundation, Inc.
   Written by Almudena Garcia Jurado-Centurion

   This file is part of GNU Mach.

   GNU Mach is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GNU Mach is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA. */

#include <kern/smp.h>
#include <machine/smp.h>
#include <stdint.h>

struct smp_data {
    uint8_t num_cpus;
} smp_info;

/*
 * smp_set_numcpus: initialize the number of cpus in smp_info structure
 */

void smp_set_numcpus(uint8_t numcpus)
{
   smp_info.num_cpus = numcpus;
}

/*
 * smp_get_numcpus: returns the number of cpus existing in the machine
 */
uint8_t smp_get_numcpus(void)
{
   uint8_t numcpus = smp_info.num_cpus;

   if (numcpus == 0)
      return 1; /* Although SMP doesn't find cpus, always there are at least one. */
   else
      return numcpus;
}
