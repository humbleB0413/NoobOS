.include "kernel/isr_macro.S"

.global asm_irq_keyboard
asm_irq_keyboard:
    push $0
    push $32
    push_regs
    mov %esp, %eax
    push %eax
    call irq_keyboard
    add $4, %esp
    pop_regs
    add $8, %esp
    iret
