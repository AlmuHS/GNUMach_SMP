#ifndef _LINUX_GENHD_H
#define _LINUX_GENHD_H

/*
 * 	genhd.h Copyright (C) 1992 Drew Eckhardt
 *	Generic hard disk header file by  
 * 		Drew Eckhardt
 *
 *		<drew@colorado.edu>
 */

#include <linux/config.h>

#define CONFIG_MSDOS_PARTITION 1

#ifdef __alpha__
#define CONFIG_OSF_PARTITION 1
#endif

#if defined(__sparc__) || defined(CONFIG_SMD_DISKLABEL)
#define CONFIG_SUN_PARTITION 1
#endif

/* These three have identical behaviour; use the second one if DOS fdisk gets
   confused about extended/logical partitions starting past cylinder 1023. */
#define DOS_EXTENDED_PARTITION 5
#define LINUX_EXTENDED_PARTITION 0x85
#define WIN98_EXTENDED_PARTITION 0x0f

#define DM6_PARTITION		0x54	/* has DDO: use xlated geom & offset */
#define EZD_PARTITION		0x55	/* EZ-DRIVE:  same as DM6 (we think) */
#define DM6_AUX1PARTITION	0x51	/* no DDO:  use xlated geom */
#define DM6_AUX3PARTITION	0x53	/* no DDO:  use xlated geom */

#ifdef MACH_INCLUDE
struct linux_partition
{
#else
struct partition {
#endif
	unsigned char boot_ind;		/* 0x80 - active */
	unsigned char head;		/* starting head */
	unsigned char sector;		/* starting sector */
	unsigned char cyl;		/* starting cylinder */
	unsigned char sys_ind;		/* What partition type */
	unsigned char end_head;		/* end head */
	unsigned char end_sector;	/* end sector */
	unsigned char end_cyl;		/* end cylinder */
	unsigned int start_sect;	/* starting sector counting from 0 */
	unsigned int nr_sects;		/* nr of sectors in partition */
} __attribute((packed));	/* Give a polite hint to egcs/alpha to generate
				   unaligned operations */

struct hd_struct {
	long start_sect;
	long nr_sects;
};

struct gendisk {
	int major;			/* major number of driver */
	const char *major_name;		/* name of major driver */
	int minor_shift;		/* number of times minor is shifted to
					   get real minor */
	int max_p;			/* maximum partitions per device */
	int max_nr;			/* maximum number of real devices */

	void (*init)(struct gendisk *);	/* Initialization called before we do our thing */
	struct hd_struct *part;		/* partition table */
	int *sizes;			/* device size in blocks, copied to blk_size[] */
	int nr_real;			/* number of real devices */

	void *real_devices;		/* internal use */
	struct gendisk *next;
};

#ifdef CONFIG_BSD_DISKLABEL
/*
 * BSD disklabel support by Yossi Gottlieb <yogo@math.tau.ac.il>
 */

#define BSD_PARTITION		0xa5	/* Partition ID */

#define BSD_DISKMAGIC	(0x82564557UL)	/* The disk magic number */
#define BSD_MAXPARTITIONS	8
#define BSD_FS_UNUSED		0	/* disklabel unused partition entry ID */
struct bsd_disklabel {
	__u32	d_magic;		/* the magic number */
	__s16	d_type;			/* drive type */
	__s16	d_subtype;		/* controller/d_type specific */
	char	d_typename[16];		/* type name, e.g. "eagle" */
	char	d_packname[16];			/* pack identifier */ 
	__u32	d_secsize;		/* # of bytes per sector */
	__u32	d_nsectors;		/* # of data sectors per track */
	__u32	d_ntracks;		/* # of tracks per cylinder */
	__u32	d_ncylinders;		/* # of data cylinders per unit */
	__u32	d_secpercyl;		/* # of data sectors per cylinder */
	__u32	d_secperunit;		/* # of data sectors per unit */
	__u16	d_sparespertrack;	/* # of spare sectors per track */
	__u16	d_sparespercyl;		/* # of spare sectors per cylinder */
	__u32	d_acylinders;		/* # of alt. cylinders per unit */
	__u16	d_rpm;			/* rotational speed */
	__u16	d_interleave;		/* hardware sector interleave */
	__u16	d_trackskew;		/* sector 0 skew, per track */
	__u16	d_cylskew;		/* sector 0 skew, per cylinder */
	__u32	d_headswitch;		/* head switch time, usec */
	__u32	d_trkseek;		/* track-to-track seek, usec */
	__u32	d_flags;		/* generic flags */
#define NDDATA 5
	__u32	d_drivedata[NDDATA];	/* drive-type specific information */
#define NSPARE 5
	__u32	d_spare[NSPARE];	/* reserved for future use */
	__u32	d_magic2;		/* the magic number (again) */
	__u16	d_checksum;		/* xor of data incl. partitions */

			/* filesystem and partition information: */
	__u16	d_npartitions;		/* number of partitions in following */
	__u32	d_bbsize;		/* size of boot area at sn0, bytes */
	__u32	d_sbsize;		/* max size of fs superblock, bytes */
	struct	bsd_partition {		/* the partition table */
		__u32	p_size;		/* number of sectors in partition */
		__u32	p_offset;	/* starting sector */
		__u32	p_fsize;	/* filesystem basic fragment size */
		__u8	p_fstype;	/* filesystem type, see below */
		__u8	p_frag;		/* filesystem fragments per block */
		__u16	p_cpg;		/* filesystem cylinders per group */
	} d_partitions[BSD_MAXPARTITIONS];	/* actually may be more */
};

#endif	/* CONFIG_BSD_DISKLABEL */

#ifdef CONFIG_GPT_DISKLABEL
/*
 * GPT disklabel support by наб <nabijaczleweli@gmail.com>
 *
 * Based on UEFI specification 2.8A (current as of May 2020):
 * https://uefi.org/specifications
 * https://uefi.org/sites/default/files/resources/UEFI_Spec_2_8_A_Feb14.pdf
 *
 * CRC32 behaviour (final ^ ~0) courtesy of util-linux documentation:
 * https://git.kernel.org/pub/scm/utils/util-linux/util-linux.git/tree/libblkid/src/partitions/gpt.c?id=042f62dfc514da177c148c257e4dcb32e5f8379d#n104
 */

#define GPT_PARTITION		0xee	/* Partition ID in MBR */

#define GPT_GUID_SIZE	16
struct gpt_guid {
	__u32	g_time_low;		/* Low field of timestamp */
	__u16	g_time_mid;		/* Medium field of timestamp */
	__u16	g_time_high_version;		/* High field of timestamp and version */
	__u8	g_clock_sec_high;		/* High field of clock sequence and variant */
	__u8	g_clock_sec_low;		/* Low field of clock sequence */
	__u8	g_node_id[6];		/* Spatially unique node identifier (MAC address or urandom) */
} __attribute((packed));
typedef char __gpt_guid_right_size[(sizeof(struct gpt_guid) == GPT_GUID_SIZE) ? 1 : -1];

static const struct gpt_guid GPT_GUID_TYPE_UNUSED = {0,0,0,0,0,{0,0,0,0,0,0}};

#define GPT_SIGNATURE	"EFI PART"		/* The header signauture */
#define GPT_REVISION	(0x00010000UL)	/* Little-endian on disk */
#define GPT_HEADER_SIZE	92
#define GPT_MAXPARTITIONS	128
struct gpt_disklabel_header {
	char	h_signature[8];		/* Must match GPT_SIGNATURE */
	__u32	h_revision;			/* Disklabel revision, must match GPT_REVISION */
	__u32	h_header_size;		/* Must match GPT_HEADER_SIZE */
	__u32	h_header_crc;		/* CRC32 of header, zero for calculation */
	__u32	h_reserved;		/* Must be zero */
	__u64	h_lba_current;		/* LBA of this copy of the header */
	__u64	h_lba_backup;		/* LBA of the second (backup) copy of the header */
	__u64	h_lba_usable_first;		/* First usable LBA for partitions (last LBA of primary table + 1) */
	__u64	h_lba_usable_last;		/* Last usable LBA for partitions (first LBA of secondary table - 1) */
	struct gpt_guid	h_guid;		/* ID of the disk */
	__u64	h_part_table_lba;		/* First LBA of the partition table (usually 2 in primary header) */
	__u32	h_part_table_len;		/* Amount of entries in the partition table */
	__u32	h_part_table_entry_size;		/* Size of each partition entry (usually 128) */
	__u32	h_part_table_crc;		/* CRC32 of entire partition table, starts at h_part_table_lba, is h_part_table_len*h_part_table_entry_size long */
						/* Rest of block must be zero */
} __attribute((packed));
typedef char __gpt_header_right_size[(sizeof(struct gpt_disklabel_header) == GPT_HEADER_SIZE) ? 1 : -1];

/* 3-47: reserved; 48-63: defined for individual partition types. */
#define GPT_PARTITION_ATTR_PLATFORM_REQUIRED	(1ULL << 0)		/* Required by the platform to function */
#define GPT_PARTITION_ATTR_EFI_IGNORE	(1ULL << 1)		/* To be ignored by the EFI firmware */
#define GPT_PARTITION_ATTR_BIOS_BOOTABLE	(1ULL << 2)		/* Equivalent to MBR active flag */

#define GPT_PARTITION_ENTRY_SIZE	128		/* Minimum size, implementations must respect bigger vendor-specific entries */
struct gpt_disklabel_part {
	struct gpt_guid	p_type;		/* Partition type GUID */
	struct gpt_guid	p_guid;		/* ID of the partition */
	__u64	p_lba_first;		/* First LBA of the partition */
	__u64	p_lba_last;		/* Last LBA of the partition */
	__u64	p_attrs;		/* Partition attribute bitfield, see above */
	__u16	p_name[36];		/* Display name of partition, UTF-16 */
} __attribute((packed));
typedef char __gpt_part_entry_right_size[(sizeof(struct gpt_disklabel_part) == GPT_PARTITION_ENTRY_SIZE) ? 1 : -1];
#endif	/* CONFIG_GPT_DISKLABEL */

extern struct gendisk *gendisk_head;	/* linked list of disks */

/*
 * disk_name() is used by genhd.c and md.c.
 * It formats the devicename of the indicated disk
 * into the supplied buffer, and returns a pointer
 * to that same buffer (for convenience).
 */
char *disk_name (struct gendisk *hd, int minor, char *buf);

#endif
