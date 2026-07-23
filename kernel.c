#include "linux/vga.h"
#include "linux/gdt.h"
#include "linux/idt.h"
#include "linux/trap.h"
#include "linux/irq.h"
#include "driver/pic.h"
#include "driver/timer.h"
#include "driver/keyboard.h"

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

    kprintf("MAGIC:%d\n", magic);
    kprintf("Hello World!\n");
    
    char keyboard_input = '\0';
    while(1){
        keyboard_input = keyboard_get_key();
        if(keyboard_input){
            kprintf("KEY: %c\n", keyboard_input);
        }
        //else{
        //    kprintf("TIMER: %d\n", get_ticks());
        //}
    
        //sleep(1);
    }

    return 0;
}