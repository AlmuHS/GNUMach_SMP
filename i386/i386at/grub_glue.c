/*
 * Copyright (c) 2014 Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <kern/printf.h>
#include <stdarg.h>
#include <i386/vm_param.h>

#include <grub/glue.h>
#include <grub/acpi.h>

#define GRUB_DEBUG 0

void
grub_real_dprintf (const char *file, const int line, const char *condition,
		   const char *fmt, ...)
{
#if GRUB_DEBUG
  va_list	listp;
  va_start(listp, fmt);
  vprintf (fmt, listp);
  va_end(listp);
#endif
}

void
grub_millisleep (grub_uint32_t ms)
{
  /* Do nothing.  */
}

struct grub_acpi_rsdp_v20 *
grub_acpi_get_rsdpv2 (void)
{
  return grub_machine_acpi_get_rsdpv2 ();
}

struct grub_acpi_rsdp_v10 *
grub_acpi_get_rsdpv1 (void)
{
  return grub_machine_acpi_get_rsdpv1 ();
}

/* Simple checksum by summing all bytes. Used by ACPI and SMBIOS. */
grub_uint8_t
grub_byte_checksum (void *base, grub_size_t size)
{
  grub_uint8_t *ptr;
  grub_uint8_t ret = 0;
  for (ptr = (grub_uint8_t *) base; ptr < ((grub_uint8_t *) base) + size;
       ptr++)
    ret += *ptr;
  return ret;
}
