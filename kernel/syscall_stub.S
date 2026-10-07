.include "kernel/isr_macro.S"

/* int 0x80: CPU 는 에러 코드를 push 하지 않으므로 다른 IRQ 스텁처럼 0 을 채워 pt_regs 배치를 맞춘다 */
.global asm_syscall
asm_syscall:
    push $0
    push $0x80
    push_regs
    mov %esp, %eax
    push %eax
    call syscall_dispatch
    add $4, %esp
    pop_regs
    add $8, %esp
    iret
