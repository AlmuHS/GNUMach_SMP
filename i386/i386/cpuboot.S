#include <mach/machine/asm.h>
#include <i386/i386asm.h>

    .section .boot, "awx"

    .align 0x10

    .text
    .globl apstart, _apstart

apstart:
_apstart:
	jmp boot_start

	#define MULTIBOOT_MAGIC 0x1badb002
	#define MULTIBOOT_FLAGS 0x0
	#define MULTIBOOT_CHECKSUM -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)
	#define STACK_SIZE (4096 * 2)

ap_data:
    .long MULTIBOOT_MAGIC
    .long MULTIBOOT_FLAGS
    .long MULTIBOOT_CHECKSUM

boot_start:
    /* loading idt and gdt */
    lidt idt_descr
    lgdt gdt_descr

    ljmp $0x8, $1f
1:
    mov $0x10, %eax
    movw %ax, %ds
    movw %ax, %es

    movw %ax, %ss
    movl $(stack_bsp + STACK_SIZE), %esp


    /* first argument: multiboot */
    push %ebx

    cli
1:
    hlt
    jmp 1b

    .section .rodata

    .align 0x10

    .globl idt_descr
idt_descr:
    .word 0
    .long 0

    .align 0x10
gdt_descr:
    .word 0x8*5 - 1
    .long gdt

    .align 0x10
gdt:
    /* 0 */
    .quad 0
    /* boot CS = 0x8 */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte 0x9a
    .byte 0xcf
    .byte 0x00
    /* boot DS = 0x10 */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xcf
    .byte 0x00
    /* rm CS = 0x18 */
    .word 0xffff
    .word 0
    .byte 0
    .byte 0x9a
    .byte 0x8f
    .byte 0
    /* rm DS = 0x20 */
    .word 0xffff
    .word 0
    .byte 0
    .byte 0x92
    .byte 0x8f


    .bss
    .p2align 4
    .globl stack_bsp
    .comm stack_bsp, STACK_SIZE


#define M(addr) (addr - apboot + 0x7000)

    .text
    .globl apboot, apbootend, stack_ptr
    .align 0x10
    .code16
apboot:
_apboot:


    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x7000, %sp

    lgdt M(gdt_descr_tmp)

    mov %cr0, %eax
    or $0x1, %eax
    mov %eax, %cr0

    ljmp $0x8, $M(1f)

counter:
.long 0

1:
    .code32

    mov $0x10, %eax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x7000, %esp

    incw M(counter)

    lidtl idt_descr

    lgdt gdt_descr
    ljmpl $0x8, $1f

1:
    mov $0x10, %eax
    movw %ax, %ds
    movw %ax, %es



    movw %ax, %ss
    movl stack_ptr, %esp
    addl $STACK_SIZE, %esp

    pushl stack_ptr
    call cpu_ap_main


    cli
1:
    hlt
    jmp 1b

stack_ptr:
    .long 0x0

    .align 0x10
gdt_descr_tmp:
    .short 0x8*3 - 1
    .long M(gdt_tmp)

    .align 0x10

gdt_tmp:
    /* 0 */
    .quad 0
    /* boot CS = 0x8 */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte 0x9a
    .byte 0xcf
    .byte 0x00
    /* boot DS = 0x10 */
    .word 0xffff
    .word 0x0000
    .byte 0x00
    .byte 0x92
    .byte 0xcf
    .byte 0x00


_apbootend:
apbootend:

    .globl dummyf
dummyf:
ret
