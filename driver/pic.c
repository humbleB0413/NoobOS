#include "pic.h"

void pic_remap(int offset_1, int offset_2){
    outb(PIC1_PORT_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2_PORT_CMD, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC1_PORT_DATA, offset_1);
    io_wait();
    outb(PIC2_PORT_DATA, offset_2);
    io_wait();
    outb(PIC1_PORT_DATA, 1 << CASCADE_IRQ);
    io_wait();
    outb(PIC2_PORT_DATA, CASCADE_IRQ);
    io_wait();

    outb(PIC1_PORT_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_PORT_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_PORT_DATA, 0);
	outb(PIC2_PORT_DATA, 0);

    __asm__ volatile ("sti"); // 안정성 보장
}

void pic_send_eoi(uint8_t irq){
    if(irq >= 8){
        outb(PIC2_PORT_CMD, PIC_EOI);
    }
    outb(PIC1_PORT_CMD, PIC_EOI);
}

void pic_disable(){
    outb(PIC1_PORT_DATA, 0xff);
    outb(PIC2_PORT_DATA, 0xff);
}

void irq_set_mask(uint8_t IRQline) {
    uint16_t port;
    uint8_t value;

    if(IRQline < 8) {
        port = PIC1_PORT_DATA;
    } else {
        port = PIC2_PORT_DATA;
        IRQline -= 8;
    }
    value = inb(port) | (1 << IRQline);
    outb(port, value);        
}

void irq_clear_mask(uint8_t IRQline) {
    uint16_t port;
    uint8_t value;

    if(IRQline < 8) {
        port = PIC1_PORT_DATA;
    } else {
        port = PIC2_PORT_DATA;
        IRQline -= 8;
    }
    value = inb(port) & ~(1 << IRQline);
    outb(port, value);        
}