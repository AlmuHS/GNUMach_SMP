/* 
 * Copyright (c) 1995-1994 The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 *      Author: Bryan Ford, University of Utah CSL
 */
#ifndef _MACH_I386_EXEC_ELF_H_
#define _MACH_I386_EXEC_ELF_H_

typedef unsigned int	Elf32_Addr;
typedef unsigned short	Elf32_Half;
typedef unsigned int	Elf32_Off;
typedef signed int	Elf32_Sword;
typedef unsigned int	Elf32_Word;

typedef uint64_t	Elf64_Addr;
typedef uint64_t	Elf64_Off;
typedef int32_t		Elf64_Shalf;
typedef int32_t		Elf64_Sword;
typedef uint32_t	Elf64_Word;
typedef int64_t		Elf64_Sxword;
typedef uint64_t	Elf64_Xword;
typedef uint16_t	Elf64_Half;


/* Architecture identification parameters for x86.  */
#if defined(__x86_64__) && ! defined(USER32)
#define MY_ELF_CLASS	ELFCLASS64
#define MY_EI_DATA	ELFDATA2LSB
#define MY_E_MACHINE	EM_X86_64
#else
#define MY_ELF_CLASS	ELFCLASS32
#define MY_EI_DATA	ELFDATA2LSB
#define MY_E_MACHINE	EM_386
#endif

#endif /* _MACH_I386_EXEC_ELF_H_ */
