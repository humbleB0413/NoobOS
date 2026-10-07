#include "kernel.h"

int kernel_main(unsigned long magic, unsigned long addr){

    terminal_initialize();
    init_gdt();
    init_idt();
    init_trap();
    init_irq();
    pic_remap(0x20, 0x28);
    __asm__ volatile ("sti");
    init_timer();
    init_keyboard();
    init_pmm(addr);
    init_vmm();
    init_scheduling();

    char keyboard_input = '\0';
    allocated_frame_info_t alloc_info = {0};
    while(1){
        keyboard_input = keyboard_get_key();

        if(keyboard_input == 'a'){
            terminal_initialize();
            print_frame_state();
        }
        else if(keyboard_input == 's'){
            terminal_initialize();
            uint32_t result = alloc_frame(1, &alloc_info);
            if (result == 0xFFFFFFFF) {
                kprintf("Allocation failed\n");
            } else {
                kprintf("Allocated: frame %d (addr 0x%x)\n", alloc_info.frames[0], alloc_info.frames[0] * PAGE_SIZE);
            }
            kprintf("free frames remaining: %u\n", pmm_free_count());
        }
        else if(keyboard_input == 'd'){
            terminal_initialize();
            free_frame(&alloc_info);
            kprintf("Freed: frame %d\n", alloc_info.frames[0]);
            kprintf("free frames remaining: %u\n", pmm_free_count());
        }
#ifdef DEBUG
        else if(keyboard_input == 'k'){
            terminal_initialize();
            kmalloc_selftest();
        }
#endif
    }

    return 0;
}