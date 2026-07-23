#pragma once

#include <stdint.h>

#define IDT_ENTRIES_SIZE            256

#define IDT_P                           0x80
#define IDT_NP                          0x80
#define IDT_DPL_KERNEL                  0x00
#define IDT_DPL_USER                    0x60
#define IDT_GATE_TYPE_TASK              0x05
#define IDT_GATE_TYPE_16BIT_INTERRUPT   0x06   
#define IDT_GATE_TYPE_16BIT_TRAP        0x07
#define IDT_GATE_TYPE_32BIT_INTERRUPT   0x0E
#define IDT_GATE_TYPE_32BIT_TRAP        0x0F

typedef struct __attribute__((packed)) {
  uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t     reserved;     // Set to zero
	uint8_t     attributes;   // Type and attributes; see the IDT page
	uint16_t    isr_high;     // The higher 16 bits of the ISR's address
} idtr_t;

typedef struct __attribute__((packed))
{
  uint16_t limit;
  uint32_t address;
} idt_descriptor_t;

typedef struct __attribute__((packed)){
    /* pushad 저장 순서 */
    uint32_t edi, esi, ebp, esp;
    uint32_t ebx, edx, ecx, eax;
    /* 수동 push */
    uint32_t int_no;
    uint32_t err_code;
    /* CPU 자동 push */
    uint32_t eip, cs, eflags;
    /* 특권 레벨 전환 시만 */
    uint32_t useresp, ss;
} pt_regs;

#define ISR_FUNC(func, num) func##_##num 

void init_idt();
void set_idt_descriptor(idtr_t* idt, void* isr, uint16_t kernel_cs, uint8_t attributes);
void exception_handler(pt_regs* regs);