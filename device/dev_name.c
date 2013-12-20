/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date: 	8/89
 */

#include <kern/printf.h>
#include <string.h>
#include <device/device_types.h>
#include <device/dev_hdr.h>
#include <device/conf.h>



/*
 * Routines placed in empty entries in the device tables
 */
int nulldev(void)
{
	return (D_SUCCESS);
}

int nodev(void)
{
	return (D_INVALID_OPERATION);
}

vm_offset_t
nomap(void)
{
	return (D_INVALID_OPERATION);
}

/*
 * Name comparison routine.
 * Compares first 'len' characters of 'src'
 * with 'target', which is zero-terminated.
 * Returns TRUE if strings are equal:
 *   src and target are equal in first 'len' characters
 *   next character of target is 0 (end of string).
 */
boolean_t __attribute__ ((pure))
name_equal(src, len, target)
	const char 	*src;
	int		len;
	const char 	*target;
{
	while (--len >= 0)
	    if (*src++ != *target++)
		return FALSE;
	return *target == 0;
}

/*
 * device name lookup
 */
boolean_t dev_name_lookup(name, ops, unit)
	char 		*name;
	dev_ops_t	*ops;	/* out */
	int		*unit;	/* out */
{
	/*
	 * Assume that block device names are of the form
	 *
	 * <device_name><unit_number>[[<slice num>]<partition>]
	 *
	 * where
	 * <device_name>	is the name in the device table
	 * <unit_number>	is an integer
	 * <slice num>	*	is 's' followed by a number (disks only!)
	 * <partition>		is a letter in [a-h] (disks only?)
	 */

	char 		*cp = name;
	int		len;
	int		j = 0;
	int		c;
	dev_ops_t	dev;
	boolean_t 	found;

	int slice_num = 0;

	/*
	 * Find device type name (characters before digit)
	 */
	while ((c = *cp) != '\0' &&
		!(c >= '0' && c <= '9'))
	    cp++;

	len = cp - name;
	if (c != '\0') {
	    /*
	     * Find unit number
	     */
	    while ((c = *cp) != '\0' &&
		    c >= '0' && c <= '9') {
		j = j * 10 + (c - '0');
		cp++;
	    }
	}

	found = FALSE;
	dev_search(dev) {
	    if (name_equal(name, len, dev->d_name)) {
		found = TRUE;
		break;
	    }
	}
	if (!found) {
	    /* name not found - try indirection list */
	    dev_indirect_t	di;

	    dev_indirect_search(di) {
		if (name_equal(name, len, di->d_name)) {
		    /*
		     * Return device and unit from indirect vector.
		     */
		    *ops = di->d_ops;
		    *unit = di->d_unit;
		    return (TRUE);
		}
	    }
	    /* Not found in either list. */
	    return (FALSE);
	}

	*ops = dev;
	*unit = j;

	/*
	 * Find sub-device number
	 */

	j = dev->d_subdev;
	if (j > 0) {
	    /* if no slice string, slice num = 0 */

	    /* <subdev_count>*unit + <slice_number>*16 -- I know it's bad */
	    *unit *= j;

	    /* find slice ? */
	    if (c == 's') {
		cp++;
		while ((c = *cp) != '\0' &&
			c >= '0' && c <= '9') {
		    slice_num = slice_num * 10 + (c - '0');
		    cp++;
		}
	    }

	    *unit += (slice_num << 4);
		/* if slice==0, it is either compatability or whole device */

	    if (c >= 'a' && c < 'a' + j) { /* note: w/o this -> whole slice */
		/*
		 * Minor number is <subdev_count>*unit + letter.
		 * NOW it is slice result + letter
		 */
		*unit += (c - 'a' +1);
	    }
	}
	return (TRUE);
}

/*
 * Change an entry in the indirection list.
 */
void
dev_set_indirection(name, ops, unit)
	const char	*name;
	dev_ops_t	ops;
	int		unit;
{
	dev_indirect_t di;

	dev_indirect_search(di) {
	    if (!strcmp(di->d_name, name)) {
		di->d_ops = ops;
		di->d_unit = unit;
		break;
	    }
	}
}

boolean_t dev_change_indirect(iname, dname, unit)
	const char 	*iname;
	const char	*dname;
	int 		unit;
{
    struct dev_ops *dp;
    struct dev_indirect *di;
    boolean_t found = FALSE;

    dev_search(dp) {
	if (!strcmp(dp->d_name, dname)) {
	    found = TRUE;
	    break;
	}
    }
    if (!found) return FALSE;
    dev_indirect_search(di) {
	if (!strcmp(di->d_name, iname)) {
	    di->d_ops = dp;
	    di->d_unit = unit;
	    return TRUE;
	}
    }
    return FALSE;
}
