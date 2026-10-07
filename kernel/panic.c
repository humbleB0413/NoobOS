#include "linux/panic.h"
#include "linux/vga.h"

static void panic_header(void) {
  __asm__ volatile("cli");
  terminal_setcolor(VGA_COLOR_WHITE | VGA_COLOR_RED << 4);
  kprintf("\nKERNEL PANIC: ");
}

__attribute__((noreturn)) static void panic_halt(void) {
  terminal_setcolor(VGA_COLOR_LIGHT_GREY | VGA_COLOR_BLACK << 4);
  kprintf("\nSystem halted.\n");
  while (1) {
    __asm__ volatile("cli; hlt");
  }
}

void kpanic(const char *fmt, ...) {
  va_list ap;
  panic_header();
  va_start(ap, fmt);
  kvprintf(fmt, ap);
  va_end(ap);
  panic_halt();
}

void kpanic_regs(pt_regs *regs, const char *fmt, ...) {
  va_list ap;
  panic_header();
  va_start(ap, fmt);
  kvprintf(fmt, ap);
  va_end(ap);

  terminal_setcolor(VGA_COLOR_LIGHT_GREY | VGA_COLOR_BLACK << 4);
  kprintf("\n  int=%u err=0x%x eip=0x%x cs=0x%x eflags=0x%x\n",
          regs->int_no, regs->err_code, regs->eip, regs->cs, regs->eflags);
  kprintf("  eax=0x%x ebx=0x%x ecx=0x%x edx=0x%x\n",
          regs->eax, regs->ebx, regs->ecx, regs->edx);
  kprintf("  esi=0x%x edi=0x%x ebp=0x%x esp=0x%x\n",
          regs->esi, regs->edi, regs->ebp, regs->esp);
  if (regs->cs & 0x3) {
    kprintf("  useresp=0x%x ss=0x%x\n", regs->useresp, regs->ss);
  }
  panic_halt();
}
