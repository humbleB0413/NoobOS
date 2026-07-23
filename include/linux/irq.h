/**
 * This file is for initializing interrupt 32 ~ 47,
 * which are the remapped hardware IRQ0 ~ IRQ15 lines from the 8259 PIC.
 */

#pragma once

#include "idt.h"

#define IRQ_BASE            32
#define IRQ_COUNT           16

#define IRQ_TIMER           (IRQ_BASE + 0)
#define IRQ_KEYBOARD        (IRQ_BASE + 1)
#define IRQ_CASCADE         (IRQ_BASE + 2)
#define IRQ_COM2            (IRQ_BASE + 3)
#define IRQ_COM1            (IRQ_BASE + 4)
#define IRQ_LPT2            (IRQ_BASE + 5)
#define IRQ_FLOPPY          (IRQ_BASE + 6)
#define IRQ_LPT1            (IRQ_BASE + 7)
#define IRQ_RTC             (IRQ_BASE + 8)
#define IRQ_ACPI            (IRQ_BASE + 9)
#define IRQ_OPEN1           (IRQ_BASE + 10)
#define IRQ_OPEN2           (IRQ_BASE + 11)
#define IRQ_MOUSE           (IRQ_BASE + 12)
#define IRQ_FPU             (IRQ_BASE + 13)
#define IRQ_ATA_PRIMARY     (IRQ_BASE + 14)
#define IRQ_ATA_SECONDARY   (IRQ_BASE + 15)

void init_irq();
void irq_handler(pt_regs* regs);
