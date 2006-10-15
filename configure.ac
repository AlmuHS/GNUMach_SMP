dnl Configure script for GNU Mach.

dnl Copyright 1997, 1998, 1999, 2004, 2006 Free Software Foundation, Inc.

dnl Permission to use, copy, modify and distribute this software and its
dnl documentation is hereby granted, provided that both the copyright
dnl notice and this permission notice appear in all copies of the
dnl software, derivative works or modified versions, and any portions
dnl thereof, and that both notices appear in supporting documentation.
dnl
dnl THE FREE SOFTWARE FOUNDATION ALLOWS FREE USE OF THIS SOFTWARE IN ITS
dnl "AS IS" CONDITION.  THE FREE SOFTWARE FOUNDATION DISCLAIMS ANY
dnl LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
dnl USE OF THIS SOFTWARE.

AC_PREREQ([2.57])

m4_include([version.m4])
AC_INIT([AC_PACKAGE_NAME], [AC_PACKAGE_VERSION], [AC_PACKAGE_BUGREPORT],
  [AC_PACKAGE_TARNAME])
AC_CONFIG_SRCDIR([kern/ipc_kobject.c])

AC_CONFIG_AUX_DIR([build-aux])

dnl Do not clutter the main build directory.
AM_INIT_AUTOMAKE([subdir-objects])

#
# Deduce the output variable `systype' from the configuration parameters.
#

AC_CANONICAL_HOST

m4_define([set_HOST], [[
  case $host_cpu in
    $2)] AM_CONDITIONAL([HOST_$1], [true]) [
      systype=$1;;
    *)] AM_CONDITIONAL([HOST_$1], [false]) [;;
  esac
]])

[systype=unsupported]
# Our supported architectures.
set_HOST([i386],[i?86])

[if test -f "$srcdir"/"$systype"/Makefrag.am; then :; else]
  AC_MSG_ERROR([unsupported CPU type $host_cpu])
[fi]

AC_SUBST([systype])

#
# Programs.
#

AC_PROG_AWK
AM_PROG_AS
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_CHECK_TOOL([AR], [ar])
AC_CHECK_TOOL([LD], [ld])
AC_CHECK_TOOL([NM], [nm])

AC_CHECK_TOOL([MIG], [mig], [mig])

dnl Needed for the Automake option `subdir-objects'.
AM_PROG_CC_C_O

dnl Makerules can make use of these.
AC_CHECK_PROG([GZIP], [gzip], [gzip], [gzip-not-found])
AC_CHECK_TOOL([STRIP], [strip])

dnl See below why we need to patch stuff during build...
AC_CHECK_PROG([PATCH], [patch], [patch], [patch-not-found])

#
# configure fragments.
#

# The test suite.
m4_include([tests/configfrag.ac])

# General options.
m4_include([configfrag.ac])

# Machine-specific configuration.

# i386.
[if [ "$systype" = i386 ]; then]
  m4_include([i386/configfrag.ac])
[fi]

# Linux code snarfed into GNU Mach.
dnl Strictly speaking, we could have a `linux' option too, but it's currently
dnl not possible to built a useful kernel without at least one Linux driver, so
dnl that's not really necessary.
dnl
dnl The following doesn't work because of ``configure: error: conditional
dnl "device_driver_floppy" was never defined.''.
dnl AC_ARG_ENABLE([linux-device-drivers],
dnl   AS_HELP_STRING([--disable-linux-device-drivers],
dnl    [disable any use of Linux's device drivers]))
dnl [if [ x"$enable_linux_device_drivers" != xno ]; then]
m4_include([linux/configfrag.ac])
dnl [fi]

#
# Output.
#

# Set up `machine' and `mach/machine' links in the build directory for easier
# header file location.
AC_CONFIG_LINKS([machine:${systype}/${systype}
		 mach/machine:${systype}/include/mach/${systype}])

AC_CONFIG_HEADER([config.h])

AC_CONFIG_FILES([Makefile version.c])

#
# The remaining ugly, dark corners...
#
# Attention, parents: don't show this to your children...
#

#
# config.status.dep.patch
#
# This is a (ugly --- I admit) bootstrap hack to get to-be-generated files
# created before any other source files are compiled.
#
# See <http://lists.gnu.org/archive/html/automake/2006-05/msg00038.html>.
#
# We don't use `BUILT_SOURCES' (as it was suggested in the follow-up message),
# as we also want things like `make SPECIFIC_TARGET' to work.
#
# This affair is especially ugly because internals are used (the `# dummy'
# tag): internals that may be subject to changes.  That's the reason why a
# real patch is being used here and not some `sed' magic: to make it fail
# loudly in case.
#
# For all source files create dependency files where it is stated that the
# Makefile depends on all to-be-generated files.  Later, when compiling the
# source files, these dependency files will be rewritten to contain the files's
# actual dependencies.  From then on this hack will be forgotten.  The choice
# of the Makefile as the file to depends on is not be the best one, because
# when running `make clean', the build system may decide to first build all the
# to-be-genereated files and afterwards remove them again, but it works and I
# don't want to spend more thoughts on this issue at the moment.  Feel free to
# suggest something better.  TODO.  See <http://savannah.gnu.org/bugs/?18011>
# --- ``GNU Mach: `make install-data' will build the ``to-be-generated
# files''''.
#

dnl AC_CONFIG_COMMANDS_POST([
dnl   sed -i -e \
dnl     's%#\ dummy%Makefile: $(filter-out $(DIST_SOURCES),$(SOURCES))%' \
dnl     config.status
dnl ])
AC_CONFIG_COMMANDS_POST([
  if "$PATCH" -f < "$srcdir"/config.status.dep.patch
  then] AC_MSG_NOTICE([Applied a patch to work around a deficiency in]
    [Automake.  See `configure.ac' for details.])
  [else] AC_MSG_ERROR([failed to patch using `config.status.dep.patch'.]
      [You have a serious problem.  Please contact <$PACKAGE_BUGREPORT>.])
  [fi
])


#
# Makefile.correct_output_files_for_.S_files.patch
#
# The used version of Automake might emit rules that don't put compiled (pre
# processed) Assembler files where they belong.  We're bitten by this bug
# because we use the Automake option `subdir-objects', to not clutter the main
# build directory with all the object files.  The problem is that the emitted
# rules will place those in the main build directory, but later, when creating
# the libraries, look for them in the subdirectories.
#
# See <http://sourceware.org/cgi-bin/gnatsweb.pl?cmd=view%20audit-trail&database=automake&pr=492>.
#
# Another note: even with versions of Automake that don't have this issue
# fixed, a correct rule is emitted for linux/src/arch/i386/lib/semaphore.S, see
# the comment by Thomas Schwinge on 2006-10-10 on the above bug report.
#
# Again, a real patch is being used here and not some `sed' magic, so that
# it'll fail loudly in case some internals we're relying on are changed.
#

AC_CONFIG_COMMANDS([frob_Makefile_for_correct_output_files_for_.S_files],[
  # Do we have a version of Automake that doesn't have this bug?
  if grep -q CPPASCOMPILE.\*-c.\*-o < "$srcdir"/Makefile.in
  then] AC_MSG_NOTICE([... not a problem with the used version of Automake.])
  [# We don't.
  elif "$PATCH" -f \
    < "$srcdir"/Makefile.correct_output_files_for_.S_files.patch
  then]
    AC_MSG_NOTICE([Applied a patch to fix a bug in your version of Automake.]
      [The applied fix should work in most cases.  You could also consider]
      [updating to Automake 1.10 or better.])
  [else]
    AC_MSG_ERROR([failed to patch using ]
      [`Makefile.correct_output_files_for_.S_files.patch'.]
      [You have a serious problem.  Please contact <$PACKAGE_BUGREPORT>.])
  [fi
],[
  PACKAGE_BUGREPORT=$PACKAGE_BUGREPORT
  PATCH=$PATCH
])


#
# Makefile.dependency_tracking_for_.S_files.patch
#
# The used version of Automake might not support automatic dependency tracking
# for (preprocessed) Assembler source files.  We might add such support here.
# But since adding such support is nontrivial and the nonexistent dependency
# tracking (missing only for .S files) is not considered a fundamental problem,
# we save the costs of adding such support here.  You should instead consider
# updating to Automake 1.10 or better.
#

AC_CONFIG_COMMANDS([frob_Makefile_for_dependency_tracking_for_.S_files],[
  # Do we have a version of Automake that doesn't have this bug?
  if grep -q CPPASCOMPILE.\*depbase < "$srcdir"/Makefile.in
  then] AC_MSG_NOTICE([... not a problem with the used version of Automake.])
  [# We don't.
  else]
    AC_MSG_NOTICE([Your build of GNU Mach won't have dependency tracking for]
      [(preprocessed) Assembler source files.  Consider updating to Automake]
      [1.10 or better if you need such support.])
dnl In case someone feels like completing this instead of simply using a fixed
dnl version of Automake...
dnl    if "$PATCH" -f; then :; else]
dnl      AC_MSG_ERROR([failed to patch using]
dnl        [`Makefile.dependency_tracking_for_.S_files.patch'.]
dnl        [You have a serious problem.  Please contact <$PACKAGE_BUGREPORT>.])
dnl    [fi < "$srcdir"/Makefile.dependency_tracking_for_.S_files.patch
dnl
dnl    # Change the Makefile to care for dependencies of .S files.
dnl    ( cd "$srcdir"/ &&
dnl      find ./ -name \*.S ) | \
dnl        sed -e 's%^\./%%' -e 's%\.S$%%' -e 's%^\(.*\)/%\1 %' | \
dnl	  while read d f; do
dnl	    echo &&
dnl	    echo "$d"/"$f"'.$(OBJEXT): '"$d"'/$(am__dirstamp) ' \
dnl	      "$d"'/$(DEPDIR)/$(am__dirstamp)' &&
dnl	    echo 'include '"$d"'/$(DEPDIR)/'"$f"'.Po' &&
dnl
dnl	    # Create the dependency files if they don't exist so far.
dnl	    if test -f "$d"/"$DEPDIR"/"$f"'.Po'; then :; else
dnl	      echo '# dummy' > "$d"/"$DEPDIR"/"$f"'.Po'
dnl	    fi
dnl	  done >> Makefile
dnl
dnl TODO.  linux/src/arch/i386/lib/.deps/liblinux_a-semaphore.Po vs.
dnl linux/src/arch/i386/lib/.deps/semaphore.Po.  Because of the above
dnl mentioned special rule for `semaphore.S' the .S.o rule is not used.
dnl
dnl gcc -nostdinc -imacros config.h -Ii386 -I. -I../i386 -I../include -I../i386/include/mach/sa -DASSEMBLER -g -O2 -c ../i386/i386at/boothdr.S
dnl
dnl Makefile
dnl
dnl COMPILE = $(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS)
dnl .c.o:
dnl         depbase=`echo $@ | sed 's|[^/]*$$|$(DEPDIR)/&|;s|\.o$$||'`; \
dnl         if $(COMPILE) -MT $@ -MD -MP -MF "$$depbase.Tpo" -c -o $@ $<; \
dnl         then mv -f "$$depbase.Tpo" "$$depbase.Po"; else rm -f "$$depbase.Tpo"; exit 1; fi
dnl #       source='$<' object='$@' libtool=no \
dnl #       DEPDIR=$(DEPDIR) $(CCDEPMODE) $(depcomp) \
dnl #       $(COMPILE) -c -o $@ $<
dnl
dnl CCAS = gcc
dnl CCASCOMPILE = $(CCAS) $(AM_CCASFLAGS) $(CCASFLAGS)
dnl .S.o:
dnl        $(CCASCOMPILE) -c $<
dnl
dnl source=../i386/i386at/boothdr.S object=i386/i386at/boothdr.o libtool=no DEPDIR=.deps depmode=gcc3 ../depcomp gcc -nostdinc -imacros config.h -Ii386 -I. -I../i386 -I../include -I../i386/include/mach/sa -DASSEMBLER -g -O2 -c -o i386/i386at/boothdr.o ../i386/i386at/boothdr.S
dnl ... creates a correct i386/i386at/.deps/boothdr.Po.
  [fi
dnl ],[
dnl  DEPDIR=$DEPDIR
dnl  PACKAGE_BUGREPORT=$PACKAGE_BUGREPORT
dnl  PATCH=$PATCH
])

#
# Fire.
#

AC_OUTPUT
