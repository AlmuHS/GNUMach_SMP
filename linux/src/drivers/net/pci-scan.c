/* pci-scan.c: Linux PCI network adapter support code. */
/*
	Originally written 1999-2003 by Donald Becker.

	This software may be used and distributed according to the terms
	of the GNU General Public License (GPL), incorporated herein by
	reference.  Drivers interacting with these functions are derivative
	works and thus also must be licensed under the GPL and include an explicit
	GPL notice.

	This code provides common scan and activate functions for PCI network
	interfaces.

	The author may be reached as becker@scyld.com, or
	Donald Becker
	Scyld Computing Corporation
	914 Bay Ridge Road, Suite 220
	Annapolis MD 21403

	Other contributers:
*/
static const char version[] =
"pci-scan.c:v1.12 7/30/2003  Donald Becker <becker@scyld.com>"
" http://www.scyld.com/linux/drivers.html\n";

/* A few user-configurable values that may be modified when a module. */

static int msg_level = 1;		/* 1 normal messages, 0 quiet .. 7 verbose. */
static int min_pci_latency = 32;

#if ! defined(__KERNEL__)
#define __KERNEL__ 1
#endif
#if !defined(__OPTIMIZE__)
#warning  You must compile this file with the correct options!
#warning  See the last lines of the source file.
#error You must compile this driver with the proper options, including "-O".
#endif

#if defined(MODULE) && ! defined(EXPORT_SYMTAB)
#define EXPORT_SYMTAB
#endif

#include <linux/config.h>
#if defined(CONFIG_SMP) && ! defined(__SMP__)
#define __SMP__
#endif
#if defined(MODULE) && defined(CONFIG_MODVERSIONS) && ! defined(MODVERSIONS)
#define MODVERSIONS
#endif

#include <linux/version.h>
#if defined(MODVERSIONS)
#include <linux/modversions.h>
#endif
#if LINUX_VERSION_CODE < 0x20500  &&  defined(MODVERSIONS)
/* Another interface semantics screw-up. */
#include <linux/module.h>
#include <linux/modversions.h>
#else
#include <linux/module.h>
#endif

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#if LINUX_VERSION_CODE >= 0x20300
/* Bogus change in the middle of a "stable" kernel series.
   Also, in 2.4.7+ slab must come before interrupt.h to avoid breakage. */
#include <linux/slab.h>
#else
#include <linux/malloc.h>
#endif
#include <asm/io.h>
#include "pci-scan.h"
#include "kern_compat.h"
#if defined(CONFIG_APM)  &&  LINUX_VERSION_CODE < 0x20400 
#include <linux/apm_bios.h>
#endif
#ifdef CONFIG_PM
/* New in 2.4 kernels, pointlessly incompatible with earlier APM. */
#include <linux/pm.h>
#endif

#if (LINUX_VERSION_CODE >= 0x20100) && defined(MODULE)
char kernel_version[] = UTS_RELEASE;
#endif
#if (LINUX_VERSION_CODE < 0x20100)
#define PCI_CAPABILITY_LIST	0x34	/* Offset of first capability list entry */
#define PCI_STATUS_CAP_LIST	0x10	/* Support Capability List */
#define PCI_CAP_ID_PM		0x01	/* Power Management */
#endif

int (*register_hotswap_hook)(struct drv_id_info *did);
void (*unregister_hotswap_hook)(struct drv_id_info *did);

#if LINUX_VERSION_CODE > 0x20118  &&  defined(MODULE)
MODULE_LICENSE("GPL");
MODULE_PARM(msg_level, "i");
MODULE_PARM(min_pci_latency, "i");
MODULE_PARM_DESC(msg_level, "Enable additional status messages (0-7)");
MODULE_PARM_DESC(min_pci_latency,
				 "Minimum value for the PCI Latency Timer settings");
#if defined(EXPORT_SYMTAB)
EXPORT_SYMBOL_NOVERS(pci_drv_register);
EXPORT_SYMBOL_NOVERS(pci_drv_unregister);
EXPORT_SYMBOL_NOVERS(acpi_wake);
EXPORT_SYMBOL_NOVERS(acpi_set_pwr_state);
EXPORT_SYMBOL_NOVERS(register_hotswap_hook);
EXPORT_SYMBOL_NOVERS(unregister_hotswap_hook);
#endif
#endif

/* List of registered drivers. */
static struct drv_id_info *drv_list;
/* List of detected PCI devices, for APM events. */
static struct dev_info {
	struct dev_info *next;
	void *dev;
	struct drv_id_info *drv_id;
	int flags;
} *dev_list;

/*
  This code is not intended to support every configuration.
  It is intended to minimize duplicated code by providing the functions
  needed in almost every PCI driver.

  The "no kitchen sink" policy:
  Additional features and code will be added to this module only if more
  than half of the drivers for common hardware would benefit from the feature.
*/

/*
  Ideally we would detect and number all cards of a type (e.g. network) in
  PCI slot order.
  But that does not work with hot-swap card, CardBus cards and added drivers.
  So instead we detect just the each chip table in slot order.

  This routine takes a PCI ID table, scans the PCI bus, and calls the
  associated attach/probe1 routine with the hardware already activated and
  single I/O or memory address already mapped.

  This routine will later be supplemented with CardBus and hot-swap PCI
  support using the same table.  Thus the pci_chip_tbl[] should not be
  marked as __initdata.
*/

#if LINUX_VERSION_CODE >= 0x20200
/* Grrrr.. complex abstaction layers with negative benefit. */
int pci_drv_register(struct drv_id_info *drv_id, void *initial_device)
{
	int chip_idx, cards_found = 0;
	struct pci_dev *pdev = NULL;
	struct pci_id_info *pci_tbl = drv_id->pci_dev_tbl;
	struct drv_id_info *drv;
	void *newdev;


	/* Ignore a double-register attempt. */
	for (drv = drv_list; drv; drv = drv->next)
		if (drv == drv_id)
			return -EBUSY;

	while ((pdev = pci_find_class(drv_id->pci_class, pdev)) != 0) {
		u32 pci_id, pci_subsys_id, pci_class_rev;
		u16 pci_command, new_command;
		int pci_flags;
		long pciaddr;			/* Bus address. */
		long ioaddr;			/* Mapped address for this processor. */

		pci_read_config_dword(pdev, PCI_VENDOR_ID, &pci_id);
		/* Offset 0x2c is PCI_SUBSYSTEM_ID aka PCI_SUBSYSTEM_VENDOR_ID. */
		pci_read_config_dword(pdev, 0x2c, &pci_subsys_id);
		pci_read_config_dword(pdev, PCI_REVISION_ID, &pci_class_rev);

		if (msg_level > 3)
			printk(KERN_DEBUG "PCI ID %8.8x subsystem ID is %8.8x.\n",
				   pci_id, pci_subsys_id);
		for (chip_idx = 0; pci_tbl[chip_idx].name; chip_idx++) {
			struct pci_id_info *chip = &pci_tbl[chip_idx];
			if ((pci_id & chip->id.pci_mask) == chip->id.pci
				&& (pci_subsys_id&chip->id.subsystem_mask) == chip->id.subsystem
				&& (pci_class_rev&chip->id.revision_mask) == chip->id.revision)
				break;
		}
		if (pci_tbl[chip_idx].name == 0) 		/* Compiled out! */
			continue;

		pci_flags = pci_tbl[chip_idx].pci_flags;
#if LINUX_VERSION_CODE >= 0x2030C
		/* Wow. A oversized, hard-to-use abstraction. Bogus. */
		pciaddr = pdev->resource[(pci_flags >> 4) & 7].start;
#else
		pciaddr = pdev->base_address[(pci_flags >> 4) & 7];
#if defined(__alpha__)			/* Really any machine with 64 bit addressing. */
		if (pci_flags & PCI_ADDR_64BITS)
			pciaddr |= ((long)pdev->base_address[((pci_flags>>4)&7)+ 1]) << 32;
#endif
#endif
		if (msg_level > 2)
			printk(KERN_INFO "Found %s at PCI address %#lx, mapped IRQ %d.\n",
				   pci_tbl[chip_idx].name, pciaddr, pdev->irq);

		if ( ! (pci_flags & PCI_UNUSED_IRQ)  &&
			 (pdev->irq == 0 || pdev->irq == 255)) {
			if (pdev->bus->number == 32) 	/* Broken CardBus activation. */
				printk(KERN_WARNING "Resources for CardBus device '%s' have"
					   " not been allocated.\n"
					   KERN_WARNING "Activation has been delayed.\n",
					   pci_tbl[chip_idx].name);
			else
				printk(KERN_WARNING "PCI device '%s' was not assigned an "
					   "IRQ.\n"
					   KERN_WARNING "It will not be activated.\n",
				   pci_tbl[chip_idx].name);
			continue;
		}
		if ((pci_flags & PCI_BASE_ADDRESS_SPACE_IO)) {
			ioaddr = pciaddr & PCI_BASE_ADDRESS_IO_MASK;
			if (check_region(ioaddr, pci_tbl[chip_idx].io_size))
				continue;
		} else if ((ioaddr = (long)ioremap(pciaddr & PCI_BASE_ADDRESS_MEM_MASK,
										   pci_tbl[chip_idx].io_size)) == 0) {
			printk(KERN_INFO "Failed to map PCI address %#lx for device "
				   "'%s'.\n", pciaddr, pci_tbl[chip_idx].name);
			continue;
		}
		if ( ! (pci_flags & PCI_NO_ACPI_WAKE))
			acpi_wake(pdev);
		pci_read_config_word(pdev, PCI_COMMAND, &pci_command);
		new_command = pci_command | (pci_flags & 7);
		if (pci_command != new_command) {
			printk(KERN_INFO "  The PCI BIOS has not enabled the"
				   " device at %d/%d!  Updating PCI command %4.4x->%4.4x.\n",
				   pdev->bus->number, pdev->devfn, pci_command, new_command);
			pci_write_config_word(pdev, PCI_COMMAND, new_command);
		}

		newdev = drv_id->probe1(pdev, initial_device,
								ioaddr, pdev->irq, chip_idx, cards_found);
		if (newdev == NULL)
			continue;
		initial_device = 0;
		cards_found++;
		if (pci_flags & PCI_COMMAND_MASTER) {
			pci_set_master(pdev);
			if ( ! (pci_flags & PCI_NO_MIN_LATENCY)) {
				u8 pci_latency;
				pci_read_config_byte(pdev, PCI_LATENCY_TIMER, &pci_latency);
				if (pci_latency < min_pci_latency) {
					printk(KERN_INFO "  PCI latency timer (CFLT) is "
						   "unreasonably low at %d.  Setting to %d clocks.\n",
						   pci_latency, min_pci_latency);
					pci_write_config_byte(pdev, PCI_LATENCY_TIMER,
										  min_pci_latency);
				}
			}
		}
		{
			struct dev_info *devp =
				kmalloc(sizeof(struct dev_info), GFP_KERNEL);
			if (devp == 0)
				continue;
			devp->next = dev_list;
			devp->dev = newdev;
			devp->drv_id = drv_id;
			dev_list = devp;
		}
	}

	if (((drv_id->flags & PCI_HOTSWAP)
		 && register_hotswap_hook && (*register_hotswap_hook)(drv_id) == 0)
		|| cards_found) {
		MOD_INC_USE_COUNT;
		drv_id->next = drv_list;
		drv_list = drv_id;
		return 0;
	} else
		return -ENODEV;
}
#else
int pci_drv_register(struct drv_id_info *drv_id, void *initial_device)
{
	int pci_index, cards_found = 0;
	unsigned char pci_bus, pci_device_fn;
	struct pci_dev *pdev;
	struct pci_id_info *pci_tbl = drv_id->pci_dev_tbl;
	void *newdev;

	if ( ! pcibios_present())
		return -ENODEV;

	for (pci_index = 0; pci_index < 0xff; pci_index++) {
		u32 pci_id, subsys_id, pci_class_rev;
		u16 pci_command, new_command;
		int chip_idx, irq, pci_flags;
		long pciaddr;
		long ioaddr;
		u32 pci_busaddr;
		u8 pci_irq_line;

		if (pcibios_find_class (drv_id->pci_class, pci_index,
								&pci_bus, &pci_device_fn)
			!= PCIBIOS_SUCCESSFUL)
			break;
		pcibios_read_config_dword(pci_bus, pci_device_fn,
								  PCI_VENDOR_ID, &pci_id);
		/* Offset 0x2c is PCI_SUBSYSTEM_ID aka PCI_SUBSYSTEM_VENDOR_ID. */
		pcibios_read_config_dword(pci_bus, pci_device_fn, 0x2c, &subsys_id);
		pcibios_read_config_dword(pci_bus, pci_device_fn,
								  PCI_REVISION_ID, &pci_class_rev);

		for (chip_idx = 0; pci_tbl[chip_idx].name; chip_idx++) {
			struct pci_id_info *chip = &pci_tbl[chip_idx];
			if ((pci_id & chip->id.pci_mask) == chip->id.pci
				&& (subsys_id & chip->id.subsystem_mask) == chip->id.subsystem
				&& (pci_class_rev&chip->id.revision_mask) == chip->id.revision)
				break;
		}
		if (pci_tbl[chip_idx].name == 0) 		/* Compiled out! */
			continue;

		pci_flags = pci_tbl[chip_idx].pci_flags;
		pdev = pci_find_slot(pci_bus, pci_device_fn);
		pcibios_read_config_byte(pci_bus, pci_device_fn,
								 PCI_INTERRUPT_LINE, &pci_irq_line);
		irq = pci_irq_line;
		pcibios_read_config_dword(pci_bus, pci_device_fn,
								  ((pci_flags >> 2) & 0x1C) + 0x10,
								  &pci_busaddr);
		pciaddr = pci_busaddr;
#if defined(__alpha__)
		if (pci_flags & PCI_ADDR_64BITS) {
			pcibios_read_config_dword(pci_bus, pci_device_fn,
									  ((pci_flags >> 2) & 0x1C) + 0x14,
									  &pci_busaddr);
			pciaddr |= ((long)pci_busaddr)<<32;
		}
#endif

		if (msg_level > 2)
			printk(KERN_INFO "Found %s at PCI address %#lx, IRQ %d.\n",
				   pci_tbl[chip_idx].name, pciaddr, irq);

		if ( ! (pci_flags & PCI_UNUSED_IRQ)  &&
			 (irq == 0 || irq >= 16)) {
			if (pci_bus == 32) 	/* Broken CardBus activation. */
				printk(KERN_WARNING "Resources for CardBus device '%s' have"
					   " not been allocated.\n"
					   KERN_WARNING "It will not be activated.\n",
					   pci_tbl[chip_idx].name);
			else
				printk(KERN_WARNING "PCI device '%s' was not assigned an "
					   "IRQ.\n"
					   KERN_WARNING "It will not be activated.\n",
				   pci_tbl[chip_idx].name);
			continue;
		}

		if ((pciaddr & PCI_BASE_ADDRESS_SPACE_IO)) {
			ioaddr = pciaddr & PCI_BASE_ADDRESS_IO_MASK;
			if (check_region(ioaddr, pci_tbl[chip_idx].io_size))
				continue;
		} else if ((ioaddr = (long)ioremap(pciaddr & PCI_BASE_ADDRESS_MEM_MASK,
										   pci_tbl[chip_idx].io_size)) == 0) {
			printk(KERN_INFO "Failed to map PCI address %#lx.\n",
				   pciaddr);
			continue;
		}

		if ( ! (pci_flags & PCI_NO_ACPI_WAKE))
			acpi_wake(pdev);
		pcibios_read_config_word(pci_bus, pci_device_fn,
								 PCI_COMMAND, &pci_command);
		new_command = pci_command | (pci_flags & 7);
		if (pci_command != new_command) {
			printk(KERN_INFO "  The PCI BIOS has not enabled the"
				   " device at %d/%d!  Updating PCI command %4.4x->%4.4x.\n",
				   pci_bus, pci_device_fn, pci_command, new_command);
			pcibios_write_config_word(pci_bus, pci_device_fn,
									  PCI_COMMAND, new_command);
		}

		newdev = drv_id->probe1(pdev, initial_device,
							   ioaddr, irq, chip_idx, cards_found);

		if (newdev  && (pci_flags & PCI_COMMAND_MASTER)  &&
			! (pci_flags & PCI_NO_MIN_LATENCY)) {
			u8 pci_latency;
			pcibios_read_config_byte(pci_bus, pci_device_fn,
									 PCI_LATENCY_TIMER, &pci_latency);
			if (pci_latency < min_pci_latency) {
				printk(KERN_INFO "  PCI latency timer (CFLT) is "
					   "unreasonably low at %d.  Setting to %d clocks.\n",
					   pci_latency, min_pci_latency);
				pcibios_write_config_byte(pci_bus, pci_device_fn,
										  PCI_LATENCY_TIMER, min_pci_latency);
			}
		}
		if (newdev) {
			struct dev_info *devp =
				kmalloc(sizeof(struct dev_info), GFP_KERNEL);
			if (devp) {
				devp->next = dev_list;
				devp->dev = newdev;
				devp->drv_id = drv_id;
				dev_list = devp;
			}
		}
		initial_device = 0;
		cards_found++;
	}

	if (((drv_id->flags & PCI_HOTSWAP)
		 && register_hotswap_hook && (*register_hotswap_hook)(drv_id) == 0)
		|| cards_found) {
		MOD_INC_USE_COUNT;
		drv_id->next = drv_list;
		drv_list = drv_id;
		return 0;
	} else
		return cards_found ? 0 : -ENODEV;
}
#endif

void pci_drv_unregister(struct drv_id_info *drv_id)
{
	struct drv_id_info **drvp;
	struct dev_info **devip = &dev_list;

	if (unregister_hotswap_hook)
		(*unregister_hotswap_hook)(drv_id);

	for (drvp = &drv_list; *drvp; drvp = &(*drvp)->next)
		if (*drvp == drv_id) {
			*drvp = (*drvp)->next;
			MOD_DEC_USE_COUNT;
			break;
		}
	while (*devip) {
		struct dev_info *thisdevi = *devip;
		if (thisdevi->drv_id == drv_id) {
			*devip = thisdevi->next;
			kfree(thisdevi);
		} else
			devip = &(*devip)->next;
	}

	return;
}

#if LINUX_VERSION_CODE < 0x20400
/*
  Search PCI configuration space for the specified capability registers.
  Return the index, or 0 on failure.
  The 2.4 kernel now includes this function.
*/
int pci_find_capability(struct pci_dev *pdev, int findtype)
{
	u16 pci_status, cap_type;
	u8 pci_cap_idx;
	int cap_idx;

	pci_read_config_word(pdev, PCI_STATUS, &pci_status);
	if ( ! (pci_status & PCI_STATUS_CAP_LIST))
		return 0;
	pci_read_config_byte(pdev, PCI_CAPABILITY_LIST, &pci_cap_idx);
	cap_idx = pci_cap_idx;
	for (cap_idx = pci_cap_idx; cap_idx; cap_idx = (cap_type >> 8) & 0xff) {
		pci_read_config_word(pdev, cap_idx, &cap_type);
		if ((cap_type & 0xff) == findtype)
			return cap_idx;
	}
	return 0;
}
#endif

/* Change a device from D3 (sleep) to D0 (active).
   Return the old power state.
   This is more complicated than you might first expect since most cards
   forget all PCI config info during the transition! */
int acpi_wake(struct pci_dev *pdev)
{
	u32 base[5], romaddr;
	u16 pci_command, pwr_command;
	u8  pci_latency, pci_cacheline, irq;
	int i, pwr_cmd_idx = pci_find_capability(pdev, PCI_CAP_ID_PM);

	if (pwr_cmd_idx == 0)
		return 0;
	pci_read_config_word(pdev, pwr_cmd_idx + 4, &pwr_command);
	if ((pwr_command & 3) == 0)
		return 0;
	pci_read_config_word(pdev, PCI_COMMAND, &pci_command);
	for (i = 0; i < 5; i++)
		pci_read_config_dword(pdev, PCI_BASE_ADDRESS_0 + i*4,
								  &base[i]);
	pci_read_config_dword(pdev, PCI_ROM_ADDRESS, &romaddr);
	pci_read_config_byte( pdev, PCI_LATENCY_TIMER, &pci_latency);
	pci_read_config_byte( pdev, PCI_CACHE_LINE_SIZE, &pci_cacheline);
	pci_read_config_byte( pdev, PCI_INTERRUPT_LINE, &irq);

	pci_write_config_word(pdev, pwr_cmd_idx + 4, 0x0000);
	for (i = 0; i < 5; i++)
		if (base[i])
			pci_write_config_dword(pdev, PCI_BASE_ADDRESS_0 + i*4,
									   base[i]);
	pci_write_config_dword(pdev, PCI_ROM_ADDRESS, romaddr);
	pci_write_config_byte( pdev, PCI_INTERRUPT_LINE, irq);
	pci_write_config_byte( pdev, PCI_CACHE_LINE_SIZE, pci_cacheline);
	pci_write_config_byte( pdev, PCI_LATENCY_TIMER, pci_latency);
	pci_write_config_word( pdev, PCI_COMMAND, pci_command | 5);
	return pwr_command & 3;
}

int acpi_set_pwr_state(struct pci_dev *pdev, enum acpi_pwr_state new_state)
{
	u16 pwr_command;
	int pwr_cmd_idx = pci_find_capability(pdev, PCI_CAP_ID_PM);

	if (pwr_cmd_idx == 0)
		return 0;
	pci_read_config_word(pdev, pwr_cmd_idx + 4, &pwr_command);
	if ((pwr_command & 3) == ACPI_D3  &&  new_state != ACPI_D3)
		acpi_wake(pdev);		/* The complicated sequence. */
	pci_write_config_word(pdev, pwr_cmd_idx + 4,
							  (pwr_command & ~3) | new_state);
	return pwr_command & 3;
}

#if defined(CONFIG_PM)
static int handle_pm_event(struct pm_dev *dev, int event, void *data)
{
	static int down = 0;
	struct dev_info *devi;
	int pwr_cmd = -1;

	if (msg_level > 1)
		printk(KERN_DEBUG "pci-scan: Handling power event %d for driver "
			   "list %s...\n",
			   event, drv_list->name);
	switch (event) {
	case PM_SUSPEND:
		if (down) {
			printk(KERN_DEBUG "pci-scan: Received extra suspend event\n");
			break;
		}
		down = 1;
		for (devi = dev_list; devi; devi = devi->next)
			if (devi->drv_id->pwr_event)
				devi->drv_id->pwr_event(devi->dev, DRV_SUSPEND);
		break;
	case PM_RESUME:
		if (!down) {
			printk(KERN_DEBUG "pci-scan: Received bogus resume event\n");
			break;
		}
		for (devi = dev_list; devi; devi = devi->next) {
			if (devi->drv_id->pwr_event) {
				if (msg_level > 3)
					printk(KERN_DEBUG "pci-scan: Calling resume for %s "
						   "device.\n", devi->drv_id->name);
				devi->drv_id->pwr_event(devi->dev, DRV_RESUME);
			}
		}
		down = 0;
		break;
	case PM_SET_WAKEUP: pwr_cmd = DRV_PWR_WakeOn; break;
	case PM_EJECT:		pwr_cmd = DRV_DETACH;	break;
	default:
		printk(KERN_DEBUG "pci-scan: Unknown power management event %d.\n",
			   event);
	}
	if (pwr_cmd >= 0)
		for (devi = dev_list; devi; devi = devi->next)
			if (devi->drv_id->pwr_event)
				devi->drv_id->pwr_event(devi->dev, pwr_cmd);

	return 0;
}

#elif defined(CONFIG_APM)  &&  LINUX_VERSION_CODE < 0x20400 
static int handle_apm_event(apm_event_t event)
{
	static int down = 0;
	struct dev_info *devi;

	if (msg_level > 1)
		printk(KERN_DEBUG "pci-scan: Handling APM event %d for driver "
			   "list %s...\n",
			   event, drv_list->name);
	return 0;
	switch (event) {
	case APM_SYS_SUSPEND:
	case APM_USER_SUSPEND:
		if (down) {
			printk(KERN_DEBUG "pci-scan: Received extra suspend event\n");
			break;
		}
		down = 1;
		for (devi = dev_list; devi; devi = devi->next)
			if (devi->drv_id->pwr_event)
				devi->drv_id->pwr_event(devi->dev, DRV_SUSPEND);
		break;
	case APM_NORMAL_RESUME:
	case APM_CRITICAL_RESUME:
		if (!down) {
			printk(KERN_DEBUG "pci-scan: Received bogus resume event\n");
			break;
		}
		for (devi = dev_list; devi; devi = devi->next)
			if (devi->drv_id->pwr_event)
				devi->drv_id->pwr_event(devi->dev, DRV_RESUME);
		down = 0;
		break;
	}
	return 0;
}
#endif /* CONFIG_APM */

#ifdef MODULE
int init_module(void)
{
	if (msg_level)	/* Emit version even if no cards detected. */
		printk(KERN_INFO "%s", version);

#if defined(CONFIG_PM)
	pm_register(PM_PCI_DEV, 0, &handle_pm_event);
#elif defined(CONFIG_APM)  &&  LINUX_VERSION_CODE < 0x20400 
	apm_register_callback(&handle_apm_event);
#endif
	return 0;
}
void cleanup_module(void)
{
#if defined(CONFIG_PM)
	pm_unregister_all(&handle_pm_event);
#elif defined(CONFIG_APM)  &&  LINUX_VERSION_CODE < 0x20400 
	apm_unregister_callback(&handle_apm_event);
#endif
	if (dev_list != NULL)
		printk(KERN_WARNING "pci-scan: Unfreed device references.\n");
	return;
}
#endif


/*
 * Local variables:
 *  compile-command: "gcc -DMODULE -D__KERNEL__ -DEXPORT_SYMTAB -Wall -Wstrict-prototypes -O6 -c pci-scan.c"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
