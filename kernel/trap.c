#include "linux/trap.h"
#include "linux/vga.h"
#include "linux/panic.h"
#include "../mm/process.h"

extern __attribute__((aligned(0x10))) idtr_t KERNEL_IDT[IDT_ENTRIES_SIZE];
extern void *intel_isr_stub_table[32];

static const char* intel_exception_message[32] = {
    [ISR_DIVIDE_ERROR]          = "Divide by Zero",
    [ISR_DEBUG]                 = "Debug",
    [ISR_NMI]                   = "NMI",
    [ISR_BREAKPOINT]            = "Breakpoint",
    [ISR_OVERFLOW]              = "Overflow",
    [ISR_BOUND_RANGE]           = "Bound Range Exceeded",
    [ISR_INVALID_OPCODE]        = "Invalid Opcode",
    [ISR_DEVICE_NOT_AVAILABLE]  = "Device Not Available",
    [ISR_DOUBLE_FAULT]          = "Double Fault",
    [ISR_COPROCESSOR_OVERRUN]   = "Coprocessor Segment Overrun",
    [ISR_INVALID_TSS]           = "Invalid TSS",
    [ISR_SEGMENT_NOT_PRESENT]   = "Segment Not Present",
    [ISR_STACK_SEGMENT_FAULT]   = "Stack-Segment Fault",
    [ISR_GENERAL_PROTECTION]    = "General Protection Fault",
    [ISR_PAGE_FAULT]            = "Page Fault",
    [ISR_FPU_ERROR]             = "x87 FPU Error",
    [ISR_ALIGNMENT_CHECK]       = "Alignment Check",
    [ISR_MACHINE_CHECK]         = "Machine Check",
    [ISR_SIMD_FP_EXCEPTION]     = "SIMD FP Exception",
    [ISR_VIRTUALIZATION]        = "Virtualization Exception",
    [ISR_CONTROL_PROTECTION]    = "Control Protection Exception",
};

const char* exception_name(uint32_t int_no){
    if(int_no < 32 && intel_exception_message[int_no]){
        return intel_exception_message[int_no];
    }
    return "Reserved Exception";
}

void init_trap(){
    __asm__ volatile ("cli");// 안정성 보장
    for(int i= 0; i < 32; i++){
        set_idt_descriptor(KERNEL_IDT + i, intel_isr_stub_table[i], 0x08, IDT_P | IDT_DPL_KERNEL | IDT_GATE_TYPE_32BIT_INTERRUPT);
    }
}

/* 예외 후 그냥 iret 하면 같은 명령을 다시 실행해 무한 반복되므로, 지금은 전부 패닉으로 처리한다 */
void exception_handler(pt_regs* regs){
    if(regs->cs & 0x3){
        /* 유저 프로그램의 잘못은 커널 전체가 아니라 그 프로세스만 끝낸다 */
        kprintf("[pid %u] %s at eip=0x%x - killed\n", process_getpid(), exception_name(regs->int_no), regs->eip);
        process_exit(PROCESS_EXIT_FAULT);
    }
    kpanic_regs(regs, "%s exception", exception_name(regs->int_no));
}
