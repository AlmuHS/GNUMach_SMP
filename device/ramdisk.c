#include <mach/vm_param.h>
#include <machine/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/vm_user.h>
#include <device/device_types.h>
#include <device/ds_routines.h>
#include <device/conf.h>
#include <device/ramdisk.h>
#include <kern/printf.h>
#include <string.h>

static struct ramdisk {
	void *data;
	vm_size_t size;
} ramdisk[RAMDISK_MAX];

static int ramdisk_num = 0;

/* Initial ramdisks are created from the boot scripts */
int ramdisk_create(vm_size_t size, const void *initdata, int *out_no)
{
	struct ramdisk *rd = &ramdisk[ramdisk_num];
	int err;

	if(ramdisk_num >= RAMDISK_MAX)
		return -1;

	/* allocate the memory */
	rd->size = round_page(size);
	err = kmem_alloc(kernel_map, (vm_offset_t *) &rd->data, rd->size);
	if(err != KERN_SUCCESS)
		return err;

	/* initialize */
	if(initdata)
		memcpy(rd->data, initdata, rd->size);
	else
		memset(rd->data, 0, rd->size);

	/* report */
	if(out_no) *out_no = ramdisk_num;
	printf("%s%d: %lu bytes @%p\n", RAMDISK_NAME, ramdisk_num,
			(unsigned long) rd->size, rd->data);

	ramdisk_num++;
	return KERN_SUCCESS;
}

/* On d_open() we just check whether the ramdisk exists */
int ramdisk_open(dev_t dev, int mode, io_req_t ior)
{
	return (dev < ramdisk_num) ? D_SUCCESS : D_NO_SUCH_DEVICE;
}

/* d_getstat() is used to query the device characteristics */
int ramdisk_getstat(dev_t dev, dev_flavor_t flavor, dev_status_t status,
		mach_msg_type_number_t *status_count)
{
	switch(flavor) {
		case DEV_GET_SIZE:
			status[DEV_GET_SIZE_DEVICE_SIZE] = ramdisk[dev].size;
			status[DEV_GET_SIZE_RECORD_SIZE] = RAMDISK_BLOCKSZ;
			*status_count = DEV_GET_SIZE_COUNT;
			return D_SUCCESS;

		case DEV_GET_RECORDS:
			status[DEV_GET_RECORDS_DEVICE_RECORDS]
					= ramdisk[dev].size / RAMDISK_BLOCKSZ;
			status[DEV_GET_RECORDS_RECORD_SIZE] = RAMDISK_BLOCKSZ;
			*status_count = DEV_GET_RECORDS_COUNT;
			return D_SUCCESS;
	}
	return D_INVALID_OPERATION;
}

/* TODO: implement freeramdisk with setstat() ? */

/* Check the given io request and compute a pointer to the ramdisk data and the
 * amount to be handled. */
static int ramdisk_ioreq(int dev, io_req_t ior, void **data, int *amt)
{
	vm_offset_t ofs = ior->io_recnum * RAMDISK_BLOCKSZ;
	if(ofs >= ramdisk[dev].size)
		return D_INVALID_RECNUM;

	*data = (char*) ramdisk[dev].data + ofs;
	*amt = ior->io_count;
	if(ofs + *amt > ramdisk[dev].size)
		*amt = ramdisk[dev].size - ofs;

	return KERN_SUCCESS;
}

/* Copy data from a vm_map_copy by mapping it temporarily. */
static int mem_map_cpy(void *dst, vm_map_copy_t src, int amt)
{
	vm_offset_t srcaddr;
	int err;

	err = vm_map_copyout(device_io_map, &srcaddr, src);
	if (err != KERN_SUCCESS)
		return err;

	memcpy(dst, (void *) srcaddr, amt);
	vm_deallocate(device_io_map, srcaddr, amt);
	return KERN_SUCCESS;
}

int ramdisk_read(dev_t dev, io_req_t ior)
{
	void *data;
	int amt, err;

	err = ramdisk_ioreq(dev, ior, &data, &amt);
	if(err != KERN_SUCCESS)
		return err;

	err = device_read_alloc (ior, ior->io_count);
	if (err != KERN_SUCCESS)
		return err;

	memcpy(ior->io_data, data, amt);
	ior->io_residual = ior->io_count - amt;

	return D_SUCCESS;
}

int ramdisk_write(dev_t dev, io_req_t ior)
{
	void *data;
	int amt, err;

	err = ramdisk_ioreq(dev, ior, &data, &amt);
	if(err != KERN_SUCCESS)
		return err;

	if (!(ior->io_op & IO_INBAND)) {
		/* Out-of-band data is transmitted as a vm_map_copy */
		err = mem_map_cpy(data, (vm_map_copy_t) ior->io_data, amt);
		if(err != KERN_SUCCESS)
			return err;
	} else {
		/* In-band data can be accessed directly */
		memcpy(data, ior->io_data, amt);
	}

	ior->io_residual = ior->io_count - amt;
	return D_SUCCESS;
}

vm_offset_t ramdisk_mmap(dev_t dev, vm_offset_t off, vm_prot_t prot)
{
	if(dev >= ramdisk_num)
		return -1;
	if(off >= ramdisk[dev].size)
		return -1;

	return pmap_phys_to_frame(kvtophys((vm_offset_t) ramdisk[dev].data + off));
}

