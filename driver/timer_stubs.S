.include "kernel/isr_macro.S"

.global irq_system_timer
irq_system_timer:
    push $0
    push $32
    push_regs
    mov %esp, %eax
    push %eax
    call irq_pic_timer
    add $4, %esp
    pop_regs
    add $8, %esp
    iret
