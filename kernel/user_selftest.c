#ifdef DEBUG
#include "linux/vga.h"
#include "linux/user_selftest.h"
#include "../mm/process.h"
#include "../mm/vmm.h"
#include "../driver/timer.h"
#include "../mm/pmm.h"
#include "uapi/syscall_nr.h"
#include "../fs/vfs.h"
#include "linux/exec.h"

#define UBLOB(name) extern const char ublob_##name[], ublob_##name##_end[]
UBLOB(hello);
UBLOB(cli);
UBLOB(kwrite);
UBLOB(badptr);
UBLOB(spin);
UBLOB(cat);
UBLOB(noent);

#define UTEST_CODE_BASE 0x08048000u

static uint32_t spawn_blob(const char *name, const char *start, const char *end) {
  address_space_t *as = vmm_create_address_space();
  uint32_t len = (uint32_t)(end - start);
  if (!as || vmm_map_user_range(as, UTEST_CODE_BASE, len, 0) < 0 ||
      vmm_map_user_range(as, USER_STACK_TOP - USER_STACK_PAGES * 4096, USER_STACK_PAGES * 4096, 1) < 0) {
    vmm_destroy_address_space(as);
    return 0;
  }
  vmm_copy_to_user(as, UTEST_CODE_BASE, start, len);
  process_t *proc = create_user_process(name, as, UTEST_CODE_BASE, USER_STACK_TOP);
  if (!proc) {
    vmm_destroy_address_space(as);
    return 0;
  }
  return proc->pid;
}

static int check(const char *name, int cond) {
  kprintf("  [%s] %s\n", cond ? "PASS" : "FAIL", name);
  return cond;
}

int usermode_selftest(void) {
  int pass = 1;
  kprintf("=== user mode selftest ===\n");
  uint32_t frames_before = pmm_free_count();

  uint32_t pid = spawn_blob("u-hello", ublob_hello, ublob_hello_end);
  pass &= check("syscalls write/getpid/sleep/exit, exit code 7", pid && process_wait(pid) == 7);

  pid = spawn_blob("u-cli", ublob_cli, ublob_cli_end);
  pass &= check("privileged cli in ring3 -> #GP kills only that process", pid && process_wait(pid) == PROCESS_EXIT_FAULT);

  pid = spawn_blob("u-kwrite", ublob_kwrite, ublob_kwrite_end);
  pass &= check("write to kernel memory -> #PF kills the process", pid && process_wait(pid) == PROCESS_EXIT_FAULT);

  pid = spawn_blob("u-badptr", ublob_badptr, ublob_badptr_end);
  pass &= check("kernel pointer passed to write() -> E_FAULT", pid && process_wait(pid) == E_FAULT);

  vfs_node_t *motd = vfs_lookup("/etc/motd");
  pid = spawn_blob("u-cat", ublob_cat, ublob_cat_end);
  pass &= check("open/read/close a file from the initrd", motd && pid && process_wait(pid) == (int)motd->size);

  pid = spawn_blob("u-noent", ublob_noent, ublob_noent_end);
  pass &= check("open of a missing file -> E_NOENT", pid && process_wait(pid) == E_NOENT);

  int epid = exec_user("/bin/echo", "echo   ELF loader: argv works");
  pass &= check("exec ELF /bin/echo with arguments", epid > 0 && process_wait((uint32_t)epid) == 0);
  pass &= check("exec of a non-ELF file -> E_INVAL", exec_user("/etc/motd", 0) == E_INVAL);
  pass &= check("exec of a missing file -> E_NOENT", exec_user("/bin/nope", 0) == E_NOENT);

  pid = spawn_blob("u-spin", ublob_spin, ublob_spin_end);
  uint64_t t0 = get_ticks();
  msleep(50);
  pass &= check("kernel keeps running while ring3 spins (preemption)", get_ticks() - t0 >= 50);
  pass &= check("kill a spinning user process", pid && process_kill(pid) == 0 && process_wait(pid) == PROCESS_EXIT_KILLED);

  pass &= check("all user frames/page tables returned to PMM", pmm_free_count() == frames_before);
  kprintf("=== result: %s ===\n", pass ? "ALL PASS" : "SOME FAILED");
  return pass;
}
#endif
