.include "kernel/isr_macro.S"

.extern irq_handler

.section .text

/* 32~47: remapped hardware IRQ0~15 - no CPU-pushed error code */
isr_no_err_stub 32, irq_handler
isr_no_err_stub 33, irq_handler
isr_no_err_stub 34, irq_handler
isr_no_err_stub 35, irq_handler
isr_no_err_stub 36, irq_handler
isr_no_err_stub 37, irq_handler
isr_no_err_stub 38, irq_handler
isr_no_err_stub 39, irq_handler
isr_no_err_stub 40, irq_handler
isr_no_err_stub 41, irq_handler
isr_no_err_stub 42, irq_handler
isr_no_err_stub 43, irq_handler
isr_no_err_stub 44, irq_handler
isr_no_err_stub 45, irq_handler
isr_no_err_stub 46, irq_handler
isr_no_err_stub 47, irq_handler

/* 스텁 주소 배열 */
.global irq_stub_table
irq_stub_table:
    .long isr_stub_32
    .long isr_stub_33
    .long isr_stub_34
    .long isr_stub_35
    .long isr_stub_36
    .long isr_stub_37
    .long isr_stub_38
    .long isr_stub_39
    .long isr_stub_40
    .long isr_stub_41
    .long isr_stub_42
    .long isr_stub_43
    .long isr_stub_44
    .long isr_stub_45
    .long isr_stub_46
    .long isr_stub_47
