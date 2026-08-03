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
    
    char keyboard_input = '\0';
    uint32_t frame_no = 0;
    while(1){
        keyboard_input = keyboard_get_key();
        
        if(keyboard_input == 'a'){
            terminal_initialize();
            print_frame_state();
        }
        else if(keyboard_input == 's'){
            terminal_initialize();
            frame_no = alloc_frame();
            kprintf("Allocated: frame %d (addr 0x%x)\n", frame_no, frame_no * PAGE_SIZE);
            kprintf("free frames remaining: %u\n", pmm_free_count());
        }
        else if(keyboard_input == 'd'){
            terminal_initialize();
            free_frame(frame_no);
            kprintf("Freed: frame %d\n", frame_no);
            kprintf("free frames remaining: %u\n", pmm_free_count());
        }
    }

    return 0;
}