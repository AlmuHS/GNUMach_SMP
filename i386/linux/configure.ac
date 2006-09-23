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

AC_INIT([PACKAGE], [VERSION], [bug-hurd@gnu.org], [gnumach])
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

#
# Options.
#
AC_ARG_ENABLE([default-device-drivers],
AS_HELP_STRING([--disable-default-device-drivers], [have all device drivers
disabled that can be disabled and would otherwise have been enabled by default;
then use the `--enable-*' options to enable only those you actually want to
have enabled]))
[if test x"$enable_default_device_drivers" != xno
then enable_default_device_drivers=yes
fi]

dnl Kinds of drivers that have gobs of source files that get brought in.

AC_DRIVER_CLASS([scsi], [CONFIG_SCSI], [ \
  constants.o hosts.o scsi.o scsi_ioctl.o scsi_proc.o scsicam.o sd.o \
  sd_ioctl.o sr.o sr_ioctl.o])

AC_DRIVER_CLASS([net], [CONFIG_INET], [ \
  auto_irq.o net.o Space.o dev.o net_init.o pci-scan.o])

AC_DRIVER_CLASS([pcmcia], [CONFIG_PCMCIA], [ \
  pcmcia.o cs.o ds.o rsrc_mgr.o bulkmem.o cistpl.o pci_fixup.o])

AC_DRIVER_CLASS([wireless], [CONFIG_WIRELESS], [])

dnl Strictly speaking, we could have a `linux' option too, but it's
dnl not possible to built a useful kernel without at least one Linux
dnl driver, so that's not really necessary.


dnl
dnl linux_DRIVER(machname, MACRO, linuxname, class)
dnl
AC_DEFUN([linux_DRIVER], [
  AC_DRIVER([$1], [$2], [CONFIG_$3], [$4.o], [$5])
])
AC_DEFUN([linux_DRIVER_nodef], [
  AC_DRIVER_nodef([$1], [$2], [CONFIG_$3], [$4.o], [$5])
])

dnl non-SCSI Disk controllers

AC_DRIVER([floppy],
  [PC floppy],
  [CONFIG_BLK_DEV_FD], [floppy.o])
AC_DRIVER([ide],
  [IDE disks],
  [CONFIG_BLK_DEV_IDE], [cmd640.o ide-cd.o ide.o rz1000.o triton.o])


dnl SCSI Disk controllers

linux_DRIVER([advansys],
  [SCSI controller AdvanSys],
  [SCSI_ADVANSYS], [advansys], [scsi])
linux_DRIVER([BusLogic],
  [SCSI controller BusLogic],
  [SCSI_BUSLOGIC], [BusLogic], [scsi])
# TODO.  What's that?  And what about FlashPoint.o?
dnl Dirty implementation...
AC_ARG_ENABLE([flashpoint],
  AS_HELP_STRING([--enable-flashpoint], [enable SCSI flashpoint]),
  [test x"$enableval" = xno &&
   AC_DEFINE([CONFIG_SCSI_OMIT_FLASHPOINT], [], [scsi omit flashpoint])])
linux_DRIVER([u1434f],
  [SCSI controller UltraStor 14F/34F],
  [SCSI_U14_34F], [u14-34f], [scsi])
linux_DRIVER([ultrastor],
  [SCSI controller UltraStor],
  [SCSI_ULTRASTOR], [ultrastor], [scsi])
linux_DRIVER([aha152x],
  [SCSI controller Adaptec AHA-152x/2825 (aha152x, aha2825)],
  [SCSI_AHA152X], [aha152x], [scsi])
linux_DRIVER([aha1542],
  [SCSI controller Adaptec AHA-1542],
  [SCSI_AHA1542], [aha1542], [scsi])
linux_DRIVER([aha1740],
  [SCSI controller Adaptec AHA-1740],
  [SCSI_AHA1740], [aha1740], [scsi])
linux_DRIVER([aic7xxx],
  [SCSI controller Adaptec AIC7xxx],
  [SCSI_AIC7XXX], [aic7xxx], [scsi])
linux_DRIVER([fdomain],
  [SCSI controller Future Domain 16xx],
  [SCSI_FUTURE_DOMAIN], [fdomain], [scsi])
linux_DRIVER([in2000],
  [SCSI controller Always IN 2000],
  [SCSI_IN2000], [in2000], [scsi])
# Disabled by default.
linux_DRIVER_nodef([g_NCR5380],
  [SCSI controller Generic NCR5380/53c400 (ncr5380, ncr53c400)],
  [SCSI_GENERIC_NCR5380], [g_NCR5380], [scsi])
# Disabled by default.
linux_DRIVER_nodef([NCR53c406a],
  [SCSI controller NCR53c406a chip],
  [SCSI_NCR53C406A], [NCR53c406a], [scsi])
linux_DRIVER([pas16],
  [SCSI controller PAS16],
  [SCSI_PASS16], [pas16], [scsi])
linux_DRIVER([seagate],
  [SCSI controller Seagate ST02, Future Domain TMC-8xx],
  [SCSI_SEAGATE], [seagate], [scsi])
linux_DRIVER([t128],
  [SCSI controller Trantor T128/T128F/T228 (t128, t128f, t228)],
  [SCSI_T128], [t128], [scsi])
linux_DRIVER([53c78xx],
  [SCSI controller NCR 53C7,8xx],
  [SCSI_NCR53C7xx], [53c78xx], [scsi])
# Disabled by default.
linux_DRIVER_nodef([eata_dma],
  [SCSI controller EATA-DMA (DPT, NEC, AT&T, SNI, AST, Olivetti, Alphatronix)],
  [SCSI_EATA_DMA], [eata_dma], [scsi])
linux_DRIVER([eata_pio],
  [SCSI controller EATA-PIO (old DPT PM2001, PM2012A)],
  [SCSI_EATA_PIO], [eata_pio], [scsi])
linux_DRIVER([wd7000],
  [SCSI controller WD 7000],
  [SCSI_7000FASST], [wd7000], [scsi])
linux_DRIVER([eata],
  [SCSI controller EATA ISA/EISA/PCI (DPT and generic EATA/DMA-compliant boards)],
  [SCSI_EATA], [eata], [scsi])
linux_DRIVER([AM53C974],
  [SCSI controller AM53/79C974 (am53c974, am79c974)],
  [SCSI_AM53C974], [AM53C974], [scsi])
linux_DRIVER([dtc],
  [SCSI controller DTC3180/3280 (dtc3180, dtc3280)],
  [SCSI_DTC3280], [dtc], [scsi])
linux_DRIVER([ncr53c8xx],
  [SCSI controller NCR53C8XX (ncr53c8xx, dc390f, dc390u, dc390w)],
  [SCSI_NCR53C8XX], [ncr53c8xx], [scsi])
linux_DRIVER([tmscsim],
  [SCSI controller Tekram DC-390(T) (dc390, dc390t)],
  [SCSI_DC390T], [tmscsim], [scsi])
linux_DRIVER([ppa],
  [IOMEGA Parallel Port ZIP drive],
  [SCSI_PPA], [ppa], [scsi])
linux_DRIVER([qlogicfas],
  [SCSI controller Qlogic FAS],
  [SCSI_QLOGIC_FAS], [qlogicfas], [scsi])
linux_DRIVER([qlogicisp],
  [SCSI controller Qlogic ISP],
  [SCSI_QLOGIC_ISP], [qlogicisp], [scsi])
linux_DRIVER([gdth],
  [GDT SCSI Disk Array Controller],
  [SCSI_GDTH], [gdth], [scsi])


dnl Ethernet controllers

AC_DRIVER([ne],
  [Ethernet controller NE2000/NE1000 ISA (ne, ne1000, ne2000)],
  [CONFIG_NE2000], [ne.o 8390.o], [net])
AC_DRIVER([3c503],
  [Ethernet controller 3Com 503 (3c503) / Etherlink II],
  [CONFIG_EL2], [3c503.o 8390.o], [net])
linux_DRIVER([3c509],
  [Ethernet controller 3Com 509/579 (3c509, 3c579) / Etherlink III],
  [EL3], [3c509], [net])
AC_DRIVER([wd],
  [Ethernet controller WD80x3],
  [CONFIG_WD80x3], [wd.o 8390.o], [net])
linux_DRIVER([3c501],
  [Ethernet controller 3COM 501 (3c501) / Etherlink I],
  [EL1], [3c501], [net])
AC_DRIVER([smcultra],
  [Ethernet controller SMC Ultra],
  [CONFIG_ULTRA], [smc-ultra.o 8390.o], [net])
AC_DRIVER([smcultra32],
  [Ethernet controller SMC Ultra32],
  [CONFIG_ULTRA32], [smc-ultra32.o 8390.o], [net])
AC_DRIVER([hpplus],
  [Ethernet controller HP PCLAN+ (27247B and 27252A)],
  [CONFIG_HPLAN_PLUS], [hp-plus.o 8390.o], [net])
AC_DRIVER([hp],
  [Ethernet controller HP PCLAN (27245 and other 27xxx series)],
  [CONFIG_HPLAN], [hp.o 8390.o], [net])
linux_DRIVER([3c59x],
  [Ethernet controller 3Com 59x/90x (3c59x, 3c590, 3c592, 3c595, 3c597, 3c90x, 3c900, 3c905) "Vortex/Boomerang"],
  [VORTEX], [3c59x], [net])
linux_DRIVER([seeq8005],
  [Ethernet controller Seeq8005],
  [SEEQ8005], [seeq8005], [net])
linux_DRIVER([hp100],
  [Ethernet controller HP 10/100VG PCLAN (ISA, EISA, PCI) (hp100, hpj2577, hpj2573, hpj2585, hp27248b)],
  [HP100], [hp100], [net])
AC_DRIVER([ac3200],
  [Ethernet controller Ansel Communications EISA 3200],
  [CONFIG_AC3200], [ac3200.o 8390.o], [net])
AC_DRIVER([e2100],
  [Ethernet controller Cabletron E21xx],
  [CONFIG_E2100], [e2100.o 8390.o], [net])
linux_DRIVER([at1700],
  [Ethernet controller AT1700 (Fujitsu 86965)],
  [AT1700], [at1700], [net])
linux_DRIVER([eth16i],
  [Ethernet controller ICL EtherTeam 16i/32 (eth16i, eth32)],
  [ETH16I], [eth16i], [net])
linux_DRIVER([znet],
  [Ethernet controller Zenith Z-Note (znet, znote)],
  [ZNET], [znet], [net])
linux_DRIVER([eexpress],
  [Ethernet controller EtherExpress 16],
  [EEXPRESS], [eexpress], [net])
linux_DRIVER([eepro],
  [Ethernet controller EtherExpressPro],
  [EEXPRESS_PRO], [eepro], [net])
linux_DRIVER([eepro100],
  [Ethernet controller Intel EtherExpressPro PCI 10+/100B/100+],
  [EEXPRESS_PRO100B], [eepro100], [net])
linux_DRIVER([depca],
  [Ethernet controller DEPCA (de100, de101, de200, de201, de202, de210, de422)],
  [DEPCA], [depca], [net])
linux_DRIVER([ewrk3],
  [Ethernet controller EtherWORKS 3 (ewrk3, de203, de204, de205)],
  [EWRK3], [ewrk3], [net])
linux_DRIVER([de4x5],
  [Ethernet controller DE4x5 (de4x5, de425, de434, de435, de450, de500)],
  [DE4X5], [de4x5], [net])
linux_DRIVER([apricot],
  [Ethernet controller Apricot XEN-II on board ethernet],
  [APRICOT], [apricot], [net])
# Disabled by default.
linux_DRIVER_nodef([wavelan],
  [Ethernet controller AT&T WaveLAN & DEC RoamAbout DS],
  [WAVELAN], [wavelan], [net])
linux_DRIVER([3c507],
  [Ethernet controller 3Com 507 (3c507, el16)],
  [EL16], [3c507], [net])
linux_DRIVER([3c505],
  [Ethernet controller 3Com 505 (3c505, elplus)],
  [ELPLUS], [3c505], [net])
linux_DRIVER([de600],
  [Ethernet controller D-Link DE-600],
  [DE600], [de600], [net])
linux_DRIVER([de620],
  [Ethernet controller D-Link DE-620],
  [DE620], [de620], [net])
linux_DRIVER([sk_g16],
  [Ethernet controller Schneider & Koch G16],
  [SK_G16], [sk_g16], [net])
linux_DRIVER([ni52],
  [Ethernet controller NI5210],
  [NI52], [ni52], [net])
linux_DRIVER([ni65],
  [Ethernet controller NI6510],
  [NI65], [ni65], [net])
# Disabled by default.
linux_DRIVER_nodef([atp],
  [Ethernet controller AT-LAN-TEC/RealTek pocket adaptor],
  [ATP], [atp], [net])
linux_DRIVER([lance],
  [Ethernet controller AMD LANCE and PCnet (at1500, ne2100)],
  [LANCE], [lance], [net])
linux_DRIVER([tulip],
  [Ethernet controller DECchip Tulip (dc21x4x) PCI (elcp, tulip)],
  [DEC_ELCP], [tulip], [net])
linux_DRIVER([fmv18x],
  [Ethernet controller FMV-181/182/183/184],
  [FMV18X], [fmv18x], [net])
linux_DRIVER([3c515],
  [Ethernet controller 3Com 515 ISA Fast EtherLink],
  [3C515], [3c515], [net])
linux_DRIVER([pcnet32],
  [Ethernet controller AMD PCI PCnet32 (PCI bus NE2100 cards)],
  [PCNET32], [pcnet32], [net])
AC_DRIVER([ne2kpci],
  [Ethernet controller PCI NE2000],
  [CONFIG_NE2K_PCI], [ne2k-pci.o 8390.o], [net])
linux_DRIVER([yellowfin],
  [Ethernet controller Packet Engines Yellowfin Gigabit-NIC],
  [YELLOWFIN], [yellowfin], [net])
linux_DRIVER([rtl8139],
  [Ethernet controller RealTek 8129/8139 (rtl8129, rtl8139) (not 8019/8029!)],
  [RTL8139], [rtl8139], [net])
linux_DRIVER([epic100],
  [Ethernet controller SMC 83c170/175 EPIC/100 (epic, epic100) / EtherPower II],
  [EPIC], [epic100], [net])
linux_DRIVER([tlan],
  [Ethernet controller TI ThunderLAN],
  [TLAN], [tlan], [net])
linux_DRIVER([viarhine],
  [Ethernet controller VIA Rhine],
  [VIA_RHINE], [via-rhine], [net])
linux_DRIVER([hamachi],
  [Ethernet controller Packet Engines "Hamachi" GNIC-2 Gigabit Ethernet],
  [HAMACHI], [hamachi], [net])
linux_DRIVER([intelgige],
  [Ethernet controller Intel PCI Gigabit Ethernet],
  [INTEL_GIGE], [intel-gige], [net])
linux_DRIVER([myson803],
  [Ethernet controller Myson MTD803 Ethernet adapter series],
  [MYSON803], [myson803], [net])
linux_DRIVER([natsemi],
  [Ethernet controller National Semiconductor DP8381x series PCI Ethernet],
  [NATSEMI], [natsemi], [net])
linux_DRIVER([ns820],
  [Ethernet controller National Semiconductor DP8382x series PCI Ethernet],
  [NS820], [ns820], [net])
linux_DRIVER([starfire],
  [Ethernet controller Adaptec Starfire network adapter],
  [STARFIRE], [starfire], [net])
linux_DRIVER([sundance],
  [Ethernet controller Sundance ST201 "Alta" PCI Ethernet],
  [SUNDANCE], [sundance], [net])
linux_DRIVER([winbond840],
  [Ethernet controller Winbond W89c840 PCI Ethernet],
  [WINBOND840], [winbond-840], [net])
dnl FIXME: Can't be enabled since it is a pcmcia driver, and we don't
dnl have that kind of fluff.
dnl
dnl linux_DRIVER([cb_shim], [CB_SHIM], [cb_shim], [net])



dnl PCMCIA device support.

linux_DRIVER([i82365],
  [Intel 82365 PC Card controller],
  [I82365], [i82365], [pcmcia])

AC_DEFUN([AC_PCMCIA_OPTION], [
  AC_ARG_ENABLE([pcmcia-$1], 
    AS_HELP_STRING([--disable-pcmcia-$1], [$4]))
  [enable_pcmcia_$1=${enable_pcmcia_$1-$enable_default_device_drivers}
  if test x"$enable_pcmcia_$1" = xyes; then]
    AC_DEFINE([CONFIG_$2], [1], [$4])
    [device_drivers="$device_drivers $3"
    if test "${driver_class_pcmcia_selected+set}" != set; then
      driver_class_pcmcia_selected=yes]
      AC_DEFINE_UNQUOTED([$driver_class_pcmcia_option], [1])
      [device_drivers="$device_drivers $driver_class_pcmcia_files"
    fi
  fi
]])

AC_PCMCIA_OPTION([isa], [ISA], [],
		 [disable isa-bus-support (pcmcia core)])


dnl PCMCIA device drivers.

linux_DRIVER([3c574_cs],
  [3Com 3c574 ``RoadRunner'' PCMCIA Ethernet],
  [3C574_CS], [3c574_cs], [pcmcia])
linux_DRIVER([3c589_cs],
  [3Com 3c589 PCMCIA Ethernet card],
  [3C589_CS], [3c589_cs], [pcmcia])
linux_DRIVER([axnet_cs],
  [Asix AX88190-based PCMCIA Ethernet adapters],
  [AXNET_CS], [axnet_cs], [pcmcia])
linux_DRIVER([fmvj18x_cs],
  [fmvj18x chipset based PCMCIA Ethernet cards],
  [FMVJ18X_CS], [fmvj18x_cs], [pcmcia])
linux_DRIVER([nmclan_cs],
  [New Media Ethernet LAN PCMCIA cards],
  [NMCLAN_CS], [nmclan_cs], [pcmcia])
AC_DRIVER([pcnet_cs],
  [NS8390-based PCMCIA cards],
  [CONFIG_PCNET_CS], [pcnet_cs.o 8390.o], [pcmcia])
linux_DRIVER([smc91c92_cs],
  [SMC91c92-based PCMCIA cards],
  [SMC91C92_CS], [smc91c92_cs], [pcmcia])
linux_DRIVER([xirc2ps_cs],
  [Xircom CreditCard and Realport PCMCIA ethernet],
  [XIRC2PS_CS], [xirc2ps_cs], [pcmcia])

dnl Wireless device drivers.

AC_DRIVER([orinoco_cs],
  [Hermes or Prism 2 PCMCIA Wireless adapters (Orinoco)],
  [CONFIG_ORINOCO_CS], [hermes.o orinoco.o orinoco_cs.o], [wireless])


dnl If any pcmcia wireless devices have been enabled, make sure to include the
dnl pcmcia glue as well.
if test x"${driver_class_wireless_selected+set}" = xset; then
  if test x"${driver_class_pcmcia_selected+set}" != xset; then
    driver_class_pcmcia_selected=yes
    AC_DEFINE_UNQUOTED([$driver_class_pcmcia_option], [1])
    device_drivers="$device_drivers $driver_class_pcmcia_files"
  fi
fi

dnl If any pcmcia devices have been enabled, make sure to include the network
dnl glue as well.
if test x"${driver_class_pcmcia_selected+set}" = xset; then
  if test x"${driver_class_net_selected+set}" != xset; then
    driver_class_net_selected=yes
    AC_DEFINE_UNQUOTED([$driver_class_net_option], [1])
    device_drivers="$device_drivers $driver_class_net_files"
  fi
fi


AC_CONFIG_FILES([Makefile Makerules])
AC_OUTPUT
