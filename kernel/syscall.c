#include "linux/syscall.h"
#include "linux/console.h"
#include "linux/vga.h"
#include "../mm/process.h"
#include "../driver/timer.h"
#include "../fs/vfs.h"
#include "string.h"
#include "linux/exec.h"

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

/* NUL 로 끝나는 유저 문자열을 커널 버퍼로 복사. 너무 길거나 잘못된 포인터면 음수 */
static int copy_user_string(char *dst, uint32_t src, uint32_t max) {
  for (uint32_t i = 0; i < max; i++) {
    if (!user_buffer_ok(src + i, 1, 0)) return E_FAULT;
    dst[i] = *(const char *)(src + i);
    if (dst[i] == '\0') return (int)i;
  }
  return E_INVAL;
}

static int file_fd_ok(process_t *proc, uint32_t fd) {
  return fd >= PROCESS_FIRST_FILE_FD && fd < PROCESS_MAX_FILES && proc->files[fd].node;
}

static int sys_open(uint32_t upath) {
  char path[VFS_NAME_MAX + 1];
  int r = copy_user_string(path, upath, sizeof(path));
  if (r < 0) return r;
  vfs_node_t *node = vfs_lookup(path);
  if (!node || node->type != VFS_FILE) return E_NOENT;

  process_t *proc = process_current();
  for (uint32_t fd = PROCESS_FIRST_FILE_FD; fd < PROCESS_MAX_FILES; fd++) {
    if (!proc->files[fd].node) {
      proc->files[fd].node = node;
      proc->files[fd].offset = 0;
      return (int)fd;
    }
  }
  return E_MFILE;
}

static int sys_close(uint32_t fd) {
  process_t *proc = process_current();
  if (!file_fd_ok(proc, fd)) return E_BADF;
  proc->files[fd].node = 0;
  return 0;
}

static int sys_readdir(uint32_t index, uint32_t ubuf, uint32_t len) {
  vfs_node_t *node = vfs_entry(index);
  if (!node) return 0;
  uint32_t need = (uint32_t)strlen(node->path) + 1;
  if (len < need) return E_INVAL;
  if (!user_buffer_ok(ubuf, need, 1)) return E_FAULT;
  memcpy((void *)ubuf, node->path, need);
  return (int)node->type;
}

static int sys_spawn(uint32_t upath, uint32_t uargline) {
  char path[VFS_NAME_MAX + 1];
  char argline[EXEC_ARGLINE_MAX];
  int r = copy_user_string(path, upath, sizeof(path));
  if (r < 0) return r;
  argline[0] = '\0';
  if (uargline) {
    r = copy_user_string(argline, uargline, sizeof(argline));
    if (r < 0) return r;
  }
  return exec_user(path, argline);
}

static int sys_wait(uint32_t pid) {
  if (pid == process_getpid()) return E_INVAL;
  return process_wait(pid);
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
  if (len == 0) return 0;
  if (!user_buffer_ok(buf, len, 1)) return E_FAULT;
  if (fd != 0) {
    process_t *proc = process_current();
    if (!file_fd_ok(proc, fd)) return E_BADF;
    int n = vfs_read(proc->files[fd].node, proc->files[fd].offset, (void *)buf, len);
    if (n > 0) proc->files[fd].offset += (uint32_t)n;
    return n;
  }
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
    case SYS_OPEN:
      ret = sys_open(regs->ebx);
      break;
    case SYS_CLOSE:
      ret = sys_close(regs->ebx);
      break;
    case SYS_SPAWN:
      ret = sys_spawn(regs->ebx, regs->ecx);
      break;
    case SYS_WAIT:
      ret = sys_wait(regs->ebx);
      break;
    case SYS_READDIR:
      ret = sys_readdir(regs->ebx, regs->ecx, regs->edx);
      break;
    default:
      ret = E_NOSYS;
      break;
  }
  regs->eax = (uint32_t)ret;
}
