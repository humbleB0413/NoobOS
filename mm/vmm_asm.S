.global init_paging_asm
init_paging_asm:
    mov 4(%esp), %eax
    mov %eax, %cr3
    mov %cr0, %eax
    orl $0x80000001, %eax
    mov %eax, %cr0
    ret

.include "kernel/isr_macro.S"

.global asm_isr_page_fault
asm_isr_page_fault:
    push $0
    push $32
    push_regs
    mov %esp, %eax
    push %eax
    call isr_page_fault
    add $4, %esp
    pop_regs
    add $8, %esp
    iret
