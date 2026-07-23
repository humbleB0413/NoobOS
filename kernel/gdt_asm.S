.global asm_load_gdt
asm_load_gdt:
    mov 4(%esp), %eax
    lgdt (%eax)
    movl %cr0, %eax
    orl $1, %eax
    movl %eax, %cr0
    ljmp $0x08, $enter_protected_mode

enter_protected_mode:
    movw   $0x10, %ax
    movw   %ax, %ds
    movw   %ax, %es
    movw   %ax, %fs
    movw   %ax, %gs
    movw   %ax, %ss
    movw   $0x28, %ax
    ltr    %ax
    ret