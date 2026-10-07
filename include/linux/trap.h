/**
 * This file is for initializing interrupt 0 ~ 31,
 * which are the interrupts correspond to intel reservattion
 */

#pragma once

#include "idt.h"

/* Intel 예약 벡터 0~31 */
#define ISR_DIVIDE_ERROR            0   /* #DE - Divide Error */
#define ISR_DEBUG                   1   /* #DB - Debug */
#define ISR_NMI                     2   /* #NMI - Non-Maskable Interrupt */
#define ISR_BREAKPOINT              3   /* #BP - Breakpoint */
#define ISR_OVERFLOW                4   /* #OF - Overflow */
#define ISR_BOUND_RANGE             5   /* #BR - Bound Range Exceeded */
#define ISR_INVALID_OPCODE          6   /* #UD - Invalid Opcode */
#define ISR_DEVICE_NOT_AVAILABLE    7   /* #NM - Device Not Available */
#define ISR_DOUBLE_FAULT            8   /* #DF - Double Fault */
#define ISR_COPROCESSOR_OVERRUN     9   /*     - Coprocessor Segment Overrun (deprecated) */
#define ISR_INVALID_TSS             10  /* #TS - Invalid TSS */
#define ISR_SEGMENT_NOT_PRESENT     11  /* #NP - Segment Not Present */
#define ISR_STACK_SEGMENT_FAULT     12  /* #SS - Stack-Segment Fault */
#define ISR_GENERAL_PROTECTION      13  /* #GP - General Protection Fault */
#define ISR_PAGE_FAULT              14  /* #PF - Page Fault */
#define ISR_RESERVED_15             15  /*     - Reserved */
#define ISR_FPU_ERROR               16  /* #MF - x87 FPU Error */
#define ISR_ALIGNMENT_CHECK         17  /* #AC - Alignment Check */
#define ISR_MACHINE_CHECK           18  /* #MC - Machine Check */
#define ISR_SIMD_FP_EXCEPTION       19  /* #XM - SIMD FP Exception */
#define ISR_VIRTUALIZATION          20  /* #VE - Virtualization Exception */
#define ISR_CONTROL_PROTECTION      21  /* #CP - Control Protection Exception */
#define ISR_RESERVED_22             22  /*     - Reserved */
#define ISR_RESERVED_23             23  /*     - Reserved */
#define ISR_RESERVED_24             24  /*     - Reserved */
#define ISR_RESERVED_25             25  /*     - Reserved */
#define ISR_RESERVED_26             26  /*     - Reserved */
#define ISR_RESERVED_27             27  /*     - Reserved */
#define ISR_RESERVED_28             28  /*     - Reserved */
#define ISR_RESERVED_29             29  /*     - Reserved */
#define ISR_RESERVED_30             30  /*     - Reserved */
#define ISR_RESERVED_31             31  /*     - Reserved */

void init_trap();
void exception_handler(pt_regs* regs);
const char* exception_name(uint32_t int_no);