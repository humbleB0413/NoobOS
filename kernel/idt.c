#include "linux/idt.h"
#include "string.h"

__attribute__((aligned(0x10))) 
idtr_t KERNEL_IDT[IDT_ENTRIES_SIZE];
void (*KERNEL_ISR[IDT_ENTRIES_SIZE])(pt_regs*);

void init_idt(){
    memset(KERNEL_IDT, 0, sizeof(KERNEL_IDT));

    __asm__ volatile ("cli");// 안정성 보장

    for(int i =0; i < IDT_ENTRIES_SIZE; i++){
        set_idt_descriptor(KERNEL_IDT + i, 0, 0x08, 0x00); // 비활성화된 더미 인터럽트
    }

    idt_descriptor_t idt_desc;
    idt_desc.limit = sizeof(KERNEL_IDT) - 1;
    idt_desc.address = (uint32_t)KERNEL_IDT;
    
    __asm__ volatile ("lidt %0": :"m"(idt_desc));

    return;
}

void set_idt_descriptor(idtr_t* idt, void* isr, uint16_t kernel_cs, uint8_t attributes){
    idt->isr_low = (uint32_t)isr & 0xFFFF;
    idt->kernel_cs = kernel_cs;
    idt->attributes = attributes;
    idt->isr_high = (uint32_t)isr >> 16;
}