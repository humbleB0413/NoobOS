.ifndef MACROS_S
.set MACROS_S, 1

.macro push_regs
    pushal
.endm

.macro pop_regs
    popal
.endm

.macro isr_err_stub num, handler
isr_stub_\num:
    push $\num
    push_regs
    mov %esp, %eax
    push %eax
    call \handler
    add $4, %esp
    pop_regs
    add $8, %esp
    iret
.endm

.macro isr_no_err_stub num, handler
isr_stub_\num:
    push $0
    push $\num
    push_regs
    mov %esp, %eax
    push %eax
    call \handler
    add $4, %esp
    pop_regs
    add $8, %esp
    iret
.endm

.endif