#include <linux/linkage.h>

/*
 * BogoMips loop. Non-inlined because various x86's have so wildly
 * varying results depending on the exact alignment.
 */

ENTRY(__do_delay)
	jmp 1f
.align 16
1:	jmp 2f
.align 16
2:	decl %eax
	jns 2b
	ret



