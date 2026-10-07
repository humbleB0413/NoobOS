#include "linux/syscall.h"
#include "linux/console.h"
#include "linux/vga.h"
#include "../mm/process.h"
#include "../driver/timer.h"

extern __attribute__((aligned(0x10))) idtr_t KERNEL_IDT[IDT_ENTRIES_SIZE];
extern void asm_syscall(void);

void init_syscall(void) {
  /* DPL 3 이어야 ring3 에서 int 0x80 을 쓸 수 있다. interrupt gate 라 처리 중엔 IF=0 */
  set_idt_descriptor(KERNEL_IDT + SYSCALL_VECTOR, asm_syscall, 0x08,
                     IDT_P | IDT_DPL_USER | IDT_GATE_TYPE_32BIT_INTERRUPT);
}

/* 유저가 넘긴 포인터는 반드시 현재 주소 공간에서 유저 접근 가능한지 확인한 뒤에만 만진다 —
 * 커널 주소를 넘겨 커널 메모리를 읽거나 쓰게 만드는 것을 막는다 */
static int user_buffer_ok(uint32_t addr, uint32_t len, int writable) {
  return len == 0 || vmm_user_range_ok(process_current_as(), addr, len, writable);
}

static int sys_write(uint32_t fd, uint32_t buf, uint32_t len) {
  if (fd != 1 && fd != 2) return E_BADF;
  if (!user_buffer_ok(buf, len, 0)) return E_FAULT;
  const char *s = (const char *)buf;
  for (uint32_t i = 0; i < len; i++) kprintf("%c", s[i]);
  return (int)len;
}

/* 콘솔에서 최소 한 글자를 기다린 뒤, 이미 들어와 있는 입력은 len 까지 더 가져간다 */
static int sys_read(uint32_t fd, uint32_t buf, uint32_t len) {
  if (fd != 0) return E_BADF;
  if (len == 0) return 0;
  if (!user_buffer_ok(buf, len, 1)) return E_FAULT;
  char *dst = (char *)buf;
  uint32_t n = 0;
  dst[n++] = (char)console_getchar();
  int c;
  while (n < len && dst[n - 1] != '\n' && (c = console_poll_char())) dst[n++] = (char)c;
  return (int)n;
}

void syscall_dispatch(pt_regs *regs) {
  int ret;
  switch (regs->eax) {
    case SYS_EXIT:
      process_exit((int)regs->ebx);
    case SYS_WRITE:
      ret = sys_write(regs->ebx, regs->ecx, regs->edx);
      break;
    case SYS_READ:
      ret = sys_read(regs->ebx, regs->ecx, regs->edx);
      break;
    case SYS_GETPID:
      ret = (int)process_getpid();
      break;
    case SYS_SLEEP:
      process_sleep(regs->ebx);
      ret = 0;
      break;
    case SYS_YIELD:
      yield();
      ret = 0;
      break;
    case SYS_UPTIME:
      ret = (int)get_ticks();
      break;
    default:
      ret = E_NOSYS;
      break;
  }
  regs->eax = (uint32_t)ret;
}
