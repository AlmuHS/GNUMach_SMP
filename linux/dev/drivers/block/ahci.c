/*
 *  Copyright (C) 2013 Free Software Foundation
 *
 * This program is free software ; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation ; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY ; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the program ; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <ahci.h>
#include <kern/assert.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/fs.h>
#include <linux/bios32.h>
#include <linux/major.h>
#include <linux/hdreg.h>
#include <linux/genhd.h>
#include <asm/io.h>

#define MAJOR_NR SCSI_DISK_MAJOR
#include <linux/blk.h>

/* Standard AHCI BAR for mmio */
#define AHCI_PCI_BAR 5

/* minor: 2 bits for device number, 6 bits for partition number. */

#define MAX_PORTS 4
#define PARTN_BITS 6
#define PARTN_MASK ((1<<PARTN_BITS)-1)

/* We need to use one DMA scatter element per physical page.
 * ll_rw_block creates at most 8 buffer heads */
/* See MAX_BUF */
#define PRDTL_SIZE 8

#define WAIT_MAX (1*HZ) /* Wait at most 1s for requests completion */

/* AHCI standard structures */

struct ahci_prdt {
	u32 dba;			/* Data base address */
	u32 dbau;			/* upper 32bit */
	u32 rsv0;			/* Reserved */

	u32 dbc;			/* Byte count bits 0-21,
					 * bit31 interrupt on completion. */
};

struct ahci_cmd_tbl {
	u8 cfis[64];
	u8 acmd[16];
	u8 rsv[48];

	struct ahci_prdt prdtl[PRDTL_SIZE];
};

struct ahci_command {
	u32 opts;			/* Command options */

	u32 prdbc;			/* Physical Region Descriptor byte count */

	u32 ctba;			/* Command Table Descriptor Base Address */
	u32 ctbau;			/* upper 32bit */

	u32 rsv1[4];			/* Reserved */
};

struct ahci_fis_dma {
	u8 fis_type;
	u8 flags;
	u8 rsved[2];
	u64 id;
	u32 rsvd;
	u32 offset;
	u32 count;
	u32 resvd;
};

struct ahci_fis_pio {
	u8 fis_type;
	u8 flags;
	u8 status;
	u8 error;

	u8 lba0;
	u8 lba1;
	u8 lba2;
	u8 device;

	u8 lba3;
	u8 lba4;
	u8 lba5;
	u8 rsv2;

	u8 countl;
	u8 counth;
	u8 rsv3;
	u8 e_status;

	u16 tc;	/* Transfer Count */
	u8 rsv4[2];
};

struct ahci_fis_d2h {
	u8 fis_type;
	u8 flags;
	u8 status;
	u8 error;

	u8 lba0;
	u8 lba1;
	u8 lba2;
	u8 device;

	u8 lba3;
	u8 lba4;
	u8 lba5;
	u8 rsv2;

	u8 countl;
	u8 counth;
	u8 rsv3[2];

	u8 rsv4[4];
};

struct ahci_fis_dev {
	u8 rsvd[8];
};

struct ahci_fis_h2d {
	u8 fis_type;
	u8 flags;
	u8 command;
	u8 featurel;

	u8 lba0;
	u8 lba1;
	u8 lba2;
	u8 device;

	u8 lba3;
	u8 lba4;
	u8 lba5;
	u8 featureh;

	u8 countl;
	u8 counth;
	u8 icc;
	u8 control;

	u8 rsv1[4];
};

struct ahci_fis_data {
	u8 fis_type;
	u8 flags;
	u8 rsv1[2];
	u32 data1[];
};

struct ahci_fis {
	struct ahci_fis_dma dma_fis;
	u8 pad0[4];

	struct ahci_fis_pio pio_fis;
	u8 pad1[12];

	struct ahci_fis_d2h d2h_fis;
	u8 pad2[4];

	struct ahci_fis_dev dev_fis;

	u8 ufis[64];

	u8 rsv[0x100 - 0xa0];
};

struct ahci_port {
	u32 clb;			/* Command List Base address */
	u32 clbu;			/* upper 32bit */
	u32 fb;				/* FIS Base */
	u32 fbu;			/* upper 32bit */
	u32 is;				/* Interrupt Status */
	u32 ie;				/* Interrupt Enable */
	u32 cmd;			/* Command and Status */
	u32 rsv0;			/* Reserved */
	u32 tfd;			/* Task File Data */
	u32 sig;			/* Signature */
	u32 ssts;			/* SATA Status */
	u32 sctl;			/* SATA Control */
	u32 serr;			/* SATA Error */
	u32 sact;			/* SATA Active */
	u32 ci;				/* Command Issue */
	u32 sntf;			/* SATA Notification */
	u32 fbs;			/* FIS-based switch control */
	u8 rsv1[0x70 - 0x44];		/* Reserved */
	u8 vendor[0x80 - 0x70];		/* Vendor-specific */
};

struct ahci_host {
	u32 cap;			/* Host capabilities */
	u32 ghc;			/* Global Host Control */
	u32 is;				/* Interrupt Status */
	u32 pi;				/* Port Implemented */
	u32 v;				/* Version */
	u32 ccc_ctl;			/* Command Completion Coalescing control */
	u32 ccc_pts;			/* Command Completion Coalescing ports */
	u32 em_loc;			/* Enclosure Management location */
	u32 em_ctrl;			/* Enclosure Management control */
	u32 cap2;			/* Host capabilities extended */
	u32 bohc;			/* BIOS/OS Handoff Control and status */
	u8 rsv[0xa0 - 0x2c];		/* Reserved */
	u8 vendor[0x100 - 0xa0];	/* Vendor-specific */
	struct ahci_port ports[];	/* Up to 32 ports */
};

/* Our own data */

static struct port {
	/* memory-mapped regions */
	const volatile struct ahci_host *ahci_host;
	const volatile struct ahci_port *ahci_port;

	/* host-memory buffers */
	struct ahci_command *command;
	struct ahci_fis *fis;
	struct ahci_cmd_tbl *prdtl;

	struct hd_driveid id;
	unsigned is_cd;
	unsigned long long capacity;	/* Nr of sectors */
	u32 status;			/* interrupt status */
	unsigned cls;			/* Command list maximum size.
					   We currently only use 1. */
	struct wait_queue *q;		/* IRQ wait queue */
	struct hd_struct *part;		/* drive partition table */
	unsigned lba48;			/* Whether LBA48 is supported */
	unsigned identify;		/* Whether we are just identifying
					   at boot */
	struct gendisk *gd;
} ports[MAX_PORTS];


/* do_request() gets called by the block layer to push a request to the disk.
   We just push one, and when an interrupt tells it's over, we call do_request()
   ourself again to push the next request, etc. */

/* Request completed, either successfully or with an error */
static void ahci_end_request(int uptodate)
{
	struct request *rq = CURRENT;
	struct buffer_head *bh;

	rq->errors = 0;
	if (!uptodate) {
		if (!rq->quiet)
			printk("end_request: I/O error, dev %s, sector %lu\n",
					kdevname(rq->rq_dev), rq->sector);
	}

	for (bh = rq->bh; bh; )
	{
		struct buffer_head *next = bh->b_reqnext;
		bh->b_reqnext = NULL;
		mark_buffer_uptodate (bh, uptodate);
		unlock_buffer (bh);
		bh = next;
	}

	CURRENT = rq->next;
	if (rq->sem != NULL)
		up(rq->sem);
	rq->rq_status = RQ_INACTIVE;
	wake_up(&wait_for_request);
}

/* Push the request to the controler port */
static void ahci_do_port_request(struct port *port, unsigned long long sector, struct request *rq)
{
	struct ahci_command *command = port->command;
	struct ahci_cmd_tbl *prdtl = port->prdtl;
	struct ahci_fis_h2d *fis_h2d;
	unsigned slot = 0;
	struct buffer_head *bh;
	unsigned i;

	rq->rq_status = RQ_SCSI_BUSY;

	/* Shouldn't ever happen: the block glue is limited at 8 blocks */
	assert(rq->nr_sectors < 0x10000);

	fis_h2d = (void*) &prdtl[slot].cfis;
	fis_h2d->fis_type = FIS_TYPE_REG_H2D;
	fis_h2d->flags = 128;
	if (port->lba48)
		if (rq->cmd == READ)
			fis_h2d->command = WIN_READDMA_EXT;
		else
			fis_h2d->command = WIN_WRITEDMA_EXT;
	else
		if (rq->cmd == READ)
			fis_h2d->command = WIN_READDMA;
		else
			fis_h2d->command = WIN_WRITEDMA;

	fis_h2d->device = 1<<6;	/* LBA */

	fis_h2d->lba0 = sector;
	fis_h2d->lba1 = sector >> 8;
	fis_h2d->lba2 = sector >> 16;

	fis_h2d->lba3 = sector >> 24;
	fis_h2d->lba4 = sector >> 32;
	fis_h2d->lba5 = sector >> 40;

	fis_h2d->countl = rq->nr_sectors;
	fis_h2d->counth = rq->nr_sectors >> 8;

	command[slot].opts = sizeof(*fis_h2d) / sizeof(u32);

	if (rq->cmd == WRITE)
		command[slot].opts |= AHCI_CMD_WRITE;

	for (i = 0, bh = rq->bh; bh; i++, bh = bh->b_reqnext)
	{
		assert(i < PRDTL_SIZE);
		assert((((unsigned long) bh->b_data) & ~PAGE_MASK) ==
			(((unsigned long) bh->b_data + bh->b_size - 1) & ~PAGE_MASK));
		prdtl[slot].prdtl[i].dbau = 0;
		prdtl[slot].prdtl[i].dba = vmtophys(bh->b_data);
		prdtl[slot].prdtl[i].dbc = bh->b_size - 1;
	}

	command[slot].opts |= i << 16;

	/* Make sure main memory buffers are up to date */
	mb();

	/* Issue command */
	writel(1 << slot, &port->ahci_port->ci);

	/* TODO: IRQ timeout handler */
}

/* Called by block core to push a request */
/* TODO: ideally, would have one request queue per port */
/* TODO: ideally, would use tags to process several requests at a time */
static void ahci_do_request()	/* invoked with cli() */
{
	struct request *rq;
	unsigned minor, unit;
	unsigned long long block, blockend;
	struct port *port;

	rq = CURRENT;
	if (!rq)
		return;

	if (rq->rq_status != RQ_ACTIVE)
		/* Current one is already ongoing, let the interrupt handler
		 * push the new one when the current one is finished. */
		return;

	if (MAJOR(rq->rq_dev) != MAJOR_NR) {
		printk("bad ahci major %u\n", MAJOR(rq->rq_dev));
		goto kill_rq;
	}

	minor = MINOR(rq->rq_dev);
	unit = minor >> PARTN_BITS;
	if (unit > MAX_PORTS) {
		printk("bad ahci unit %u\n", unit);
		goto kill_rq;
	}

	port = &ports[unit];

	/* Compute start sector */
	block = rq->sector;
	block += port->part[minor & PARTN_MASK].start_sect;

	/* And check end */
	blockend = block + rq->nr_sectors;
	if (blockend < block) {
		if (!rq->quiet)
			printk("bad blockend %lu vs %lu\n", (unsigned long) blockend, (unsigned long) block);
		goto kill_rq;
	}
	if (blockend > port->capacity) {
		if (!rq->quiet)
		{
			printk("offset for %u was %lu\n", minor, port->part[minor & PARTN_MASK].start_sect);
			printk("bad access: block %lu, count= %lu\n", (unsigned long) blockend, (unsigned long) port->capacity);
		}
		goto kill_rq;
	}

	/* Push this to the port */
	ahci_do_port_request(port, block, rq);
	return;

kill_rq:
	ahci_end_request(0);
}

/* The given port got an interrupt, terminate the current request if any */
static void ahci_port_interrupt(struct port *port, u32 status)
{
	unsigned slot = 0;

	if (readl(&port->ahci_port->ci) & (1 << slot)) {
		/* Command still pending */
		return;
	}

	if (port->identify) {
		port->status = status;
		wake_up(&port->q);
		return;
	}

	if (!CURRENT || CURRENT->rq_status != RQ_SCSI_BUSY) {
		/* No request currently running */
		return;
	}

	if (status & (PORT_IRQ_TF_ERR | PORT_IRQ_HBUS_ERR | PORT_IRQ_HBUS_DATA_ERR | PORT_IRQ_IF_ERR | PORT_IRQ_IF_NONFATAL)) {
		printk("ahci error %x %x\n", status, readl(&port->ahci_port->tfd));
		ahci_end_request(0);
		return;
	}

	ahci_end_request(1);
}

/* Start of IRQ handler. Iterate over all ports for this host */
static void ahci_interrupt (int irq, void *host, struct pt_regs *regs)
{
	struct port *port;
	struct ahci_host *ahci_host = host;
	u32 irq_mask;
	u32 status;

	irq_mask = readl(&ahci_host->is);

	if (!irq_mask)
		return;

	for (port = &ports[0]; port < &ports[MAX_PORTS]; port++) {
		if (port->ahci_host == ahci_host && (irq_mask & (1 << (port->ahci_port - ahci_host->ports)))) {
			status = readl(&port->ahci_port->is);
			/* Clear interrupt before possibly triggering others */
			writel(status, &port->ahci_port->is);
			ahci_port_interrupt (port, status);
		}
	}

	if (CURRENT)
		/* Still some requests, queue another one */
		ahci_do_request();

	/* Clear host after clearing ports */
	writel(irq_mask, &ahci_host->is);

	/* unlock */
}

static int ahci_ioctl (struct inode *inode, struct file *file,
			unsigned int cmd, unsigned long arg)
{
	int major, unit;

	if (!inode || !inode->i_rdev)
		return -EINVAL;

	major = MAJOR(inode->i_rdev);
	if (major != MAJOR_NR)
		return -ENOTTY;

	unit = DEVICE_NR(inode->i_rdev);
	if (unit >= MAX_PORTS)
		return -EINVAL;

	switch (cmd) {
		case BLKRRPART:
			if (!suser()) return -EACCES;
			if (!ports[unit].gd)
				return -EINVAL;
			resetup_one_dev(ports[unit].gd, unit);
			return 0;
		default:
			return -EPERM;
	}
}

static int ahci_open (struct inode *inode, struct file *file)
{
	int target;

	if (MAJOR(inode->i_rdev) != MAJOR_NR)
		return -ENXIO;

	target = MINOR(inode->i_rdev) >> PARTN_BITS;
	if (target >= MAX_PORTS)
		return -ENXIO;

	if (!ports[target].ahci_port)
		return -ENXIO;

	return 0;
}

static void ahci_release (struct inode *inode, struct file *file)
{
}

static int ahci_fsync (struct inode *inode, struct file *file)
{
	printk("fsync\n");
	return -ENOSYS;
}

static struct file_operations ahci_fops = {
	.lseek = NULL,
	.read = block_read,
	.write = block_write,
	.readdir = NULL,
	.select = NULL,
	.ioctl = ahci_ioctl,
	.mmap = NULL,
	.open = ahci_open,
	.release = ahci_release,
	.fsync = ahci_fsync,
	.fasync = NULL,
	.check_media_change = NULL,
	.revalidate = NULL,
};

/* Disk timed out while processing identify, interrupt ahci_probe_port */
static void identify_timeout(unsigned long data)
{
	struct port *port = (void*) data;

	wake_up(&port->q);
}

static struct timer_list identify_timer = { .function = identify_timeout };

static int ahci_identify(const volatile struct ahci_host *ahci_host, const volatile struct ahci_port *ahci_port, struct port *port, unsigned cmd)
{
	struct hd_driveid id;
	struct ahci_fis_h2d *fis_h2d;
	struct ahci_command *command = port->command;
	struct ahci_cmd_tbl *prdtl = port->prdtl;
	unsigned long flags;
	unsigned slot;
	unsigned long first_part;
	unsigned long long timeout;
	int ret = 0;

	/* Identify device */
	/* TODO: make this a request */
	slot = 0;

	fis_h2d = (void*) &prdtl[slot].cfis;
	fis_h2d->fis_type = FIS_TYPE_REG_H2D;
	fis_h2d->flags = 128;
	fis_h2d->command = cmd;
	fis_h2d->device = 0;

	/* Fetch the 512 identify data */
	memset(&id, 0, sizeof(id));

	command[slot].opts = sizeof(*fis_h2d) / sizeof(u32);

	first_part = PAGE_ALIGN((unsigned long) &id) - (unsigned long) &id;

	if (first_part && first_part < sizeof(id)) {
		/* split over two pages */

		command[slot].opts |= (2 << 16);

		prdtl[slot].prdtl[0].dbau = 0;
		prdtl[slot].prdtl[0].dba = vmtophys((void*) &id);
		prdtl[slot].prdtl[0].dbc = first_part - 1;
		prdtl[slot].prdtl[1].dbau = 0;
		prdtl[slot].prdtl[1].dba = vmtophys((void*) &id + first_part);
		prdtl[slot].prdtl[1].dbc = sizeof(id) - first_part - 1;
	}
	else
	{
		command[slot].opts |= (1 << 16);

		prdtl[slot].prdtl[0].dbau = 0;
		prdtl[slot].prdtl[0].dba = vmtophys((void*) &id);
		prdtl[slot].prdtl[0].dbc = sizeof(id) - 1;
	}

	timeout = jiffies + WAIT_MAX;
	while (readl(&ahci_port->tfd) & (BUSY_STAT | DRQ_STAT))
		if (jiffies > timeout) {
			printk("sd%u: timeout waiting for ready\n", port-ports);
			port->ahci_host = NULL;
			port->ahci_port = NULL;
			return 3;
		}

	save_flags(flags);
	cli();

	port->identify = 1;
	port->status = 0;

	/* Issue command */
	mb();
	writel(1 << slot, &ahci_port->ci);

	timeout = jiffies + WAIT_MAX;
	identify_timer.expires = timeout;
	identify_timer.data = (unsigned long) port;
	add_timer(&identify_timer);
	while (!port->status) {
		if (jiffies >= timeout) {
			printk("sd%u: timeout waiting for ready\n", port-ports);
			port->ahci_host = NULL;
			port->ahci_port = NULL;
			del_timer(&identify_timer);
			return 3;
		}
		sleep_on(&port->q);
	}
	del_timer(&identify_timer);
	restore_flags(flags);

	if ((port->status & PORT_IRQ_TF_ERR) || readl(&ahci_port->is) & PORT_IRQ_TF_ERR)
	{
		/* Identify error */
		port->capacity = 0;
		port->lba48 = 0;
		ret = 2;
	} else {
		memcpy(&port->id, &id, sizeof(id));
		port->is_cd = 0;

		ide_fixstring(id.model,     sizeof(id.model),     1);
		ide_fixstring(id.fw_rev,    sizeof(id.fw_rev),    1);
		ide_fixstring(id.serial_no, sizeof(id.serial_no), 1);
		if (cmd == WIN_PIDENTIFY)
		{
			unsigned char type = (id.config >> 8) & 0x1f;

			printk("sd%u: %s, ATAPI ", port - ports, id.model);
			if (type == 5)
			{
				printk("unsupported CDROM drive\n");
				port->is_cd = 1;
				port->lba48 = 0;
				port->capacity = 0;
			}
			else
			{
				printk("unsupported type %d\n", type);
				port->lba48 = 0;
				port->capacity = 0;
				return 2;
			}
			return 0;
		}

		if (id.command_set_2 & (1U<<10))
		{
			port->lba48 = 1;
			port->capacity = id.lba_capacity_2;
			if (port->capacity >= (1ULL << 32))
			{
				port->capacity = (1ULL << 32) - 1;
				printk("Warning: truncating disk size to 2TiB\n");
			}
		}
		else
		{
			port->lba48 = 0;
			port->capacity = id.lba_capacity;
			if (port->capacity > (1ULL << 24))
			{
				port->capacity = (1ULL << 24);
				printk("Warning: truncating disk size to 128GiB\n");
			}
		}
		if (port->capacity/2048 >= 10240)
			printk("sd%u: %s, %uGB w/%dkB Cache\n", port - ports, id.model, (unsigned) (port->capacity/(2048*1024)), id.buf_size/2);
		else
			printk("sd%u: %s, %uMB w/%dkB Cache\n", port - ports, id.model, (unsigned) (port->capacity/2048), id.buf_size/2);
	}
	port->identify = 0;

	return ret;
}

/* Probe one AHCI port */
static void ahci_probe_port(const volatile struct ahci_host *ahci_host, const volatile struct ahci_port *ahci_port)
{
	struct port *port;
	void *mem;
	unsigned cls = ((readl(&ahci_host->cap) >> 8) & 0x1f) + 1;
	struct ahci_command *command;
	struct ahci_fis *fis;
	struct ahci_cmd_tbl *prdtl;
	vm_size_t size =
		  cls * sizeof(*command)
		+ sizeof(*fis)
		+ cls * sizeof(*prdtl);
	unsigned i;
	unsigned long long timeout;

	for (i = 0; i < MAX_PORTS; i++) {
		if (!ports[i].ahci_port)
			break;
	}
	if (i == MAX_PORTS)
		return;
	port = &ports[i];

	/* Has to be 1K-aligned */
	mem = vmalloc (size);
	if (!mem)
		return;
	assert (!(((unsigned long) mem) & (1024-1)));
	memset (mem, 0, size);

	port->ahci_host = ahci_host;
	port->ahci_port = ahci_port;
	port->cls = cls;

	port->command = command = mem;
	port->fis = fis = (void*) command + cls * sizeof(*command);
	port->prdtl = prdtl = (void*) fis + sizeof(*fis);

	/* Stop commands */
	writel(readl(&ahci_port->cmd) & ~PORT_CMD_START, &ahci_port->cmd);
	timeout = jiffies + WAIT_MAX;
	while (readl(&ahci_port->cmd) & PORT_CMD_LIST_ON)
		if (jiffies > timeout) {
			printk("sd%u: timeout waiting for list completion\n", port-ports);
			port->ahci_host = NULL;
			port->ahci_port = NULL;
			return;
		}

	writel(readl(&ahci_port->cmd) & ~PORT_CMD_FIS_RX, &ahci_port->cmd);
	timeout = jiffies + WAIT_MAX;
	while (readl(&ahci_port->cmd) & PORT_CMD_FIS_ON)
		if (jiffies > timeout) {
			printk("sd%u: timeout waiting for FIS completion\n", port-ports);
			port->ahci_host = NULL;
			port->ahci_port = NULL;
			return;
		}

	/* We don't support 64bit */
	/* Point controller to our buffers */
	writel(0, &ahci_port->clbu);
	writel(vmtophys((void*) command), &ahci_port->clb);
	writel(0, &ahci_port->fbu);
	writel(vmtophys((void*) fis), &ahci_port->fb);

	/* Clear any previous interrupts */
	writel(readl(&ahci_port->is), &ahci_port->is);
	writel(1 << (ahci_port - ahci_host->ports), &ahci_host->is);

	/* And activate them */
	writel(DEF_PORT_IRQ, &ahci_port->ie);
	writel(readl(&ahci_host->ghc) | HOST_IRQ_EN, &ahci_host->ghc);

	for (i = 0; i < cls; i++)
	{
		command[i].ctbau = 0;
		command[i].ctba = vmtophys((void*) &prdtl[i]);
	}

	/* Start commands */
	timeout = jiffies + WAIT_MAX;
	while (readl(&ahci_port->cmd) & PORT_CMD_LIST_ON)
		if (jiffies > timeout) {
			printk("sd%u: timeout waiting for list completion\n", port-ports);
			port->ahci_host = NULL;
			port->ahci_port = NULL;
			return;
		}

	writel(readl(&ahci_port->cmd) | PORT_CMD_FIS_RX | PORT_CMD_START, &ahci_port->cmd);

	if (ahci_identify(ahci_host, ahci_port, port, WIN_IDENTIFY) >= 2)
		/* Try ATAPI */
		ahci_identify(ahci_host, ahci_port, port, WIN_PIDENTIFY);
}

/* Probe one AHCI PCI device */
static void ahci_probe_dev(unsigned char bus, unsigned char device)
{
	unsigned char hdrtype;
	unsigned char dev, fun;
	const volatile struct ahci_host *ahci_host;
	const volatile struct ahci_port *ahci_port;
	unsigned nports, n, i;
	unsigned port_map;
	unsigned bar;
	unsigned char irq;

	dev = PCI_SLOT(device);
	fun = PCI_FUNC(device);

	/* Get configuration */
	if (pcibios_read_config_byte(bus, device, PCI_HEADER_TYPE, &hdrtype) != PCIBIOS_SUCCESSFUL) {
		printk("ahci: %02u:%02u.%u: Can not read configuration", bus, dev, fun);
		return;
	}

	if (hdrtype != 0) {
		printk("ahci: %02u:%02u.%u: Unknown hdrtype %d\n", bus, dev, fun, hdrtype);
		return;
	}

	if (pcibios_read_config_dword(bus, device, PCI_BASE_ADDRESS_5, &bar) != PCIBIOS_SUCCESSFUL) {
		printk("ahci: %02u:%02u.%u: Can not read BAR 5", bus, dev, fun);
		return;
	}
	if (bar & 0x01) {
		printk("ahci: %02u:%02u.%u: BAR 5 is I/O?!", bus, dev, fun);
		return;
	}
	bar &= ~0x0f;

	if (pcibios_read_config_byte(bus, device, PCI_INTERRUPT_LINE, &irq) != PCIBIOS_SUCCESSFUL) {
		printk("ahci: %02u:%02u.%u: Can not read IRQ", bus, dev, fun);
		return;
	}

	printk("AHCI SATA %02u:%02u.%u BAR 0x%x IRQ %u\n", bus, dev, fun, bar, irq);

	/* Map mmio */
	ahci_host = vremap(bar, 0x2000);

	/* Request IRQ */
	if (request_irq(irq, &ahci_interrupt, SA_SHIRQ, "ahci", (void*) ahci_host)) {
		printk("ahci: %02u:%02u.%u: Can not get irq %u\n", bus, dev, fun, irq);
		return;
	}

	nports = (readl(&ahci_host->cap) & 0x1f) + 1;
	port_map = readl(&ahci_host->pi);

	for (n = 0, i = 0; i < AHCI_MAX_PORTS; i++)
		if (port_map & (1U << i))
			n++;

	if (nports != n) {
		printk("ahci: %02u:%02u.%u: Odd number of ports, assuming %d is correct\n", bus, dev, fun, nports);
		port_map = 0;
	}
	if (!port_map) {
		port_map = (1U << nports) - 1;
	}

	for (i = 0; i < AHCI_MAX_PORTS; i++) {
		u32 ssts;

		if (!(port_map & (1U << i)))
			continue;

		ahci_port = &ahci_host->ports[i];

		ssts = readl(&ahci_port->ssts);
		if ((ssts & 0xf) != 0x3)
			/* Device not present */
			continue;
		if (((ssts >> 8) & 0xf) != 0x1)
			/* Device down */
			continue;

		/* OK! Probe this port */
		ahci_probe_port(ahci_host, ahci_port);
	}
}

/* genhd callback to set size of disks */
static void ahci_geninit(struct gendisk *gd)
{
	unsigned unit;
	struct port *port;

	for (unit = 0; unit < gd->nr_real; unit++) {
		port = &ports[unit];
		port->part[0].nr_sects = port->capacity;
		if (!port->part[0].nr_sects)
			port->part[0].nr_sects = -1;
	}
}

/* Probe all AHCI PCI devices */
void ahci_probe_pci(void)
{
	unsigned char bus, device;
	unsigned short index;
	int ret;
	unsigned nports, unit, nminors;
	struct port *port;
	struct gendisk *gd, **gdp;
	int *bs;

	for (index = 0;
		(ret = pcibios_find_class(PCI_CLASS_STORAGE_SATA_AHCI, index, &bus, &device)) == PCIBIOS_SUCCESSFUL;
		index++)
	{
		/* Note: this prevents from also having a SCSI controler.
		 * It shouldn't harm too much until we have proper hardware
		 * enumeration.
		 */
		if (register_blkdev(MAJOR_NR, "sd", &ahci_fops) < 0)
			printk("could not register ahci\n");
		ahci_probe_dev(bus, device);
	}

	for (nports = 0, port = &ports[0]; port < &ports[MAX_PORTS]; port++)
		if (port->ahci_port)
			nports++;

	nminors = nports * (1<<PARTN_BITS);

	gd              = kmalloc(sizeof(*gd), GFP_KERNEL);
	gd->sizes       = kmalloc(nminors * sizeof(*gd->sizes), GFP_KERNEL);
	gd->part        = kmalloc(nminors * sizeof(*gd->part), GFP_KERNEL);
	bs              = kmalloc(nminors * sizeof(*bs), GFP_KERNEL);

	blksize_size[MAJOR_NR] = bs;
	for (unit = 0; unit < nminors; unit++)
		/* We prefer to transfer whole pages */
		*bs++ = PAGE_SIZE;

	memset(gd->part, 0, nminors * sizeof(*gd->part));

	for (unit = 0; unit < nports; unit++) {
		ports[unit].gd = gd;
		ports[unit].part = &gd->part[unit << PARTN_BITS];
	}

	gd->major       = MAJOR_NR;
	gd->major_name  = "sd";
	gd->minor_shift = PARTN_BITS;
	gd->max_p       = 1<<PARTN_BITS;
	gd->max_nr      = nports;
	gd->nr_real     = nports;
	gd->init        = ahci_geninit;
	gd->next        = NULL;

	for (gdp = &gendisk_head; *gdp; gdp = &((*gdp)->next))
		;
	*gdp = gd;

	blk_dev[MAJOR_NR].request_fn = ahci_do_request;
}
