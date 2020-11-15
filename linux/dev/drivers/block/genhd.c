/*
 *  Code extracted from
 *  linux/kernel/hd.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *
 *  Thanks to Branko Lankester, lankeste@fwi.uva.nl, who found a bug
 *  in the early extended-partition checks and added DM partitions
 *
 *  Support for DiskManager v6.0x added by Mark Lord,
 *  with information provided by OnTrack.  This now works for linux fdisk
 *  and LILO, as well as loadlin and bootln.  Note that disks other than
 *  /dev/hda *must* have a "DOS" type 0x51 partition in the first slot (hda1).
 *
 *  More flexible handling of extended partitions - aeb, 950831
 *
 *  Check partition table on IDE disks for common CHS translations
 */

#include <linux/config.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/kernel.h>
#include <linux/major.h>
#include <linux/string.h>
#ifdef CONFIG_BLK_DEV_INITRD
#include <linux/blk.h>
#endif
#include <linux/hdreg.h>
#include <alloca.h>
#ifdef CONFIG_GPT_DISKLABEL
#include <linux/blkdev.h>
#include <kern/kalloc.h>
#include <stddef.h>
#endif

#include <asm/system.h>

/*
 * Many architectures don't like unaligned accesses, which is
 * frequently the case with the nr_sects and start_sect partition
 * table entries.
 */
#include <asm/unaligned.h>

#ifdef MACH
#include <machine/spl.h>
#include <linux/dev/glue/glue.h>
#endif

#define SYS_IND(p)	get_unaligned(&p->sys_ind)
#define NR_SECTS(p)	get_unaligned(&p->nr_sects)
#define START_SECT(p)	get_unaligned(&p->start_sect)


struct gendisk *gendisk_head = NULL;

static int current_minor = 0;
extern int *blk_size[];
extern void rd_load(void);
extern void initrd_load(void);

extern int chr_dev_init(void);
extern int blk_dev_init(void);
extern int scsi_dev_init(void);
extern int net_dev_init(void);

/*
 * disk_name() is used by genhd.c and md.c.
 * It formats the devicename of the indicated disk
 * into the supplied buffer, and returns a pointer
 * to that same buffer (for convenience).
 */
char *disk_name (struct gendisk *hd, int minor, char *buf)
{
	unsigned int part;
	const char *maj = hd->major_name;
#ifdef MACH
	char unit = (minor >> hd->minor_shift) + '0';
#else
	char unit = (minor >> hd->minor_shift) + 'a';
#endif

#ifdef CONFIG_BLK_DEV_IDE
	/*
	 * IDE devices use multiple major numbers, but the drives
	 * are named as:  {hda,hdb}, {hdc,hdd}, {hde,hdf}, {hdg,hdh}..
	 * This requires special handling here.
	 */
	switch (hd->major) {
		case IDE3_MAJOR:
			unit += 2;
		case IDE2_MAJOR:
			unit += 2;
		case IDE1_MAJOR:
			unit += 2;
		case IDE0_MAJOR:
			maj = "hd";
	}
#endif
	part = minor & ((1 << hd->minor_shift) - 1);
	if (part)
#ifdef MACH
		sprintf(buf, "%s%cs%d", maj, unit, part);
#else
		sprintf(buf, "%s%c%d", maj, unit, part);
#endif
	else
		sprintf(buf, "%s%c", maj, unit);
	return buf;
}

static void add_partition (struct gendisk *hd, int minor, int start, int size)
{
	char buf[8];
	hd->part[minor].start_sect = start;
	hd->part[minor].nr_sects   = size;
	printk(" %s", disk_name(hd, minor, buf));
}

#if defined (MACH) && defined (CONFIG_BSD_DISKLABEL)
static int mach_minor;
static void
add_bsd_partition (struct gendisk *hd, int minor, int slice,
		   int start, int size)
{
  char buf[16];
  hd->part[minor].start_sect = start;
  hd->part[minor].nr_sects = size;
  printk (" %s%c", disk_name (hd, mach_minor, buf), slice);
}
#endif

static inline int is_extended_partition(struct partition *p)
{
	return (SYS_IND(p) == DOS_EXTENDED_PARTITION ||
		SYS_IND(p) == WIN98_EXTENDED_PARTITION ||
		SYS_IND(p) == LINUX_EXTENDED_PARTITION);
}

#ifdef CONFIG_MSDOS_PARTITION
/*
 * Create devices for each logical partition in an extended partition.
 * The logical partitions form a linked list, with each entry being
 * a partition table with two entries.  The first entry
 * is the real data partition (with a start relative to the partition
 * table start).  The second is a pointer to the next logical partition
 * (with a start relative to the entire extended partition).
 * We do not create a Linux partition for the partition tables, but
 * only for the actual data partitions.
 */

static void extended_partition(struct gendisk *hd, kdev_t dev)
{
	struct buffer_head *bh;
	struct partition *p;
	unsigned long first_sector, first_size, this_sector, this_size;
	int mask = (1 << hd->minor_shift) - 1;
	int i;

	first_sector = hd->part[MINOR(dev)].start_sect;
	first_size = hd->part[MINOR(dev)].nr_sects;
	this_sector = first_sector;

	while (1) {
		if ((current_minor & mask) == 0)
			return;
		if (!(bh = bread(dev,0,1024)))
			return;
	  /*
	   * This block is from a device that we're about to stomp on.
	   * So make sure nobody thinks this block is usable.
	   */
		bh->b_state = 0;

		if (*(unsigned short *) (bh->b_data+510) != 0xAA55)
			goto done;

		p = (struct partition *) (0x1BE + bh->b_data);

		this_size = hd->part[MINOR(dev)].nr_sects;

		/*
		 * Usually, the first entry is the real data partition,
		 * the 2nd entry is the next extended partition, or empty,
		 * and the 3rd and 4th entries are unused.
		 * However, DRDOS sometimes has the extended partition as
		 * the first entry (when the data partition is empty),
		 * and OS/2 seems to use all four entries.
		 */

		/*
		 * First process the data partition(s)
		 */
		for (i=0; i<4; i++, p++) {
		    if (!NR_SECTS(p) || is_extended_partition(p))
		      continue;

		    /* Check the 3rd and 4th entries -
		       these sometimes contain random garbage */
		    if (i >= 2
			&& START_SECT(p) + NR_SECTS(p) > this_size
			&& (this_sector + START_SECT(p) < first_sector ||
			    this_sector + START_SECT(p) + NR_SECTS(p) >
			     first_sector + first_size))
		      continue;

		    add_partition(hd, current_minor, this_sector+START_SECT(p), NR_SECTS(p));
		    current_minor++;
		    if ((current_minor & mask) == 0)
		      goto done;
		}
		/*
		 * Next, process the (first) extended partition, if present.
		 * (So far, there seems to be no reason to make
		 *  extended_partition()  recursive and allow a tree
		 *  of extended partitions.)
		 * It should be a link to the next logical partition.
		 * Create a minor for this just long enough to get the next
		 * partition table.  The minor will be reused for the next
		 * data partition.
		 */
		p -= 4;
		for (i=0; i<4; i++, p++)
		  if(NR_SECTS(p) && is_extended_partition(p))
		    break;
		if (i == 4)
		  goto done;	 /* nothing left to do */

		hd->part[current_minor].nr_sects = NR_SECTS(p);
		hd->part[current_minor].start_sect = first_sector + START_SECT(p);
		this_sector = first_sector + START_SECT(p);
		dev = MKDEV(hd->major, current_minor);
		brelse(bh);
	}
done:
	brelse(bh);
}

#ifdef CONFIG_BSD_DISKLABEL
/*
 * Create devices for BSD partitions listed in a disklabel, under a
 * dos-like partition. See extended_partition() for more information.
 */
static void bsd_disklabel_partition(struct gendisk *hd, kdev_t dev)
{
	struct buffer_head *bh;
	struct bsd_disklabel *l;
	struct bsd_partition *p;
	int mask = (1 << hd->minor_shift) - 1;

	if (!(bh = bread(dev,0,1024)))
		return;
	bh->b_state = 0;
	l = (struct bsd_disklabel *) (bh->b_data+512);
	if (l->d_magic != BSD_DISKMAGIC) {
		brelse(bh);
		return;
	}

	p = &l->d_partitions[0];
	while (p - &l->d_partitions[0] <= BSD_MAXPARTITIONS) {
		if ((current_minor & mask) >= (4 + hd->max_p))
			break;

		if (p->p_fstype != BSD_FS_UNUSED) {
#ifdef MACH
		  add_bsd_partition (hd, current_minor,
				     p - &l->d_partitions[0] + 'a',
				     p->p_offset, p->p_size);
#else
			add_partition(hd, current_minor, p->p_offset, p->p_size);
#endif
			current_minor++;
		}
		p++;
	}
	brelse(bh);

}
#endif

#ifdef CONFIG_GPT_DISKLABEL
/*
 * Compute a CRC32 but treat some range as if it were zeros.
 *
 * Straight copy of ether_crc_le() from linux/pcmcia-cs/include/linux/crc32.h, except for the first if/else
 */
static inline unsigned ether_crc_le_hole(int length, unsigned char *data, unsigned int skip_offset, unsigned int skip_length)
{
	static unsigned const ethernet_polynomial_le = 0xedb88320U;
	unsigned int crc = 0xffffffff;      /* Initial value. */
	while(--length >= 0) {
		unsigned char current_octet = *data++;
		if(skip_offset == 0 && skip_length-- != 0)
			current_octet = 0;
		else
			--skip_offset;
		int bit;
		for (bit = 8; --bit >= 0; current_octet >>= 1) {
			if ((crc ^ current_octet) & 1) {
				crc >>= 1;
				crc ^= ethernet_polynomial_le;
			} else
				crc >>= 1;
		}
	}
	return crc;
}

/*
 * Read in a full GPT array into a contiguous chunk, allocates *PP_S bytes into *PP.
 *
 * An attempt to do as few round-trips as possible is made by reading a PAGE_SIZE at a time,
 * since that's the bread() maximum.
 */
static int gpt_read_part_table(void **pp, vm_size_t *pp_s, kdev_t dev, int bsize, __u64 first_sector, struct gpt_disklabel_header *h)
{
	__u64 lba = first_sector + h->h_part_table_lba;
	__u32 bytes_left = *pp_s = h->h_part_table_len * h->h_part_table_entry_size;
	struct buffer_head *bh;
	void *cur = *pp = (void *)kalloc(*pp_s);
	if (!cur) {
		printk(" unable to allocate GPT partition table buffer");
		return -2;
	}

	while (bytes_left) {
		unsigned bytes_to_read = MIN(bytes_left, PAGE_SIZE);
		if(!(bh = bread(dev, lba, bytes_to_read))) {
			printk(" unable to read partition table array");
			return -3;
		}

		memcpy(cur, bh->b_data, bytes_to_read);
		cur += bytes_to_read;
		bytes_left -= bytes_to_read;
		lba += PAGE_SIZE / bsize;

		brelse(bh);
	}

	return 0;
}

/*
 * Sequence from section 5.3.2 of spec 2.8A:
 * signature, CRC, lba_current matches, partition table CRC, primary: check backup for validity
 */
static int gpt_verify_header(void **pp, vm_size_t *pp_s, kdev_t dev, int bsize, __u64 first_sector, __u64 lba, struct gpt_disklabel_header *h)
{
	int res;
	__u32 crc;

	if (memcmp(h->h_signature, GPT_SIGNATURE, strlen(GPT_SIGNATURE)) != 0) {
		printk(" bad GPT signature \"%c%c%c%c%c%c%c%c\";",
			h->h_signature[0], h->h_signature[1], h->h_signature[2], h->h_signature[3],
			h->h_signature[4], h->h_signature[5], h->h_signature[6], h->h_signature[7]);
		return 1;
	}

	crc = ether_crc_le_hole(h->h_header_size, (void *)h,
		offsetof(struct gpt_disklabel_header, h_header_crc), sizeof(h->h_header_crc)) ^ ~0;
	if (crc != h->h_header_crc) {
		printk(" bad header CRC: %x != %x;", crc, h->h_header_crc);
		return 2;
	}

	if (h->h_lba_current != lba) {
		printk(" current LBA mismatch: %lld != %lld;", h->h_lba_current, lba);
		return 3;
	}

	if (*pp) {
		kfree((vm_offset_t)*pp, *pp_s);
		*pp = NULL;
	}
	if ((res = gpt_read_part_table(pp, pp_s, dev, bsize, first_sector, h)))
		return res;

	crc = ether_crc_le_hole(*pp_s, *pp, 0, 0) ^ ~0;
	if (crc != h->h_part_table_crc) {
		printk(" bad partition table CRC: %x != %x;", crc, h->h_part_table_crc);
		return 4;
	}

	for (int i = h->h_header_size; i < bsize; ++i)
		res |= ((char*)h)[i];
	if (res) {
		printk(" rest of GPT block dirty;");
		return 5;
	}

	return 0;
}

static void gpt_print_part_name(struct gpt_disklabel_part *p)
{
	for(int n = 0; n < sizeof(p->p_name) / sizeof(*p->p_name) && p->p_name[n]; ++n)
		if(p->p_name[n] & ~0xFF)
			printk("?");	/* Can't support all of Unicode, but don't print garbage at least... */
		else
			printk("%c", p->p_name[n]);
}

#ifdef DEBUG
static void gpt_print_guid(struct gpt_guid *guid)
{
	printk("%08X-%04X-%04X-%02X%02X-", guid->g_time_low, guid->g_time_mid, guid->g_time_high_version, guid->g_clock_sec_high, guid->g_clock_sec_low);
	for (int i = 0; i < sizeof(guid->g_node_id); ++i)
		printk("%02X", guid->g_node_id[i]);
}

static void gpt_dump_header(struct gpt_disklabel_header *h)
{
	printk(" [h_signature: \"%c%c%c%c%c%c%c%c\"; ",
		h->h_signature[0], h->h_signature[1], h->h_signature[2], h->h_signature[3],
		h->h_signature[4], h->h_signature[5], h->h_signature[6], h->h_signature[7]);
	printk("h_revision: %x; ", h->h_revision);
	printk("h_header_size: %u; ", h->h_header_size);
	printk("h_header_crc: %x; ", h->h_header_crc);
	printk("h_reserved: %u; ", h->h_reserved);
	printk("h_lba_current: %llu; ", h->h_lba_current);
	printk("h_lba_backup: %llu; ", h->h_lba_backup);
	printk("h_lba_usable_first: %llu; ", h->h_lba_usable_first);
	printk("h_lba_usable_last: %llu; ", h->h_lba_usable_last);
	printk("h_guid: "); gpt_print_guid(&h->h_guid); printk("; ");
	printk("h_part_table_lba: %llu; ", h->h_part_table_lba);
	printk("h_part_table_len: %u; ", h->h_part_table_len);
	printk("h_part_table_crc: %x]", h->h_part_table_crc);
}

static void gpt_dump_part(struct gpt_disklabel_part *p, int i)
{
	printk(" part#%d:[", i);
	printk("p_type: "); gpt_print_guid(&p->p_type);
	printk("; p_guid:"); gpt_print_guid(&p->p_guid);
	printk("; p_lba_first: %llu", p->p_lba_first);
	printk("; p_lba_last: %llu", p->p_lba_last);
	printk("; p_attrs: %llx", p->p_attrs);
	printk("; p_name: \""); gpt_print_part_name(p); printk("\"]");
}
#else
static void gpt_dump_header(struct gpt_disklabel_header *h) {}
static void gpt_dump_part(struct gpt_disklabel_part *p, int i) {}
#endif

static int gpt_partition(struct gendisk *hd, kdev_t dev, __u64 first_sector, int minor)
{
	struct buffer_head *bh;
	struct gpt_disklabel_header *h;
	void *pp = NULL; vm_size_t pp_s = 0;
	int res, bsize = 512;
	/* Note: this must be set by the driver; SCSI does --
	 *       only, in practice, it always sets this to 512, see sd_init() in sd.c */
	if (hardsect_size[MAJOR(dev)] && hardsect_size[MAJOR(dev)][MINOR(dev)])
		bsize = hardsect_size[MAJOR(dev)][MINOR(dev)];
	set_blocksize(dev,bsize);	/* Must override read block size since GPT has pointers, stolen from amiga_partition(). */
	if (!(bh = bread(dev, first_sector + 1, bsize))) {
		printk("unable to read GPT");
		res = -1;
		goto done;
	}

	h = (struct gpt_disklabel_header *)bh->b_data;
	gpt_dump_header(h);

	res = gpt_verify_header(&pp, &pp_s, dev, bsize, first_sector, 1, h);
	if (res < 0)
		goto done;
	else if (res > 0) {
		printk(" main GPT dirty, trying backup at %llu;", h->h_lba_backup);
		__u64 lba = h->h_lba_backup;
		brelse(bh);

		if (!(bh = bread(dev, first_sector + lba, bsize))) {
			printk("unable to read backup GPT");
			res = -4;
			goto done;
		}

		h = (struct gpt_disklabel_header *)bh->b_data;
		gpt_dump_header(h);

		res = gpt_verify_header(&pp, &pp_s, dev, bsize, first_sector, lba, h);
		if (res < 0)
			goto done;
		else if (res > 0) {
			printk(" backup GPT dirty as well; cowardly refusing to continue");
			res = -5;
			goto done;
		}
	}

	/* At least one good GPT+array */

	for(int i = 0; i < h->h_part_table_len; ++i, ++minor) {
		struct gpt_disklabel_part *p =
			(struct gpt_disklabel_part *) (pp + i * h->h_part_table_entry_size);
		if(memcmp(&p->p_type, &GPT_GUID_TYPE_UNUSED, sizeof(struct gpt_guid)) == 0)
			continue;
		gpt_dump_part(p, i);

		if (minor > hd->max_nr * hd->max_p) {
			printk(" [ignoring GPT partition %d \"", i); gpt_print_part_name(p); printk("\": too many partitions (max %d)]", hd->max_p);
		} else {
			add_partition(hd, minor, first_sector + p->p_lba_first, p->p_lba_last - p->p_lba_first + 1);
			if(p->p_name[0]) {
				printk(" ("); gpt_print_part_name(p); printk(")");
			}
		}
	}

done:
	brelse(bh);
	set_blocksize(dev,BLOCK_SIZE);
	kfree((vm_offset_t)pp, pp_s);
	printk("\n");
	return !res;
}
#endif

static int msdos_partition(struct gendisk *hd, kdev_t dev, unsigned long first_sector)
{
	int i, minor = current_minor;
	struct buffer_head *bh;
	struct partition *p;
	unsigned char *data;
	int mask = (1 << hd->minor_shift) - 1;
#ifdef CONFIG_BLK_DEV_IDE
	int tested_for_xlate = 0;

read_mbr:
#endif
	if (!(bh = bread(dev,0,1024))) {
		printk(" unable to read partition table\n");
		return -1;
	}
	data = (unsigned char *)bh->b_data;
	/* In some cases we modify the geometry    */
	/*  of the drive (below), so ensure that   */
	/*  nobody else tries to re-use this data. */
	bh->b_state = 0;
#ifdef CONFIG_BLK_DEV_IDE
check_table:
#endif
	if (*(unsigned short *)  (0x1fe + data) != 0xAA55) {
		brelse(bh);
		return 0;
	}
	p = (struct partition *) (0x1be + data);

#ifdef CONFIG_BLK_DEV_IDE
	if (!tested_for_xlate++) {	/* Do this only once per disk */
		/*
		 * Look for various forms of IDE disk geometry translation
		 */
		extern int ide_xlate_1024(kdev_t, int, const char *);
		unsigned int sig = *(unsigned short *)(data + 2);
		if (SYS_IND(p) == EZD_PARTITION) {
			/*
			 * The remainder of the disk must be accessed using
			 * a translated geometry that reduces the number of
			 * apparent cylinders to less than 1024 if possible.
			 *
			 * ide_xlate_1024() will take care of the necessary
			 * adjustments to fool fdisk/LILO and partition check.
			 */
			if (ide_xlate_1024(dev, -1, " [EZD]")) {
				data += 512;
				goto check_table;
			}
		} else if (SYS_IND(p) == DM6_PARTITION) {

			/*
			 * Everything on the disk is offset by 63 sectors,
			 * including a "new" MBR with its own partition table,
			 * and the remainder of the disk must be accessed using
			 * a translated geometry that reduces the number of
			 * apparent cylinders to less than 1024 if possible.
			 *
			 * ide_xlate_1024() will take care of the necessary
			 * adjustments to fool fdisk/LILO and partition check.
			 */
			if (ide_xlate_1024(dev, 1, " [DM6:DDO]")) {
				brelse(bh);
				goto read_mbr;	/* start over with new MBR */
			}
		} else if (sig <= 0x1ae && *(unsigned short *)(data + sig) == 0x55AA
			 && (1 & *(unsigned char *)(data + sig + 2)) )
		{
			/*
			 * DM6 signature in MBR, courtesy of OnTrack
			 */
			(void) ide_xlate_1024 (dev, 0, " [DM6:MBR]");
		} else if (SYS_IND(p) == DM6_AUX1PARTITION || SYS_IND(p) == DM6_AUX3PARTITION) {
			/*
			 * DM6 on other than the first (boot) drive
			 */
			(void) ide_xlate_1024(dev, 0, " [DM6:AUX]");
		} else {
			/*
			 * Examine the partition table for common translations.
			 * This is necessary for drives for situations where
			 * the translated geometry is unavailable from the BIOS.
			 */
			for (i = 0; i < 4 ; i++) {
				struct partition *q = &p[i];
				if (NR_SECTS(q)
				   && (q->sector & 63) == 1
				   && (q->end_sector & 63) == 63) {
					unsigned int heads = q->end_head + 1;
					if (heads == 32 || heads == 64 || heads == 128 || heads == 255) {

						(void) ide_xlate_1024(dev, heads, " [PTBL]");
						break;
					}
				}
			}
		}
	}
#endif	/* CONFIG_BLK_DEV_IDE */

	current_minor += 4;  /* first "extra" minor (for extended partitions) */
	for (i=1 ; i<=4 ; minor++,i++,p++) {
		if (!NR_SECTS(p))
			continue;
#ifdef CONFIG_GPT_DISKLABEL
		if (SYS_IND(p) == GPT_PARTITION) {
			brelse(bh);
			return gpt_partition(hd, dev, first_sector, minor);
		} else
#endif
		add_partition(hd, minor, first_sector+START_SECT(p), NR_SECTS(p));
		if (is_extended_partition(p)) {
			printk(" <");
			/*
			 * If we are rereading the partition table, we need
			 * to set the size of the partition so that we will
			 * be able to bread the block containing the extended
			 * partition info.
			 */
			hd->sizes[minor] = hd->part[minor].nr_sects
			  	>> (BLOCK_SIZE_BITS - 9);
			extended_partition(hd, MKDEV(hd->major, minor));
			printk(" >");
			/* prevent someone doing mkfs or mkswap on an
			   extended partition, but leave room for LILO */
			if (hd->part[minor].nr_sects > 2)
				hd->part[minor].nr_sects = 2;
		}
#ifdef CONFIG_BSD_DISKLABEL
		if (SYS_IND(p) == BSD_PARTITION) {
			printk(" <");
#ifdef MACH
			mach_minor = minor;
#endif
			bsd_disklabel_partition(hd, MKDEV(hd->major, minor));
			printk(" >");
		}
#endif
	}
	/*
	 *  Check for old-style Disk Manager partition table
	 */
	if (*(unsigned short *) (data+0xfc) == 0x55AA) {
		p = (struct partition *) (0x1be + data);
		for (i = 4 ; i < 16 ; i++, current_minor++) {
			p--;
			if ((current_minor & mask) == 0)
				break;
			if (!(START_SECT(p) && NR_SECTS(p)))
				continue;
			add_partition(hd, current_minor, START_SECT(p), NR_SECTS(p));
		}
	}
	printk("\n");
	brelse(bh);
	return 1;
}

#endif /* CONFIG_MSDOS_PARTITION */

#ifdef CONFIG_OSF_PARTITION

static int osf_partition(struct gendisk *hd, unsigned int dev, unsigned long first_sector)
{
	int i;
	int mask = (1 << hd->minor_shift) - 1;
	struct buffer_head *bh;
	struct disklabel {
		u32 d_magic;
		u16 d_type,d_subtype;
		u8 d_typename[16];
		u8 d_packname[16];
		u32 d_secsize;
		u32 d_nsectors;
		u32 d_ntracks;
		u32 d_ncylinders;
		u32 d_secpercyl;
		u32 d_secprtunit;
		u16 d_sparespertrack;
		u16 d_sparespercyl;
		u32 d_acylinders;
		u16 d_rpm, d_interleave, d_trackskew, d_cylskew;
		u32 d_headswitch, d_trkseek, d_flags;
		u32 d_drivedata[5];
		u32 d_spare[5];
		u32 d_magic2;
		u16 d_checksum;
		u16 d_npartitions;
		u32 d_bbsize, d_sbsize;
		struct d_partition {
			u32 p_size;
			u32 p_offset;
			u32 p_fsize;
			u8  p_fstype;
			u8  p_frag;
			u16 p_cpg;
		} d_partitions[8];
	} * label;
	struct d_partition * partition;
#define DISKLABELMAGIC (0x82564557UL)

	if (!(bh = bread(dev,0,1024))) {
		printk("unable to read partition table\n");
		return -1;
	}
	label = (struct disklabel *) (bh->b_data+64);
	partition = label->d_partitions;
	if (label->d_magic != DISKLABELMAGIC) {
		printk("magic: %08x\n", label->d_magic);
		brelse(bh);
		return 0;
	}
	if (label->d_magic2 != DISKLABELMAGIC) {
		printk("magic2: %08x\n", label->d_magic2);
		brelse(bh);
		return 0;
	}
	for (i = 0 ; i < label->d_npartitions; i++, partition++) {
		if ((current_minor & mask) == 0)
		        break;
		if (partition->p_size)
			add_partition(hd, current_minor,
				first_sector+partition->p_offset,
				partition->p_size);
		current_minor++;
	}
	printk("\n");
	brelse(bh);
	return 1;
}

#endif /* CONFIG_OSF_PARTITION */

#ifdef CONFIG_SUN_PARTITION

static int sun_partition(struct gendisk *hd, kdev_t dev, unsigned long first_sector)
{
	int i, csum;
	unsigned short *ush;
	struct buffer_head *bh;
	struct sun_disklabel {
		unsigned char info[128];   /* Informative text string */
		unsigned char spare[292];  /* Boot information etc. */
		unsigned short rspeed;     /* Disk rotational speed */
		unsigned short pcylcount;  /* Physical cylinder count */
		unsigned short sparecyl;   /* extra sects per cylinder */
		unsigned char spare2[4];   /* More magic... */
		unsigned short ilfact;     /* Interleave factor */
		unsigned short ncyl;       /* Data cylinder count */
		unsigned short nacyl;      /* Alt. cylinder count */
		unsigned short ntrks;      /* Tracks per cylinder */
		unsigned short nsect;      /* Sectors per track */
		unsigned char spare3[4];   /* Even more magic... */
		struct sun_partition {
			__u32 start_cylinder;
			__u32 num_sectors;
		} partitions[8];
		unsigned short magic;      /* Magic number */
		unsigned short csum;       /* Label xor'd checksum */
	} * label;
	struct sun_partition *p;
	int other_endian;
	unsigned long spc;
#define SUN_LABEL_MAGIC          0xDABE
#define SUN_LABEL_MAGIC_SWAPPED  0xBEDA
/* No need to optimize these macros since they are called only when reading
 * the partition table. This occurs only at each disk change. */
#define SWAP16(x)  (other_endian ? (((__u16)(x) & 0xFF) << 8) \
				 | (((__u16)(x) & 0xFF00) >> 8) \
				 : (__u16)(x))
#define SWAP32(x)  (other_endian ? (((__u32)(x) & 0xFF) << 24) \
				 | (((__u32)(x) & 0xFF00) << 8) \
				 | (((__u32)(x) & 0xFF0000) >> 8) \
				 | (((__u32)(x) & 0xFF000000) >> 24) \
				 : (__u32)(x))

	if(!(bh = bread(dev, 0, 1024))) {
		printk("Dev %s: unable to read partition table\n",
		       kdevname(dev));
		return -1;
	}
	label = (struct sun_disklabel *) bh->b_data;
	p = label->partitions;
	if (label->magic != SUN_LABEL_MAGIC && label->magic != SUN_LABEL_MAGIC_SWAPPED) {
		printk("Dev %s Sun disklabel: bad magic %04x\n",
		       kdevname(dev), label->magic);
		brelse(bh);
		return 0;
	}
	other_endian = (label->magic == SUN_LABEL_MAGIC_SWAPPED);
	/* Look at the checksum */
	ush = ((unsigned short *) (label+1)) - 1;
	for(csum = 0; ush >= ((unsigned short *) label);)
		csum ^= *ush--;
	if(csum) {
		printk("Dev %s Sun disklabel: Csum bad, label corrupted\n",
		       kdevname(dev));
		brelse(bh);
		return 0;
	}
	/* All Sun disks have 8 partition entries */
	spc = SWAP16(label->ntrks) * SWAP16(label->nsect);
	for(i=0; i < 8; i++, p++) {
		unsigned long st_sector;

		/* We register all partitions, even if zero size, so that
		 * the minor numbers end up ok as per SunOS interpretation.
		 */
		st_sector = first_sector + SWAP32(p->start_cylinder) * spc;
		add_partition(hd, current_minor, st_sector, SWAP32(p->num_sectors));
		current_minor++;
	}
	printk("\n");
	brelse(bh);
	return 1;
#undef SWAP16
#undef SWAP32
}

#endif /* CONFIG_SUN_PARTITION */

#ifdef CONFIG_AMIGA_PARTITION
#include <asm/byteorder.h>
#include <linux/affs_hardblocks.h>

static __inline__ __u32
checksum_block(__u32 *m, int size)
{
	__u32 sum = 0;

	while (size--)
		sum += htonl(*m++);
	return sum;
}

static int
amiga_partition(struct gendisk *hd, unsigned int dev, unsigned long first_sector)
{
	struct buffer_head	*bh;
	struct RigidDiskBlock	*rdb;
	struct PartitionBlock	*pb;
	int			 start_sect;
	int			 nr_sects;
	int			 blk;
	int			 part, res;

	set_blocksize(dev,512);
	res = 0;

	for (blk = 0; blk < RDB_ALLOCATION_LIMIT; blk++) {
		if(!(bh = bread(dev,blk,512))) {
			printk("Dev %d: unable to read RDB block %d\n",dev,blk);
			goto rdb_done;
		}
		if (*(__u32 *)bh->b_data == htonl(IDNAME_RIGIDDISK)) {
			rdb = (struct RigidDiskBlock *)bh->b_data;
			if (checksum_block((__u32 *)bh->b_data,htonl(rdb->rdb_SummedLongs) & 0x7F)) {
				printk("Dev %d: RDB in block %d has bad checksum\n",dev,blk);
				brelse(bh);
				continue;
			}
			printk(" RDSK");
			blk = htonl(rdb->rdb_PartitionList);
			brelse(bh);
			for (part = 1; blk > 0 && part <= 16; part++) {
				if (!(bh = bread(dev,blk,512))) {
					printk("Dev %d: unable to read partition block %d\n",
						       dev,blk);
					goto rdb_done;
				}
				pb  = (struct PartitionBlock *)bh->b_data;
				blk = htonl(pb->pb_Next);
				if (pb->pb_ID == htonl(IDNAME_PARTITION) && checksum_block(
				    (__u32 *)pb,htonl(pb->pb_SummedLongs) & 0x7F) == 0 ) {

					/* Tell Kernel about it */

					if (!(nr_sects = (htonl(pb->pb_Environment[10]) + 1 -
							  htonl(pb->pb_Environment[9])) *
							 htonl(pb->pb_Environment[3]) *
							 htonl(pb->pb_Environment[5]))) {
						continue;
					}
					start_sect = htonl(pb->pb_Environment[9]) *
						     htonl(pb->pb_Environment[3]) *
						     htonl(pb->pb_Environment[5]);
					add_partition(hd,current_minor,start_sect,nr_sects);
					current_minor++;
					res = 1;
				}
				brelse(bh);
			}
			printk("\n");
			break;
		}
	}

rdb_done:
	set_blocksize(dev,BLOCK_SIZE);
	return res;
}
#endif /* CONFIG_AMIGA_PARTITION */

static void check_partition(struct gendisk *hd, kdev_t dev)
{
	static int first_time = 1;
	unsigned long first_sector;
	char buf[8];

	if (first_time)
		printk("Partition check (DOS partitions):\n");
	first_time = 0;
	first_sector = hd->part[MINOR(dev)].start_sect;

	/*
	 * This is a kludge to allow the partition check to be
	 * skipped for specific drives (e.g. IDE cd-rom drives)
	 */
	if ((int)first_sector == -1) {
		hd->part[MINOR(dev)].start_sect = 0;
		return;
	}

	printk(" %s:", disk_name(hd, MINOR(dev), buf));
#ifdef CONFIG_MSDOS_PARTITION
	if (msdos_partition(hd, dev, first_sector))
		return;
#endif
#ifdef CONFIG_OSF_PARTITION
	if (osf_partition(hd, dev, first_sector))
		return;
#endif
#ifdef CONFIG_SUN_PARTITION
	if(sun_partition(hd, dev, first_sector))
		return;
#endif
#ifdef CONFIG_AMIGA_PARTITION
	if(amiga_partition(hd, dev, first_sector))
		return;
#endif
	printk(" unknown partition table\n");
}

/* This function is used to re-read partition tables for removable disks.
   Much of the cleanup from the old partition tables should have already been
   done */

/* This function will re-read the partition tables for a given device,
and set things back up again.  There are some important caveats,
however.  You must ensure that no one is using the device, and no one
can start using the device while this function is being executed. */

void resetup_one_dev(struct gendisk *dev, int drive)
{
	int i;
	int first_minor	= drive << dev->minor_shift;
	int end_minor	= first_minor + dev->max_p;

	blk_size[dev->major] = NULL;
	current_minor = 1 + first_minor;
	check_partition(dev, MKDEV(dev->major, first_minor));

 	/*
 	 * We need to set the sizes array before we will be able to access
 	 * any of the partitions on this device.
 	 */
	if (dev->sizes != NULL) {	/* optional safeguard in ll_rw_blk.c */
		for (i = first_minor; i < end_minor; i++)
			dev->sizes[i] = dev->part[i].nr_sects >> (BLOCK_SIZE_BITS - 9);
		blk_size[dev->major] = dev->sizes;
	}
}

static void setup_dev(struct gendisk *dev)
{
	int i, drive;
	int end_minor	= dev->max_nr * dev->max_p;

	blk_size[dev->major] = NULL;
	for (i = 0 ; i < end_minor; i++) {
		dev->part[i].start_sect = 0;
		dev->part[i].nr_sects = 0;
	}
	dev->init(dev);
	for (drive = 0 ; drive < dev->nr_real ; drive++) {
		int first_minor	= drive << dev->minor_shift;
		current_minor = 1 + first_minor;
		check_partition(dev, MKDEV(dev->major, first_minor));
	}
	if (dev->sizes != NULL) {	/* optional safeguard in ll_rw_blk.c */
		for (i = 0; i < end_minor; i++)
			dev->sizes[i] = dev->part[i].nr_sects >> (BLOCK_SIZE_BITS - 9);
		blk_size[dev->major] = dev->sizes;
	}
}

void device_setup(void)
{
	extern void console_map_init(void);
	struct gendisk *p;
	int nr=0;

#ifdef CONFIG_BLK_DEV_IDE
	extern char *kernel_cmdline;
	char *c, *param, *white;

	for (c = kernel_cmdline; c; )
	{
		param = strstr(c, " ide");
		if (!param)
			param = strstr(c, " hd");
		if (!param)
			break;
		if (param) {
			param++;
			white = strchr(param, ' ');
			if (!white) {
				ide_setup(param);
				c = NULL;
			} else {
				char *word = alloca(white - param + 1);
				strncpy(word, param, white - param);
				word[white-param] = '\0';
				ide_setup(word);
				c = white + 1;
			}
		}
	}
#endif
#ifndef MACH
	chr_dev_init();
#endif
	blk_dev_init();
	sti();
#ifdef CONFIG_SCSI
	scsi_dev_init();
#endif
#ifdef CONFIG_INET
	net_dev_init();
#endif
#ifndef MACH
	console_map_init();
#endif

	for (p = gendisk_head ; p ; p=p->next) {
		setup_dev(p);
		nr += p->nr_real;
	}
#ifdef CONFIG_BLK_DEV_RAM
#ifdef CONFIG_BLK_DEV_INITRD
	if (initrd_start && mount_initrd) initrd_load();
	else
#endif
	rd_load();
#endif
}
