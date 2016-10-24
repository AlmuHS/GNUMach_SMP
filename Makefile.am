# Makefile for GNU Mach.

# Copyright (C) 2006, 2007, 2008, 2009, 2013 Free Software Foundation, Inc.

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
	-ffreestanding -nostdinc -imacros config.h

AM_CPPFLAGS += \
	-I$(systype) \
	-I. \
	-I$(top_srcdir)/$(systype) \
	-I$(top_srcdir)/$(systype)/include/mach/sa \
	-I$(top_srcdir)/include

AM_CFLAGS += \
	-fno-builtin-log

# Yes, this makes the eyes hurt.  But perhaps someone will finally take care of
# all that scruffy Mach code...  Also see <http://savannah.gnu.org/task/?5726>.
AM_CFLAGS += \
	-Wall

# We need the GNU-style inline
AM_CFLAGS += \
	-fgnu89-inline

# Much of the Mach code predates C99 and makes invalid assumptions about
# type punning.
AM_CFLAGS += \
	-fno-strict-aliasing

# The smashing stack protector might be enabled by default, but might emit
# unsuitable code.
if disable_smashing_stack_protector
AM_CFLAGS += \
	-fno-stack-protector
endif

#
# Silent build support.
#

AWK_V = $(AWK_V_$(V))
AWK_V_ = $(AWK_V_$(AM_DEFAULT_VERBOSITY))
AWK_V_0 = @echo "  AWK    $@";

GZIP_V = $(GZIP_V_$(V))
GZIP_V_ = $(GZIP_V_$(AM_DEFAULT_VERBOSITY))
GZIP_V_0 = @echo "  GZIP   $@";

NM_V = $(NM_V_$(V))
NM_V_ = $(NM_V_$(AM_DEFAULT_VERBOSITY))
NM_V_0 = @echo "  NM     $@";

MIGCOM_V = $(MIGCOM_V_$(V))
MIGCOM_V_ = $(MIGCOM_V_$(AM_DEFAULT_VERBOSITY))
MIGCOM_V_0 = @echo "  MIG    $@";

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

# Don't needlessly overwrite files whose contents haven't changed.
# This helps avoiding unnecessary recompilation cycles when keeping
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
gnumach_o_LINK = $(LD) $(LDFLAGS) -u _start -r -o $@
noinst_PROGRAMS += \
	gnumach.o

# This is the list of routines we decide is OK to steal from the C library.
clib_routines := htonl htons ntohl ntohs			\
		 udivdi3 __udivdi3 __umoddi3			\
		 __rel_iplt_start __rel_iplt_end		\
		 __ffsdi2					\
		 _START _start etext _edata end _end # actually ld magic, not libc.
gnumach-undef: gnumach.$(OBJEXT)
	$(NM_V) $(NM) -u $< | sed 's/  *U  *//' | sort -u > $@
MOSTLYCLEANFILES += gnumach-undef
gnumach-undef-bad: gnumach-undef Makefile
	$(AM_V_GEN) sed '$(foreach r,$(clib_routines),/^$r$$/d;)' $< > $@
MOSTLYCLEANFILES += gnumach-undef-bad
clib-routines.o: gnumach-undef gnumach-undef-bad
	$(AM_V_at) if test -s gnumach-undef-bad; \
	then cat gnumach-undef-bad; exit 2; else true; fi
	$(AM_V_CCLD) $(CCLD) $(LDFLAGS) -nostdlib -nostartfiles -r -static \
	  -o $@ `sed 's/^/-Wl,-u,/' < $<` -x c /dev/null -lc -lgcc
	@if nm $@ | grep __init_cpu_features; \
	then echo "Please install a 32bit libc without multiarch support (on Debian systems, the libc6-dev:i386 package containing /usr/lib/i386-linux-gnu/libc.a)". ; \
	false ; fi

gnumach_LINK = $(LD) $(LDFLAGS) $(LINKFLAGS) $(gnumach_LINKFLAGS) -o $@
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

dist-hook: dist-rm-CVS gen-ChangeLog

.PHONY: dist-rm-CVS
dist-rm-CVS:
# Try to be very safe with respect to spuriously removing various directories
# in case of an error.
	find $(distdir)/ -type d -name CVS | while read d; do	\
	  rm -f "$$d"/{Entries,Repository,Root,Tag} &&		\
	  rmdir "$$d";						\
	done

gen_start_commit = e227045b06d62ee7d2fbab9d5ade9030ff43170b
ChangeLog_files = ChangeLog ChangeLog.0 ChangeLog.00
.PHONY: gen-ChangeLog
gen-ChangeLog:
	$(AM_V_GEN)if test -d $(top_srcdir)/.git; then			\
	  (cd $(top_srcdir)/ &&						\
	  ./gitlog-to-changelog	--strip-tab				\
	    $(gen_start_commit).. &&					\
	  echo) >> $(distdir)/cl-t &&					\
	  for f in $(ChangeLog_files); do				\
	    (cd $(top_srcdir)/ &&					\
	    git show $(gen_start_commit):$$f) >> $(distdir)/cl-t &&	\
	    rm -f $(distdir)/$$f &&					\
	    mv $(distdir)/cl-t $(distdir)/$$f				\
	    || exit $$?;						\
	  done;								\
	fi

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
