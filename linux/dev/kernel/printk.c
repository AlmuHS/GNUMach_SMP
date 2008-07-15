/*
 * Linux kernel print routine.
 * Copyright (C) 1995 Shantanu Goel.
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
 */

/*
 *  linux/kernel/printk.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

#define MACH_INCLUDE
#include <stdarg.h>
#include <asm/system.h>
#include <kern/assert.h>
#include <device/cons.h>

static char buf[2048];

#define DEFAULT_MESSAGE_LOGLEVEL 4
#define DEFAULT_CONSOLE_LOGLEVEL 7

int console_loglevel = DEFAULT_CONSOLE_LOGLEVEL;

int
printk (char *fmt, ...)
{
  va_list args;
  int n, flags;
  char *p, *msg, *buf_end;
  static int msg_level = -1;
  
  save_flags (flags);
  cli ();
  va_start (args, fmt);
  n = vsnprintf (buf + 3, sizeof (buf) - 3, fmt, args);
  assert (n <= sizeof (buf) - 3);
  buf_end = buf + 3 + n;
  va_end (args);
  for (p = buf + 3; p < buf_end; p++)
    {
      msg = p;
      if (msg_level < 0)
	{
	  if (p[0] != '<' || p[1] < '0' || p[1] > '7' || p[2] != '>')
	    {
	      p -= 3;
	      p[0] = '<';
	      p[1] = DEFAULT_MESSAGE_LOGLEVEL + '0';
	      p[2] = '>';
	    }
	  else
	    msg += 3;
	  msg_level = p[1] - '0';
	}
      for (; p < buf_end; p++)
	if (*p == '\n')
	  break;
      if (msg_level < console_loglevel)
	  while (msg <= p)
	    cnputc (*msg++);
      if (*p == '\n')
	msg_level = -1;
    }
  restore_flags (flags);
  return n;
}
