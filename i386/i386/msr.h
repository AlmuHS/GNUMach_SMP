/*
 *  Copyright (C) 2023 Free Software Foundation
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

#ifndef _MACHINE_MSR_H_
#define _MACHINE_MSR_H_

#define MSR_REG_EFER  0xC0000080
#define MSR_REG_STAR  0xC0000081
#define MSR_REG_LSTAR 0xC0000082
#define MSR_REG_CSTAR 0xC0000083
#define MSR_REG_FMASK 0xC0000084
#define MSR_REG_FSBASE 0xC0000100
#define MSR_REG_GSBASE 0xC0000101

#define MSR_EFER_SCE  0x00000001

#ifndef __ASSEMBLER__

static inline void wrmsr(uint32_t regaddr, uint64_t value)
{
  uint32_t low = (uint32_t) value, high = ((uint32_t) (value >> 32));
  asm volatile("wrmsr"
               :
               : "c" (regaddr), "a" (low), "d" (high)
               : "memory"  /* wrmsr may cause a read from memory, so
                            * make the compiler flush any changes */
               );
}

static inline uint64_t rdmsr(uint32_t regaddr)
{
  uint32_t low, high;
  asm volatile("rdmsr"
               : "=a" (low), "=d" (high)
               : "c" (regaddr)
               );
  return ((uint64_t)high << 32) | low;
}
#endif /* __ASSEMBLER__ */

#endif /* _MACHINE_MSR_H_ */
