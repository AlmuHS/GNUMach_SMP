dnl Configure script for GNU Mach.

dnl Copyright (C) 1997, 1998, 1999, 2004, 2006, 2007, 2008, 2010 Free Software
dnl Foundation, Inc.

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

AM_INIT_AUTOMAKE(
  [1.10.2]
dnl Don't define `PACKAGE' and `VERSION'.
  [no-define]
dnl Do not clutter the main build directory.
  [subdir-objects]
dnl We require GNU make.
  [-Wall -Wno-portability]
)

#
# Deduce the architecture we're building for.
#
# TODO: Should we also support constructs like `i686_xen-pc-gnu' or
# `i686-pc_xen-gnu'?

AC_CANONICAL_HOST

AC_ARG_ENABLE([platform],
  AS_HELP_STRING([--enable-platform=PLATFORM], [specify the platform to build a
    kernel for.  Defaults to `at' for `i?86'.  The other possibility is
    `xen'.]),
  [host_platform=$enable_platform],
  [host_platform=default])
[# Supported configurations.
case $host_platform:$host_cpu in
  default:i?86)
    host_platform=at;;
  at:i?86 | xen:i?86)
    :;;
  *)]
    AC_MSG_ERROR([unsupported combination of cpu type `$host_cpu' and platform
      `$host_platform'.])[;;
esac]
AC_SUBST([host_platform])

[# This is used in a few places.
case $host_cpu in
  i?86)
    systype=i386;;
  *)
    systype=$host_cpu;;
esac]
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

# Default set of device drivers.
AC_ARG_ENABLE([device-drivers],
  AS_HELP_STRING([--enable-device-drivers=WHICH], [specify WHICH (on `ix86-at'
    one of `default', `qemu', `none') to preset a certain subset of all
    available device drivers, as indicated by the below-metioned ``enabled
    ...'' comments; you can then still use further `--enable-*' or
    `--disable-*' options to refine the selection of drivers to include in
    order to choose only those you actually want to have enabled]))
[case $enable_device_drivers in
  '')
    enable_device_drivers=default;;
  no)
    enable_device_drivers=none;;
  default | none | qemu)
    :;;
  *)]
    AC_MSG_ERROR([invalid choice of]
      [`--enable-device-drivers=$enable_device_drivers'.])
    [;;
esac]

# Platform-specific configuration.

# PC AT.
# TODO.  Currently handled in `i386/configfrag.ac'.

# Xen.
m4_include([xen/configfrag.ac])

# Machine-specific configuration.

# ix86.
m4_include([i386/configfrag.ac])

# General options.
m4_include([configfrag.ac])

# Linux code snarfed into GNU Mach.
m4_include([linux/configfrag.ac])

#
# Compiler features.
#

# Smashing stack protector.
[ssp_possible=yes]
AC_MSG_CHECKING([whether the compiler accepts `-fstack-protector'])
# Is this a reliable test case?
AC_LANG_CONFTEST([[void foo (void) { volatile char a[8]; a[3]; }]])
[# `$CC -c -o ...' might not be portable.  But, oh, well...  Is calling
# `ac_compile' like this correct, after all?
if eval "$ac_compile -S -fstack-protector -o conftest.s" 2> /dev/null; then]
  AC_MSG_RESULT([yes])
  [# Should we clear up other files as well, having called `AC_LANG_CONFTEST'?
  rm -f conftest.s
else
  ssp_possible=no]
  AC_MSG_RESULT([no])
[fi
# Need that, because some distributions ship compilers that include
# `-fstack-protector' in the default specs.]
AM_CONDITIONAL([disable_smashing_stack_protector],
  [[[ x"$ssp_possible" = xyes ]]])

#
# Output.
#

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
# For all shipped source files a dependency file is tried to be created where
# it is simply stated that the respective source file depends on _all_
# to-be-generated files.  Depending on all of them doesn't do any harm, as they
# will nevertheless have to be created, sooner or later.  The problem is, that
# `config.status' doesn't know about the source file of the file it is
# currently creating the dependency file for.  So we have it do an educated
# guess...  Later, when compiling the source files, these dependency files will
# be rewritten to contain the files's actual dependencies.  From then on this
# bootstrap hack will be forgotten.
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
# Fire.
#

AC_OUTPUT
