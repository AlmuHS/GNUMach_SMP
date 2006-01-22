/* cb_shim.c: Linux CardBus device support code. */
/*
	Written 1999-2002 by Donald Becker.

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by
	reference.  This is not a documented interface.  Drivers incorporating
	or interacting with these functions are derivative works and thus
	are covered the GPL.  They must include an explicit GPL notice.

	This code provides a shim to allow newer drivers to interact with the
	older Cardbus driver activation code.  The functions supported are
	attach, suspend, power-off, resume and eject.

	The author may be reached as becker@scyld.com, or
	Donald Becker
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Support and updates available at
	http://www.scyld.com/network/drivers.html

	Other contributers:  (none yet)
*/

static const char version1[] =
"cb_shim.c:v1.03 7/12/2002  Donald Becker <becker@scyld.com>\n";
static const char version2[] =
" http://www.scyld.com/linux/drivers.html\n";

/* Module options. */
static int debug = 1;			/* 1 normal messages, 0 quiet .. 7 verbose. */

#ifndef __KERNEL__
#define __KERNEL__
#endif
#include <linux/config.h>
#if defined(CONFIG_SMP) && ! defined(__SMP__)
#define __SMP__
#endif
#if defined(CONFIG_MODVERSIONS) && ! defined(MODVERSIONS)
#define MODVERSIONS
#endif

#include <linux/version.h>
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#include <linux/module.h>

#include <linux/kernel.h>
#if LINUX_VERSION_CODE >= 0x20400
#include <linux/slab.h>
#else
#include <linux/malloc.h>
#endif
#include <linux/netdevice.h>
#include <linux/pci.h>
#include <asm/io.h>

/* These might be awkward to locate. */
#include <pcmcia/driver_ops.h>
#include "pci-scan.h"
#include "kern_compat.h"

MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("Hot-swap-PCI and Cardbus event dispatch");
MODULE_LICENSE("GPL");
MODULE_PARM(debug, "i");
MODULE_PARM_DESC(debug, "Enable additional status messages (0-7)");

/* Note: this is used in a slightly sleazy manner: it is passed to routines
   that expect and return just dev_node_t.  However using the too-simple
   dev_node_t complicates devices management -- older drivers had to
   look up dev_node_t.name in their private list. */

struct registered_pci_device {
	struct dev_node_t node;
	int magic;
	struct registered_pci_device *next;
	struct drv_id_info *drv_info;
	struct pci_dev *pci_loc;
	void *dev_instance;
} static *root_pci_devs = 0;

struct drv_shim {
	struct drv_id_info *did;
	struct driver_operations drv_ops;
	int magic;
	struct drv_shim *next;
} static *root_drv_id = 0;

static void drv_power_op(struct dev_node_t *node, enum drv_pwr_action action)
{
	struct registered_pci_device **devp, **next, *rpin = (void *)node, *rp;
	if (debug > 1)
		printk(KERN_DEBUG "power operation(%s, %d).\n",
			   rpin->drv_info->name, action);
	/* With our wrapper structure we can almost do
	   rpin->drv_info->pwr_event(rpin->dev_instance, action);
	   But the detach operation requires us to remove the object from the
	   list, so we check for uncontrolled "ghost" devices. */
	for (devp = &root_pci_devs; *devp; devp = next) {
		rp = *devp;
		next = &rp->next;
		if (rp == rpin) {
			if (rp->drv_info->pwr_event)
				rp->drv_info->pwr_event((*devp)->dev_instance, action);
			else
				printk(KERN_ERR "No power event hander for driver %s.\n",
					   rpin->drv_info->name);
			if (action == DRV_DETACH) {
				kfree(rp);
				*devp = *next;
				MOD_DEC_USE_COUNT;
			}
			return;
		}
	}
	if (debug)
		printk(KERN_WARNING "power operation(%s, %d) for a ghost device.\n",
			   node->dev_name, action);
}
/* Wrappers / static lambdas. */
static void drv_suspend(struct dev_node_t *node)
{
	drv_power_op(node, DRV_SUSPEND);
}
static void drv_resume(struct dev_node_t *node)
{
	drv_power_op(node, DRV_RESUME);
}
static void drv_detach(struct dev_node_t *node)
{
	drv_power_op(node, DRV_DETACH);
}

/* The CardBus interaction does not identify the driver the attach() is
   for, thus we must search for the ID in all PCI device tables.
   While ugly, we likely only have one driver loaded anyway.
*/
static dev_node_t *drv_attach(struct dev_locator_t *loc)
{
	struct drv_shim *dp;
	struct drv_id_info *drv_id = NULL;
	struct pci_id_info *pci_tbl = NULL;
	u32 pci_id, subsys_id, pci_rev, pciaddr;
	u8 irq;
	int chip_idx = 0, pci_flags, bus, devfn;
	long ioaddr;
	void *newdev;

	if (debug > 1)
		printk(KERN_INFO "drv_attach()\n");
	if (loc->bus != LOC_PCI) return NULL;
	bus = loc->b.pci.bus; devfn = loc->b.pci.devfn;
	if (debug > 1)
		printk(KERN_DEBUG "drv_attach(bus %d, function %d)\n", bus, devfn);

	pcibios_read_config_dword(bus, devfn, PCI_VENDOR_ID, &pci_id);
	pcibios_read_config_dword(bus, devfn, PCI_SUBSYSTEM_ID, &subsys_id);
	pcibios_read_config_dword(bus, devfn, PCI_REVISION_ID, &pci_rev);
	pcibios_read_config_byte(bus, devfn, PCI_INTERRUPT_LINE, &irq);
	for (dp = root_drv_id; dp; dp = dp->next) {
		drv_id = dp->did;
		pci_tbl = drv_id->pci_dev_tbl;
		for (chip_idx = 0; pci_tbl[chip_idx].name; chip_idx++) {
			struct pci_id_info *chip = &pci_tbl[chip_idx];
			if ((pci_id & chip->id.pci_mask) == chip->id.pci
				&& (subsys_id & chip->id.subsystem_mask) == chip->id.subsystem
				&& (pci_rev & chip->id.revision_mask) == chip->id.revision)
				break;
		}
		if (pci_tbl[chip_idx].name) 		/* Compiled out! */
			break;
	}
	if (dp == 0) {
		printk(KERN_WARNING "No driver match for device %8.8x at %d/%d.\n",
			   pci_id, bus, devfn);
		return 0;
	}
	pci_flags = pci_tbl[chip_idx].pci_flags;
	pcibios_read_config_dword(bus, devfn, ((pci_flags >> 2) & 0x1C) + 0x10,
							  &pciaddr);
	if ((pciaddr & PCI_BASE_ADDRESS_SPACE_IO)) {
		ioaddr = pciaddr & PCI_BASE_ADDRESS_IO_MASK;
	} else
		ioaddr = (long)ioremap(pciaddr & PCI_BASE_ADDRESS_MEM_MASK,
							   pci_tbl[chip_idx].io_size);
	if (ioaddr == 0 || irq == 0) {
		printk(KERN_ERR "The %s at %d/%d was not assigned an %s.\n"
			   KERN_ERR "  It will not be activated.\n",
			   pci_tbl[chip_idx].name, bus, devfn,
			   ioaddr == 0 ? "address" : "IRQ");
		return NULL;
	}
	printk(KERN_INFO "Found a %s at %d/%d address 0x%x->0x%lx IRQ %d.\n",
		   pci_tbl[chip_idx].name, bus, devfn, pciaddr, ioaddr, irq);
	{
		u16 pci_command;
		pcibios_read_config_word(bus, devfn, PCI_COMMAND, &pci_command);
		printk(KERN_INFO "%s at %d/%d command 0x%x.\n",
		   pci_tbl[chip_idx].name, bus, devfn, pci_command);
	}

	newdev = drv_id->probe1(pci_find_slot(bus, devfn), 0,
							ioaddr, irq, chip_idx, 0);
	if (newdev) {
		struct registered_pci_device *hsdev =
			kmalloc(sizeof(struct registered_pci_device), GFP_KERNEL);
		if (drv_id->pci_class == PCI_CLASS_NETWORK_ETHERNET<<8)
			strcpy(hsdev->node.dev_name, ((struct net_device *)newdev)->name);
		hsdev->node.major = hsdev->node.minor = 0;
		hsdev->node.next = NULL;
		hsdev->drv_info = drv_id;
		hsdev->dev_instance = newdev;
		hsdev->next = root_pci_devs;
		root_pci_devs = hsdev;
		drv_id->pwr_event(newdev, DRV_ATTACH);
		MOD_INC_USE_COUNT;
		return &hsdev->node;
	}
	return NULL;
}

/* Add/remove a driver ID structure to our private list of known drivers. */
int do_cb_register(struct drv_id_info *did)
{
	struct driver_operations *dop;
	struct drv_shim *dshim = kmalloc(sizeof(*dshim), GFP_KERNEL);
	if (dshim == 0)
		return 0;
	if (debug > 1)
		printk(KERN_INFO "Registering driver support for '%s'.\n",
			   did->name);
	MOD_INC_USE_COUNT;
	dshim->did = did;
	dop = &dshim->drv_ops;
	dop->name = (char *)did->name;
	dop->attach = drv_attach;
	dop->suspend = drv_suspend;
	dop->resume = drv_resume;
	dop->detach = drv_detach;
	dshim->next = root_drv_id;
	root_drv_id = dshim;
	return register_driver(dop);
}

void do_cb_unregister(struct drv_id_info *did)
{
	struct drv_shim **dp;
	for (dp = &root_drv_id; *dp; dp = &(*dp)->next)
		if ((*dp)->did == did) {
			struct drv_shim *dshim = *dp;
			unregister_driver(&dshim->drv_ops);
			*dp = dshim->next;
			kfree(dshim);
			MOD_DEC_USE_COUNT;
			return;
		}
}

extern int (*register_hotswap_hook)(struct drv_id_info *did);
extern void (*unregister_hotswap_hook)(struct drv_id_info *did);

int (*old_cb_hook)(struct drv_id_info *did);
void (*old_un_cb_hook)(struct drv_id_info *did);

int init_module(void)
{
	if (debug)
		printk(KERN_INFO "%s" KERN_INFO "%s", version1, version2);
	old_cb_hook = register_hotswap_hook;
	old_un_cb_hook = unregister_hotswap_hook;
	register_hotswap_hook = do_cb_register;
	unregister_hotswap_hook = do_cb_unregister;
	return 0;
}
void cleanup_module(void)
{
	register_hotswap_hook = 	old_cb_hook;
	unregister_hotswap_hook = old_un_cb_hook;
	return;
}


/*
 * Local variables:
 *  compile-command: "gcc -DMODULE -Wall -Wstrict-prototypes -O6 -c cb_shim.c -I/usr/include/ -I/usr/src/pcmcia/include/"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */

