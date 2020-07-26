/*
 * Automatically generated C config: don't edit
 */
#define AUTOCONF_INCLUDED

/*
 * Code maturity level options
 */
#define CONFIG_EXPERIMENTAL 1

/*
 * Loadable module support
 */
#undef  CONFIG_MODULES

/*
 * General setup
 */
#undef  CONFIG_MATH_EMULATION
#define CONFIG_NET 1
#undef  CONFIG_MAX_16M
#define CONFIG_PCI 1
#define CONFIG_PCI_OPTIMIZE 1
#define CONFIG_SYSVIPC 1
#undef  CONFIG_BINFMT_AOUT
#define CONFIG_BINFMT_ELF 1
#undef  CONFIG_BINFMT_JAVA
#define CONFIG_KERNEL_ELF 1

#if 0
#undef  CONFIG_M386
#define CONFIG_M486 1
#undef  CONFIG_M586
#undef  CONFIG_M686
#endif

#if NCPUS > 1
#define CONFIG_SMP 1
#endif

/*
 * Floppy, IDE, and other block devices
 */
#if 0
#define CONFIG_BLK_DEV_FD 1
#define CONFIG_BLK_DEV_IDE 1
#endif

/*
 * Please see Documentation/ide.txt for help/info on IDE drives
 */
#undef  CONFIG_BLK_DEV_HD_IDE
#define CONFIG_BLK_DEV_IDECD 1
#undef  CONFIG_BLK_DEV_IDETAPE
#undef  CONFIG_BLK_DEV_IDEFLOPPY
#undef  CONFIG_BLK_DEV_IDESCSI
#undef  CONFIG_BLK_DEV_IDE_PCMCIA
#undef  CONFIG_BLK_DEV_CMD640
#undef  CONFIG_BLK_DEV_CMD640_ENHANCED
#define CONFIG_BLK_DEV_RZ1000 1
#define CONFIG_BLK_DEV_TRITON 1
#undef  CONFIG_IDE_CHIPSETS

/*
 * Additional Block Devices
 */
#undef  CONFIG_BLK_DEV_LOOP
#undef  CONFIG_BLK_DEV_MD
#undef  CONFIG_BLK_DEV_RAM
#undef  CONFIG_BLK_DEV_XD
#undef  CONFIG_BLK_DEV_HD

/*
 * Networking options
 */
#if 0
#undef  CONFIG_FIREWALL
#undef  CONFIG_NET_ALIAS
#define CONFIG_INET 1
#undef  CONFIG_IP_FORWARD
#undef  CONFIG_IP_MULTICAST
#undef  CONFIG_SYN_COOKIES
#undef  CONFIG_RST_COOKIES
#undef  CONFIG_IP_ACCT
#undef  CONFIG_IP_ROUTER
#undef  CONFIG_NET_IPIP
#endif

/*
 * (it is safe to leave these untouched)
 */
#undef  CONFIG_INET_PCTCP
#undef  CONFIG_INET_RARP
#undef  CONFIG_NO_PATH_MTU_DISCOVERY
#undef  CONFIG_IP_NOSR
#undef  CONFIG_SKB_LARGE

/*
 *
 */
#undef  CONFIG_IPX
#undef  CONFIG_ATALK
#undef  CONFIG_AX25
#undef  CONFIG_BRIDGE
#undef  CONFIG_NETLINK

/*
 * SCSI support
 */
#if 0
#define CONFIG_SCSI 1
#endif

/*
 * SCSI support type (disk, tape, CD-ROM)
 */
#define CONFIG_BLK_DEV_SD 1
#undef  CONFIG_CHR_DEV_ST
#define CONFIG_BLK_DEV_SR 1
#undef  CONFIG_CHR_DEV_SG

/*
 * Some SCSI devices (e.g. CD jukebox) support multiple LUNs
 */
#if 0
#undef  CONFIG_SCSI_MULTI_LUN
#undef  CONFIG_SCSI_CONSTANTS

/*
 * SCSI low-level drivers
 */
#undef  CONFIG_SCSI_7000FASST
#undef  CONFIG_SCSI_AHA152X
#undef  CONFIG_SCSI_AHA1542
#undef  CONFIG_SCSI_AHA1740
#undef  CONFIG_SCSI_AIC7XXX
#undef  CONFIG_SCSI_ADVANSYS
#undef  CONFIG_SCSI_IN2000
#undef  CONFIG_SCSI_AM53C974
#undef  CONFIG_SCSI_BUSLOGIC
#undef  CONFIG_SCSI_DTC3280
#undef  CONFIG_SCSI_EATA_DMA
#undef  CONFIG_SCSI_EATA_PIO
#undef  CONFIG_SCSI_EATA
#undef  CONFIG_SCSI_FUTURE_DOMAIN
#undef  CONFIG_SCSI_GENERIC_NCR5380
#undef  CONFIG_SCSI_NCR53C406A
#undef  CONFIG_SCSI_NCR53C7xx
#undef  CONFIG_SCSI_NCR53C8XX
#undef  CONFIG_SCSI_DC390W
#undef  CONFIG_SCSI_PPA
#undef  CONFIG_SCSI_PAS16
#undef  CONFIG_SCSI_QLOGIC_FAS
#undef  CONFIG_SCSI_QLOGIC_ISP
#undef  CONFIG_SCSI_SEAGATE
#undef  CONFIG_SCSI_DC390T
#undef  CONFIG_SCSI_T128
#undef  CONFIG_SCSI_U14_34F
#undef  CONFIG_SCSI_ULTRASTOR
#undef  CONFIG_SCSI_GDTH
#endif

/*
 * Network device support
 */
#define CONFIG_NETDEVICES 1
#undef  CONFIG_DUMMY
#undef  CONFIG_EQUALIZER
#undef  CONFIG_DLCI
#undef  CONFIG_PLIP
#undef  CONFIG_PPP
#undef  CONFIG_SLIP
#undef  CONFIG_NET_RADIO

#if 0
#define CONFIG_NET_ETHERNET 1
#define CONFIG_NET_VENDOR_3COM 1
#undef  CONFIG_EL1
#undef  CONFIG_EL2
#undef  CONFIG_ELPLUS
#undef  CONFIG_EL16
#undef  CONFIG_EL3
#undef  CONFIG_VORTEX
#undef  CONFIG_LANCE
#undef  CONFIG_NET_VENDOR_SMC
#define CONFIG_NET_ISA 1
#undef  CONFIG_AT1700
#undef  CONFIG_E2100
#undef  CONFIG_DEPCA
#undef  CONFIG_EWRK3
#undef  CONFIG_EEXPRESS
#undef  CONFIG_EEXPRESS_PRO
#undef  CONFIG_FMV18X
#undef  CONFIG_HPLAN_PLUS
#undef  CONFIG_HPLAN
#undef  CONFIG_HP100
#undef  CONFIG_ETH16I
#undef  CONFIG_NE2000
#undef  CONFIG_NI52
#undef  CONFIG_NI65
#undef  CONFIG_SEEQ8005
#undef  CONFIG_SK_G16
#undef  CONFIG_NET_EISA
#undef  CONFIG_NET_POCKET
#undef  CONFIG_TR
#undef  CONFIG_FDDI
#undef  CONFIG_ARCNET
#endif

/*
 * ISDN subsystem
 */
#undef  CONFIG_ISDN

/*
 * CD-ROM drivers (not for SCSI or IDE/ATAPI drives)
 */
#undef  CONFIG_CD_NO_IDESCSI

/*
 * Filesystems
 */
#undef  CONFIG_QUOTA
#define CONFIG_MINIX_FS 1
#undef  CONFIG_EXT_FS
#define CONFIG_EXT2_FS 1
#undef  CONFIG_XIA_FS
#define CONFIG_FAT_FS 1
#define CONFIG_MSDOS_FS 1
#define CONFIG_VFAT_FS 1
#define CONFIG_UMSDOS_FS 1
#define CONFIG_PROC_FS 1
#define CONFIG_NFS_FS 1
#undef  CONFIG_ROOT_NFS
#undef  CONFIG_SMB_FS
#define CONFIG_ISO9660_FS 1
#define CONFIG_HPFS_FS 1
#define CONFIG_SYSV_FS 1
#undef  CONFIG_AUTOFS_FS
#define CONFIG_AFFS_FS 1
#undef  CONFIG_AMIGA_PARTITION
#define CONFIG_UFS_FS 1

/* We want Linux's partitioning code to do only the DOS partition table,
   since the Mach glue code does BSD disklabels for us.  */
#undef	CONFIG_BSD_DISKLABEL
#undef	CONFIG_SMD_DISKLABEL

/*
 * Character devices
 */
#if 0
#define CONFIG_SERIAL 1
#undef  CONFIG_DIGI
#undef  CONFIG_CYCLADES
#undef  CONFIG_STALDRV
#undef  CONFIG_RISCOM8
#define CONFIG_PRINTER 1
#undef  CONFIG_SPECIALIX
#define CONFIG_MOUSE 1
#undef  CONFIG_ATIXL_BUSMOUSE
#undef  CONFIG_BUSMOUSE
#undef  CONFIG_MS_BUSMOUSE
#define CONFIG_PSMOUSE 1
#undef  CONFIG_82C710_MOUSE
#undef  CONFIG_UMISC
#undef  CONFIG_QIC02_TAPE
#undef  CONFIG_FTAPE
#undef  CONFIG_APM
#undef  CONFIG_WATCHDOG
#undef  CONFIG_RTC
#endif

/*
 * Sound
 */
#undef  CONFIG_SOUND

/*
 * Kernel hacking
 */
#undef  CONFIG_PROFILE
