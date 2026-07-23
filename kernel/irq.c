#include "linux/irq.h"
#include "../driver/pic.h"

extern __attribute__((aligned(0x10))) idtr_t KERNEL_IDT[IDT_ENTRIES_SIZE];
extern void *irq_stub_table[IRQ_COUNT];

void init_irq(){
    for(int i = 0; i < IRQ_COUNT; i++){
        set_idt_descriptor(KERNEL_IDT + IRQ_BASE + i, irq_stub_table[i], 0x08, IDT_P | IDT_DPL_KERNEL | IDT_GATE_TYPE_32BIT_INTERRUPT);
    }
}

void irq_handler(pt_regs* regs){
    uint8_t irq = (uint8_t)(regs->int_no - IRQ_BASE);
    pic_send_eoi(irq);
}
