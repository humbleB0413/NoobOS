.include "kernel/isr_macro.S"

.extern exception_handler

.section .text

/* 0: Divide Error #DE - no error code */
isr_no_err_stub 0,  exception_handler

/* 1: Debug #DB - no error code */
isr_no_err_stub 1,  exception_handler

/* 2: Non-Maskable Interrupt #NMI - no error code */
isr_no_err_stub 2,  exception_handler

/* 3: Breakpoint #BP - no error code */
isr_no_err_stub 3,  exception_handler

/* 4: Overflow #OF - no error code */
isr_no_err_stub 4,  exception_handler

/* 5: Bound Range Exceeded #BR - no error code */
isr_no_err_stub 5,  exception_handler

/* 6: Invalid Opcode #UD - no error code */
isr_no_err_stub 6,  exception_handler

/* 7: Device Not Available #NM - no error code */
isr_no_err_stub 7,  exception_handler

/* 8: Double Fault #DF - error code (always 0) */
isr_err_stub    8,  exception_handler

/* 9: Coprocessor Segment Overrun - no error code (deprecated) */
isr_no_err_stub 9,  exception_handler

/* 10: Invalid TSS #TS - error code */
isr_err_stub    10, exception_handler

/* 11: Segment Not Present #NP - error code */
isr_err_stub    11, exception_handler

/* 12: Stack-Segment Fault #SS - error code */
isr_err_stub    12, exception_handler

/* 13: General Protection Fault #GP - error code */
isr_err_stub    13, exception_handler

/* 14: Page Fault #PF - error code */
isr_err_stub    14, exception_handler

/* 15: Reserved */
isr_no_err_stub 15, exception_handler

/* 16: x87 FPU Error #MF - no error code */
isr_no_err_stub 16, exception_handler

/* 17: Alignment Check #AC - error code */
isr_err_stub    17, exception_handler

/* 18: Machine Check #MC - no error code */
isr_no_err_stub 18, exception_handler

/* 19: SIMD FP Exception #XM - no error code */
isr_no_err_stub 19, exception_handler

/* 20: Virtualization Exception #VE - no error code */
isr_no_err_stub 20, exception_handler

/* 21: Control Protection Exception #CP - error code */
isr_err_stub    21, exception_handler

/* 22~31: Reserved */
isr_no_err_stub 22, exception_handler
isr_no_err_stub 23, exception_handler
isr_no_err_stub 24, exception_handler
isr_no_err_stub 25, exception_handler
isr_no_err_stub 26, exception_handler
isr_no_err_stub 27, exception_handler
isr_no_err_stub 28, exception_handler
isr_no_err_stub 29, exception_handler
isr_no_err_stub 30, exception_handler
isr_no_err_stub 31, exception_handler

/* 스텁 주소 배열 */
.global intel_isr_stub_table
intel_isr_stub_table:
    .long isr_stub_0
    .long isr_stub_1
    .long isr_stub_2
    .long isr_stub_3
    .long isr_stub_4
    .long isr_stub_5
    .long isr_stub_6
    .long isr_stub_7
    .long isr_stub_8
    .long isr_stub_9
    .long isr_stub_10
    .long isr_stub_11
    .long isr_stub_12
    .long isr_stub_13
    .long isr_stub_14
    .long isr_stub_15
    .long isr_stub_16
    .long isr_stub_17
    .long isr_stub_18
    .long isr_stub_19
    .long isr_stub_20
    .long isr_stub_21
    .long isr_stub_22
    .long isr_stub_23
    .long isr_stub_24
    .long isr_stub_25
    .long isr_stub_26
    .long isr_stub_27
    .long isr_stub_28
    .long isr_stub_29
    .long isr_stub_30
    .long isr_stub_31