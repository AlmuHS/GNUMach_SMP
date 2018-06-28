.macro CPU_NUMBER reg
 call cpu_number
 movl %eax, \reg
.endm
