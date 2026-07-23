#include "timer.h"
#include "linux/irq.h"
#include "../driver/pic.h"

extern __attribute__((aligned(0x10))) idtr_t KERNEL_IDT[IDT_ENTRIES_SIZE];
extern void irq_system_timer(void);

static uint64_t SYSTEM_TIMER = 0;

void init_timer(){
    uint16_t divisor = PIT_BASE_FREQUENCY / PIT_FREQUENCY;

    __asm__ volatile("cli");

    outb(PIT_CMD_MODE_PORT, PIT_CMD_SELECT_CH0 | 
        PIT_CMD_SELECT_LOW_HIGH_BYTE | PIT_CMD_SELECT_PRD_MODE | PIT_CMD_SELECT_BINARY_ENC);
    outb(PIT_CH0_DATA_PORT, divisor & 0xFF);
    outb(PIT_CH0_DATA_PORT, (divisor >> 8) & 0xFF);

    set_idt_descriptor(KERNEL_IDT+IRQ_BASE+0, (void*)irq_system_timer, 0x08, IDT_P | IDT_DPL_KERNEL | IDT_GATE_TYPE_32BIT_INTERRUPT);
    __asm__ volatile("sti");

    return;
}

void irq_pic_timer(pt_regs* regs){
    uint8_t irq = (uint8_t)(regs->int_no - IRQ_BASE);
    SYSTEM_TIMER += 1;
    pic_send_eoi(irq);
}

static inline uint64_t ms_to_ticks(uint64_t ms){
    return ms / (1000 / PIT_FREQUENCY);
}

uint64_t get_ticks(){
    return SYSTEM_TIMER;
}

void sleep(uint64_t s){
    msleep(1000 * s);
}

void msleep(uint64_t ms){
    uint64_t wait = ms_to_ticks(ms);
    uint64_t prev_sys_timer = SYSTEM_TIMER;

    while(get_ticks() - prev_sys_timer < wait);

    return ;
}
