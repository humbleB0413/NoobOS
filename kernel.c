#include "kernel.h"

int kernel_main(unsigned long magic, unsigned long addr){

    terminal_initialize();
    init_serial();
    init_gdt();
    init_idt();
    init_trap();
    init_irq();
    pic_remap(0x20, 0x28);
    __asm__ volatile ("sti");
    init_timer();
    init_keyboard();
    KASSERT(magic == MULTIBOOT_BOOTLOADER_MAGIC);
    init_pmm(addr);
    init_vmm();
    init_double_fault_task();
    init_syscall();
    init_scheduling();
    init_initrd(addr);
    kprintf("NoobOS: boot complete\n");

    /* 셸을 별도 커널 프로세스로 띄우고, 셸이 끝나면(exit) 새로 띄운다 */
    while(1){
        process_t* shell = create_process((uint32_t)shell_main, "shell");
        KASSERT(shell != 0);
        process_wait(shell->pid);
        kprintf("shell exited, restarting\n");
    }

    return 0;
}