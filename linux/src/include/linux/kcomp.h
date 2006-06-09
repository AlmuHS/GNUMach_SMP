/*
 * Kernel compatibility glue to allow USB compile on 2.2.x kernels
 */

#include <linux/list.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/pagemap.h>

#define __exit

#define pci_enable_device(x)			0

#define page_address(x)				(x | PAGE_OFFSET)

#define	TTY_DRIVER_NO_DEVFS			0

#define	net_device			device
#define dev_kfree_skb_irq(a)		dev_kfree_skb(a, FREE_WRITE)
#define netif_wake_queue(dev)		do { clear_bit(0, &dev->tbusy); mark_bh(NET_BH); } while(0)
#define netif_stop_queue(dev)		test_and_set_bit(0, &dev->tbusy)
#define netif_start_queue(dev)		do { dev->tbusy = 0; dev->interrupt = 0; dev->start = 1; } while (0)
#define netif_queue_stopped(dev)	dev->tbusy
#define netif_running(dev)		dev->start

/* hot-(un)plugging stuff */
static inline int netif_device_present(struct net_device *dev)
{
	return	test_bit(0, &dev->start);
}

static inline void netif_device_detach(struct net_device *dev)
{
	if ( test_and_clear_bit(0, &dev->start) )
		netif_stop_queue(dev);
}

static inline void netif_device_attach(struct net_device *dev)
{
	if ( !test_and_set_bit(0, &dev->start) )
		netif_wake_queue(dev);
}

#define NET_XMIT_SUCCESS	0
#define NET_XMIT_DROP		1
#define NET_XMIT_CN		2

#define IORESOURCE_IO			1
#define pci_resource_start(dev,bar) \
(((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_SPACE) ? \
 ((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_IO_MASK) : \
 ((dev)->base_address[(bar)] & PCI_BASE_ADDRESS_MEM_MASK))
#define pci_resource_flags(dev, i)	(dev->base_address[i] & IORESOURCE_IO)

