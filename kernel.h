#ifndef KERNEL_H
#define KERNEL_H

#include "linux/vga.h"
#include "linux/gdt.h"
#include "linux/idt.h"
#include "linux/trap.h"
#include "linux/panic.h"
#include "linux/irq.h"
#include "driver/pic.h"
#include "driver/timer.h"
#include "driver/keyboard.h"
#include "driver/serial.h"
#include "mm/pmm.h"
#include "mm/vmm.h"
#include "mm/process.h"
#include "multiboot.h"

#endif