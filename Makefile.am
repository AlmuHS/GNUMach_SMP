# Makefile for GNU Mach.

# Copyright (C) 2006, 2007, 2008 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2, or (at your option) any later
# version.
# 
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

#
# Various definitions of the Automake environment.
#
# These will be augmented below.
#

SUBDIRS =
DIST_SUBDIRS =
EXTRA_DIST =
DISTCHECK_CONFIGURE_FLAGS =
noinst_LIBRARIES =
noinst_PROGRAMS =
TESTS=
info_TEXINFOS =
MOSTLYCLEANFILES =
DISTCLEANFILES =
AM_CPPFLAGS =
AM_CCASFLAGS =
AM_CFLAGS =
AM_LDFLAGS =

#
# Compilation flags
#

AM_CPPFLAGS += \
	-nostdinc -imacros config.h

AM_CPPFLAGS += \
	-I$(systype) \
	-I. \
	-I$(top_srcdir)/$(systype) \
	-I$(top_srcdir)/$(systype)/include/mach/sa \
	-I$(top_srcdir)/include

# Yes, this makes the eyes hurt.  But perhaps someone will finally take care of
# all that scruffy Mach code...  Also see <http://savannah.gnu.org/task/?5726>.
AM_CFLAGS += \
	-Wall

# See <http://lists.gnu.org/archive/html/bug-hurd/2006-01/msg00148.html>.
AM_CFLAGS += \
	-fno-strict-aliasing

# The smashing stack protector might be enabled by default, but might emit
# unsuitable code.
if disable_smashing_stack_protector
AM_CFLAGS += \
	-fno-stack-protector
endif

#
# MIG Setup.
#

# MIGCOM.
MIGCOM = $(MIG) -n -cc cat - /dev/null

# We need this because we use $(CPP) to preprocess MIG .defs files.
CPP = @CPP@ -x c

#
# Other Tools' Configuration.
#

# Don't needlessly overwrite files that whose contents haven't changed.  This
# helps for avoinding unneccessary recompilation cycles when keeping
# cross-compilation toolchains up-to-date.  Thus, unconditionally use the
# `install-sh' that is supplied by GNU Automake 1.10.1, as the GNU Coreutils
# one doesn't provide this functionality yet (TODO: change that).  TODO:
# `build-aux' is hardcoded.
install_sh = $(SHELL) $(abs_srcdir)/build-aux/install-sh -C
INSTALL = $(install_sh)

#
# The main kernel functionality.
#

noinst_LIBRARIES += \
	libkernel.a
libkernel_a_SOURCES =
nodist_libkernel_a_SOURCES =
MOSTLYCLEANFILES += \
	$(nodist_libkernel_a_SOURCES)

gnumach_o_LDADD = \
	libkernel.a

gnumach_SOURCES =
gnumach_LINKFLAGS =

# Makerules: how to do some things.
include Makerules.am

# Main Makefile fragment.
include Makefrag.am

# Test suite.
include tests/Makefrag.am

# Documentation.
include doc/Makefrag.am

#
# Kernel Image
#

# We need the following junk because of the include-files-from-libc.a magic.
# TODO.  Is the following kosher from a Automake point of view?  (I.e. a
# program `gnumach.o' that is then later used again as an object file.)
gnumach_o_SOURCES =
# TODO.  ``-u _start''.  System dependent?
gnumach_o_LINK = $(LD) -u _start -r -o $@
noinst_PROGRAMS += \
	gnumach.o

# This is the list of routines we decide is OK to steal from the C library.
clib_routines := memcmp memcpy memmove memset bcopy bzero	\
		 strchr strstr strsep strpbrk strtok		\
		 htonl htons ntohl ntohs			\
		 udivdi3 __udivdi3				\
		 __rel_iplt_start __rel_iplt_end		\
		 _START _start etext _edata end _end # actually ld magic, not libc.
gnumach-undef: gnumach.$(OBJEXT)
	$(NM) -u $< | sed 's/  *U  *//' | sort -u > $@
MOSTLYCLEANFILES += gnumach-undef
gnumach-undef-bad: gnumach-undef Makefile
	sed '$(foreach r,$(clib_routines),/^$r$$/d;)' $< > $@
MOSTLYCLEANFILES += gnumach-undef-bad
clib-routines.o: gnumach-undef gnumach-undef-bad
	if test -s gnumach-undef-bad; \
	then cat gnumach-undef-bad; exit 2; else true; fi
	$(CCLD) -nostdlib -nostartfiles -r -static \
	  -o $@ `sed 's/^/-Wl,-u,/' < $<` -x c /dev/null -lc -lgcc

gnumach_LINK = $(LD) $(LINKFLAGS) $(gnumach_LINKFLAGS) -o $@
gnumach_LDADD = gnumach.o clib-routines.o

#
# Installation.
#

exec_bootdir = \
	$(exec_prefix)/boot
exec_boot_PROGRAMS = \
	gnumach

#
# Building a distribution.
#

EXTRA_DIST += \
	config.status.dep.patch

EXTRA_DIST += \
	DEVELOPMENT

dist-hook:
# Try to be very safe with respect to spuriously removing various directories
# in case of an error.
	find $(distdir)/ -type d -name CVS | while read d; do	\
	  rm -f "$$d"/{Entries,Repository,Root,Tag} &&		\
	  rmdir "$$d";						\
	done

DISTCLEANFILES += \
	Makefile.orig \
	config.status.orig

#
# Legacy support.
#

install-headers: install-data
	@echo '*****************************************************'
	@echo '* As you can see above, I was so kind to rewrite your'
	@echo '* `make $@'\'
	@echo '* into'
	@echo '* `make $^'\'
	@echo '* which is how it is to be spelled these days.'
	@echo '*'
	@echo '* Please get your instructions fixed.'
	@echo '*****************************************************'
	@echo
	@echo 'Penalty:'
	sleep 20
