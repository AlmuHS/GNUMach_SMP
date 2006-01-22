dnl Device driver options for i386
dnl Copyright 1997, 1999, 2004, 2006 Free Software Foundation, Inc.

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

m4_include([../../version.m4])

AC_INIT([PACKAGE], [VERSION])
AC_CONFIG_SRCDIR([dev/include/linux/autoconf.h])
AC_PREREQ(2.57)

m4_sinclude([../../Drivers.macros])
m4_sinclude([../../aclocal.m4])

AC_CONFIG_HEADER([device-drivers.h])

dnl Check the cpu type.

AC_CANONICAL_HOST

dnl Which host CPU
AC_DEFUN([hurd_host_CPU], [
  AC_DEFINE([CONFIG_M$1], [], [$1])
  AC_DEFINE([CPU], [$1], [CPU])
])


case "$host_cpu" in
i386) hurd_host_CPU([386]) ;;
i486) hurd_host_CPU([486]) ;;
i586) hurd_host_CPU([586]) ;;
i686) hurd_host_CPU([686]) ;;
*)    hurd_host_CPU([486]) ;;
esac

hurd_SYSTYPE

AC_CHECK_TOOL([CC], [gcc])
AC_PROG_CC

AC_CHECK_TOOL([LD], [ld])
AC_SUBST([LDFLAGS])


dnl Aliases have to come first.

AC_DRIVER_ALIAS([3c501], [el1])
AC_DRIVER_ALIAS([3c503], [el2])
AC_DRIVER_ALIAS([3c509], [el3])
AC_DRIVER_ALIAS([3c579], [el3])
AC_DRIVER_ALIAS([3c59x], [vortex])
AC_DRIVER_ALIAS([3c90x], [vortex])
AC_DRIVER_ALIAS([hpj2577], [hp100])
AC_DRIVER_ALIAS([hpj2573], [hp100])
AC_DRIVER_ALIAS([hpj2585], [hp100])
AC_DRIVER_ALIAS([hp27248b], [hp100])
AC_DRIVER_ALIAS([eth32], [eth16i])
AC_DRIVER_ALIAS([znote], [znet])
AC_DRIVER_ALIAS([de100], [depca])
AC_DRIVER_ALIAS([de101], [depca])
AC_DRIVER_ALIAS([de200], [depca])
AC_DRIVER_ALIAS([de201], [depca])
AC_DRIVER_ALIAS([de202], [depca])
AC_DRIVER_ALIAS([de210], [depca])
AC_DRIVER_ALIAS([de422], [depca])
AC_DRIVER_ALIAS([de203], [ewrk3])
AC_DRIVER_ALIAS([de204], [ewrk3])
AC_DRIVER_ALIAS([de205], [ewrk3])
AC_DRIVER_ALIAS([de425], [de4x5])
AC_DRIVER_ALIAS([de434], [de4x5])
AC_DRIVER_ALIAS([de435], [de4x5])
AC_DRIVER_ALIAS([de450], [de4x5])
AC_DRIVER_ALIAS([de500], [de4x5])
AC_DRIVER_ALIAS([3c507], [el16])
AC_DRIVER_ALIAS([3c505], [elplus])
AC_DRIVER_ALIAS([ne1000], [ne2000])
AC_DRIVER_ALIAS([ne], [ne2000])
AC_DRIVER_ALIAS([at1500], [lance])
AC_DRIVER_ALIAS([ne2100], [lance])
AC_DRIVER_ALIAS([tulip], [elcp])
AC_DRIVER_ALIAS([rtl8129], [rtl8139])
AC_DRIVER_ALIAS([aha2825], [aha152x])
AC_DRIVER_ALIAS([ncr53c400], [ncr5380])
AC_DRIVER_ALIAS([t128f], [t128])
AC_DRIVER_ALIAS([t228], [t128])
AC_DRIVER_ALIAS([am79c974], [am53c974])
AC_DRIVER_ALIAS([dtc3180], [dtc3280])
AC_DRIVER_ALIAS([dc390w], [ncr53c8xx])
AC_DRIVER_ALIAS([dc390u], [ncr53c8xx])
AC_DRIVER_ALIAS([dc390f], [ncr53c8xx])
AC_DRIVER_ALIAS([dc390], [dc390t])
AC_DRIVER_ALIAS([epic100], [epic])


dnl Kinds of drivers that have gobs of source files that get brought in.

AC_DRIVER_CLASS([scsi], [CONFIG_SCSI], [ \
  constants.o hosts.o scsi.o scsi_ioctl.o scsi_proc.o scsicam.o sd.o \
  sd_ioctl.o sr.o sr_ioctl.o])

AC_DRIVER_CLASS([net], [CONFIG_INET], [ \
  auto_irq.o net.o Space.o dev.o net_init.o pci-scan.o])

dnl Strictly speaking, we could have a `linux' option too, but it's
dnl not possible to built a useful kernel without at least one Linux
dnl driver, so that's not really necessary.


dnl
dnl linux_DRIVER(machname, MACRO, linuxname, class)
dnl
AC_DEFUN([linux_DRIVER], [
  AC_DRIVER_ALIAS([$3], [$1])
  AC_DRIVER([$1], [CONFIG_$2], [$3.o], [$4])
])dnl

dnl non-SCSI Disk controllers

AC_DRIVER([floppy], [CONFIG_BLK_DEV_FD], [floppy.o])
AC_DRIVER([ide], [CONFIG_BLK_DEV_IDE], [ \
  cmd640.o ide-cd.o ide.o rz1000.o triton.o])


dnl SCSI Disk controllers

linux_DRIVER([advansys], [SCSI_ADVANSYS], [advansys], [scsi])
linux_DRIVER([buslogic], [SCSI_BUSLOGIC], [BusLogic], [scsi])
linux_DRIVER([u1434f], [SCSI_U14_34F], [u14-34f], [scsi])
linux_DRIVER([ultrastor], [SCSI_ULTRASTOR], [ultrastor], [scsi])
linux_DRIVER([aha152x], [SCSI_AHA152X], [aha152x], [scsi])
linux_DRIVER([aha1542], [SCSI_AHA1542], [aha1542], [scsi])
linux_DRIVER([aha1740], [SCSI_AHA1740], [aha1740], [scsi])
linux_DRIVER([aic7xxx], [SCSI_AIC7XXX], [aic7xxx], [scsi])
linux_DRIVER([futuredomain], [SCSI_FUTURE_DOMAIN], [fdomain], [scsi])
linux_DRIVER([in2000], [SCSI_IN2000], [in2000], [scsi])
linux_DRIVER([ncr5380], [SCSI_GENERIC_NCR5380], [g_NCR5380], [scsi])
linux_DRIVER([ncr53c406a], [SCSI_NCR53C406A], [NCR53c406a], [scsi])
linux_DRIVER([pas16], [SCSI_PASS16], [pas16], [scsi])
linux_DRIVER([seagate], [SCSI_SEAGATE], [seagate], [scsi])
linux_DRIVER([t128], [SCSI_T128], [t128], [scsi])
linux_DRIVER([ncr53c7xx], [SCSI_NCR53C7xx], [53c78xx], [scsi])
linux_DRIVER([eatadma], [SCSI_EATA_DMA], [eata_dma], [scsi])
linux_DRIVER([eatapio], [SCSI_EATA_PIO], [eata_pio], [scsi])
linux_DRIVER([wd7000], [SCSI_7000FASST], [wd7000], [scsi])
linux_DRIVER([eata], [SCSI_EATA], [eata], [scsi])
linux_DRIVER([am53c974], [SCSI_AM53C974], [AM53C974], [scsi])
linux_DRIVER([dtc3280], [SCSI_DTC3280], [dtc], [scsi])
linux_DRIVER([ncr53c8xx], [SCSI_NCR53C8XX], [ncr53c8xx], [scsi])
linux_DRIVER([dc390t], [SCSI_DC390T], [tmscsim], [scsi])
linux_DRIVER([ppa], [SCSI_PPA], [ppa], [scsi])
linux_DRIVER([qlogicfas], [SCSI_QLOGIC_FAS], [qlogicfas], [scsi])
linux_DRIVER([qlogicisp], [SCSI_QLOGIC_ISP], [qlogicisp], [scsi])
linux_DRIVER([gdth], [SCSI_GDTH], [gdth], [scsi])
dnl Dirty implementation...

AC_ARG_ENABLE([flashpoint],
  AS_HELP_STRING([--enable-flashpoint], [enable SCSI flashpoint [[default=no]]]),
  [test x"$enableval" = xno &&
   AC_DEFINE([CONFIG_SCSI_OMIT_FLASHPOINT], [], [scsi omit flashpoint])])


dnl Ethernet controllers

dnl FIXME: Can't be enabled since it is a pcmcia driver, and we don't
dnl have that kind of fluff.
dnl
dnl linux_DRIVER([cb_shim], [CB_SHIM], [cb_shim], [net])

linux_DRIVER([starfire], [STARFIRE], [starfire], [net])
linux_DRIVER([sundance], [SUNDANCE], [sundance], [net])
linux_DRIVER([winbond_840], [WINBOND840], [winbond-840], [net])
linux_DRIVER([hamachi], [HAMACHI], [hamachi], [net])
linux_DRIVER([intel_gige], [INTEL_GIGE], [intel-gige], [net])
linux_DRIVER([natsemi], [NATSEMI], [natsemi], [net])
linux_DRIVER([myson803], [MYSON803], [myson803], [net])
linux_DRIVER([ns820], [NS820], [ns820], [net])

AC_DRIVER([ne2000], [CONFIG_NE2000], [ne.o 8390.o], [net])
AC_DRIVER([el2], [CONFIG_EL2], [3c503.o 8390.o], [net])
linux_DRIVER([el3], [EL3], [3c509], [net])
AC_DRIVER([wd80x3], [CONFIG_WD80x3], [wd.o 8390.o], [net])
linux_DRIVER([el1], [EL1], [3c501], [net])
AC_DRIVER([ul], [CONFIG_ULTRA], [smc-ultra.o 8390.o], [net])
AC_DRIVER([ul32], [CONFIG_ULTRA32], [smc-ultra32.o 8390.o], [net])
AC_DRIVER([hplanplus], [CONFIG_HPLAN_PLUS], [hp-plus.o 8390.o], [net])
AC_DRIVER([hplan], [CONFIG_HPLAN], [hp.o 8390.o], [net])
linux_DRIVER([vortex], [VORTEX], [3c59x], [net])
linux_DRIVER([seeq8005], [SEEQ8005], [seeq8005], [net])
linux_DRIVER([hp100], [HP100], [hp100], [net])
AC_DRIVER([ac3200], [CONFIG_AC3200], [ac3200.o 8390.o], [net])
AC_DRIVER([e2100], [CONFIG_E2100], [e2100.o 8390.o], [net])
linux_DRIVER([at1700], [AT1700], [at1700], [net])
linux_DRIVER([eth16i], [ETH16I], [eth16i], [net])
linux_DRIVER([znet], [ZNET], [znet], [net])
linux_DRIVER([eexpress], [EEXPRESS], [eexpress], [net])
linux_DRIVER([eexpresspro], [EEXPRESS_PRO], [eepro], [net])
linux_DRIVER([eexpresspro100], [EEXPRESS_PRO100B], [eepro100], [net])
linux_DRIVER([depca], [DEPCA], [depca], [net])
linux_DRIVER([ewrk3], [EWRK3], [ewrk3], [net])
linux_DRIVER([de4x5], [DE4X5], [de4x5], [net])
linux_DRIVER([apricot], [APRICOT], [apricot], [net])
linux_DRIVER([wavelan], [WAVELAN], [wavelan], [net])
linux_DRIVER([el16], [EL16], [3c507], [net])
linux_DRIVER([elplus], [ELPLUS], [3c505], [net])
linux_DRIVER([de600], [DE600], [de600], [net])
linux_DRIVER([de620], [DE620], [de620], [net])
linux_DRIVER([skg16], [SK_G16], [sk_g16], [net])
linux_DRIVER([ni52], [NI52], [ni52], [net])
linux_DRIVER([ni65], [NI65], [ni65], [net])
linux_DRIVER([atp], [ATP], [atp], [net])
linux_DRIVER([lance], [LANCE], [lance], [net])
linux_DRIVER([elcp], [DEC_ELCP], [tulip], [net])
linux_DRIVER([fmv18x], [FMV18X], [fmv18x], [net])
linux_DRIVER([3c515], [3C515], [3c515], [net])
linux_DRIVER([pcnet32], [PCNET32], [pcnet32], [net])
AC_DRIVER([ne2kpci], [CONFIG_NE2K_PCI], [ne2k-pci.o 8390.o], [net])
linux_DRIVER([yellowfin], [YELLOWFIN], [yellowfin], [net])
linux_DRIVER([rtl8139], [RTL8139], [rtl8139], [net])
linux_DRIVER([epic], [EPIC], [epic100], [net])
linux_DRIVER([tlan], [TLAN], [tlan], [net])
linux_DRIVER([viarhine], [VIA_RHINE], [via-rhine], [net])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
