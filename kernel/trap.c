#include "linux/trap.h"
#include "linux/vga.h"

extern __attribute__((aligned(0x10))) idtr_t KERNEL_IDT[IDT_ENTRIES_SIZE];
extern void *intel_isr_stub_table[32];

static char* intel_exception_message[32] = {
    [0]  = "Divide by Zero\n",
    [1]  = "Debug\n",
    [2]  = "NMI\n",
    [3]  = "Breakpoint\n",
    [4]  = "Overflow\n",
    [5]  = "Bound Range Exceeded\n",
    [6]  = "Invalid Opcode\n",
    [7]  = "Device Not Available\n",
    [8]  = "Double Fault\n",
    [13] = "General Protection Fault\n",
    [14] = "Page Fault\n",
};

void init_trap(){
    __asm__ volatile ("cli");// 안정성 보장
    for(int i= 0; i < 32; i++){
        set_idt_descriptor(KERNEL_IDT + i, intel_isr_stub_table[i], 0x08, IDT_P | IDT_DPL_KERNEL | IDT_GATE_TYPE_32BIT_INTERRUPT);
    }
}

void exception_handler(pt_regs* regs){
    switch (regs->int_no)
    {
    case ISR_DIVIDE_ERROR:
        kprintf(intel_exception_message[ISR_DIVIDE_ERROR]);
        break;
    case ISR_DEBUG:
        kprintf(intel_exception_message[ISR_DEBUG]);
        break;
    case ISR_NMI:
        kprintf(intel_exception_message[ISR_NMI]);
        break;
    case ISR_BREAKPOINT:
        kprintf(intel_exception_message[ISR_BREAKPOINT]);
        break;
    case ISR_OVERFLOW:
        kprintf(intel_exception_message[ISR_OVERFLOW]);
        break;
    case ISR_BOUND_RANGE:
        kprintf(intel_exception_message[ISR_BOUND_RANGE]);
        break;
    case ISR_INVALID_OPCODE:
        kprintf(intel_exception_message[ISR_INVALID_OPCODE]);
        break;
    case ISR_DEVICE_NOT_AVAILABLE:
        kprintf(intel_exception_message[ISR_DEVICE_NOT_AVAILABLE]);
        break;
    case ISR_DOUBLE_FAULT:
        kprintf(intel_exception_message[ISR_DOUBLE_FAULT]);
        break;
    case ISR_GENERAL_PROTECTION:
        kprintf(intel_exception_message[ISR_GENERAL_PROTECTION]);
        break;
    case ISR_PAGE_FAULT:
        kprintf(intel_exception_message[ISR_PAGE_FAULT]);
        break;
    default:
        kprintf("Extra Error!\n");
        break;
    }
}