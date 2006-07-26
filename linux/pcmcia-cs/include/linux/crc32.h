#ifndef _COMPAT_CRC32_H
#define _COMPAT_CRC32_H

#include <linux/version.h>

#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,4,18))

#include_next <linux/crc32.h>

#else

static inline u_int ether_crc(int length, u_char *data)
{
    static const u_int ethernet_polynomial = 0x04c11db7U;
    int crc = 0xffffffff;	/* Initial value. */

    while (--length >= 0) {
	u_char current_octet = *data++;
	int bit;
	for (bit = 0; bit < 8; bit++, current_octet >>= 1) {
	    crc = (crc << 1) ^
		((crc < 0) ^ (current_octet & 1) ? ethernet_polynomial : 0);
	}
    }
    return crc;
}

static inline unsigned ether_crc_le(int length, unsigned char *data)
{
    static unsigned const ethernet_polynomial_le = 0xedb88320U;
    unsigned int crc = 0xffffffff;	/* Initial value. */
    while(--length >= 0) {
	unsigned char current_octet = *data++;
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

#endif

#endif /* _COMPAT_CRC32_H */

