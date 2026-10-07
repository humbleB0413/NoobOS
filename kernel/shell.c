#include "linux/shell.h"
#include "linux/console.h"
#include "linux/exec.h"
#include "linux/vga.h"
#include "linux/user_selftest.h"
#include "string.h"
#include "kstd.h"
#include "port_io.h"
#include "../driver/keyboard.h"
#include "../driver/rtc.h"
#include "../driver/ata.h"
#include "../driver/timer.h"
#include "../fs/vfs.h"
#include "../mm/pmm.h"
#include "../mm/process.h"
#include "../mm/vmm.h"

#define SHELL_LINE_MAX  EXEC_ARGLINE_MAX
#define SHELL_HISTORY   8
#define SHELL_MAX_ARGS  8

static char history[SHELL_HISTORY][SHELL_LINE_MAX];
static int history_count = 0;

typedef struct {
  const char *name;
  const char *usage;
  const char *help;
  void (*run)(int argc, char **argv);
} command_t;

/* ── 줄 입력 ─────────────────────────────────────────────────────────────── */

static void erase_line(int len) {
  while (len-- > 0) kprintf("\b");
}

/* 백스페이스와 ↑/↓ 히스토리를 지원하는 한 줄 입력 */
static int read_line(char *buf, int max) {
  int len = 0;
  int hist = history_count;  /* history_count == 지금 입력 중인 새 줄 */

  for (;;) {
    int c = console_getchar();
    if (c == '\n') {
      kprintf("\n");
      buf[len] = '\0';
      return len;
    }
    if (c == '\b') {
      if (len > 0) {
        len--;
        kprintf("\b");
      }
    } else if (c == KEY_UP || c == KEY_DOWN) {
      int next = hist + (c == KEY_UP ? -1 : 1);
      int oldest = history_count > SHELL_HISTORY ? history_count - SHELL_HISTORY : 0;
      if (next < oldest || next > history_count) continue;
      hist = next;
      erase_line(len);
      if (hist == history_count) {
        len = 0;
      } else {
        strcpy(buf, history[hist % SHELL_HISTORY]);
        len = strlen(buf);
        kprintf("%s", buf);
      }
    } else if (isprint(c) && len < max - 1) {
      buf[len++] = (char)c;
      kprintf("%c", c);
    }
  }
}

static void remember(const char *line) {
  if (history_count > 0 && strcmp(history[(history_count - 1) % SHELL_HISTORY], line) == 0) return;
  strcpy(history[history_count % SHELL_HISTORY], line);
  history_count++;
}

static int split_args(char *line, char **argv) {
  int argc = 0;
  for (char *p = line; *p && argc < SHELL_MAX_ARGS;) {
    while (*p == ' ') *p++ = '\0';
    if (!*p) break;
    argv[argc++] = p;
    while (*p && *p != ' ') p++;
  }
  return argc;
}

/* ── 명령 ────────────────────────────────────────────────────────────────── */

static const command_t commands[];

static void cmd_help(int argc, char **argv) {
  (void)argc;
  (void)argv;
  kprintf("Built-in commands:\n");
  for (const command_t *c = commands; c->name; c++) {
    kprintf("  %-28s %s\n", c->usage, c->help);
  }
  kprintf("Anything else runs /bin/<name> in ring 3; append '&' to run it in the background.\n");
  kprintf("Ctrl+C stops the foreground program, Up/Down recall history.\n");
}

static void cmd_clear(int argc, char **argv) {
  (void)argc;
  (void)argv;
  terminal_initialize();
}

static void cmd_echo(int argc, char **argv) {
  for (int i = 1; i < argc; i++) kprintf(i + 1 < argc ? "%s " : "%s", argv[i]);
  kprintf("\n");
}

static const char *state_name(uint32_t state) {
  switch (state) {
    case PROCESS_RUN: return "run";
    case PROCESS_SLEEP: return "sleep";
    default: return "?";
  }
}

static void cmd_ps(int argc, char **argv) {
  (void)argc;
  (void)argv;
  process_info_t info[PROCESS_MAX];
  int n = process_snapshot(info, PROCESS_MAX);
  kprintf("  PID  STATE   CPU(ms)  NAME\n");
  for (int i = 0; i < n; i++) {
    kprintf("  %3u  %-6s %8llu  %s\n", info[i].pid, state_name(info[i].state), info[i].cpu_ticks, info[i].name);
  }
}

static void cmd_kill(int argc, char **argv) {
  if (argc < 2) {
    kprintf("usage: kill <pid>\n");
    return;
  }
  uint32_t pid = (uint32_t)atoi(argv[1]);
  if (process_kill(pid) < 0) kprintf("kill: cannot kill pid %u\n", pid);
}

static void cmd_ls(int argc, char **argv) {
  char prefix[VFS_NAME_MAX + 1];
  vfs_normalize(prefix, argc > 1 ? argv[1] : "", sizeof(prefix));
  int plen = strlen(prefix);
  for (uint32_t i = 0; i < vfs_count(); i++) {
    vfs_node_t *node = vfs_entry(i);
    if (strncmp(node->path, prefix, plen) != 0) continue;
    if (node->type == VFS_DIR) kprintf("  d %8s  /%s/\n", "-", node->path);
    else kprintf("  - %8u  /%s\n", node->size, node->path);
  }
}

static void cmd_cat(int argc, char **argv) {
  if (argc < 2) {
    kprintf("usage: cat <file>\n");
    return;
  }
  vfs_node_t *node = vfs_lookup(argv[1]);
  if (!node || node->type != VFS_FILE) {
    kprintf("cat: %s: no such file\n", argv[1]);
    return;
  }
  char buf[129];
  for (uint32_t off = 0; off < node->size;) {
    int n = vfs_read(node, off, buf, sizeof(buf) - 1);
    if (n <= 0) break;
    buf[n] = '\0';
    kprintf("%s", buf);
    off += (uint32_t)n;
  }
}

static void cmd_mem(int argc, char **argv) {
  (void)argc;
  (void)argv;
  uint32_t free_frames = pmm_free_count();
  kprintf("  physical: %u free frames (%u KB)\n", free_frames, free_frames * (PAGE_SIZE / 1024));
  kprintf("  layout:   0x00000000-0x003FFFFF kernel (identity)\n");
  kprintf("            0x08000000-0x3FFFFFFF user space (per process)\n");
  kprintf("            0x40000000-0x463FFFFF kernel heap (100 MB)\n");
}

static void cmd_frames(int argc, char **argv) {
  (void)argc;
  (void)argv;
  print_frame_state();
}

static void cmd_uptime(int argc, char **argv) {
  (void)argc;
  (void)argv;
  uint64_t ms = get_ticks();
  kprintf("  up %llu.%03llu s\n", ms / 1000, ms % 1000);
}

static void cmd_date(int argc, char **argv) {
  (void)argc;
  (void)argv;
  rtc_time_t t;
  rtc_read(&t);
  kprintf("  %04u-%02u-%02u %02u:%02u:%02u UTC\n", t.year, t.month, t.day, t.hour, t.minute, t.second);
}

static void cmd_sleep(int argc, char **argv) {
  msleep(argc > 1 ? (uint32_t)atoi(argv[1]) : 1000);
}

static void cmd_reboot(int argc, char **argv) {
  (void)argc;
  (void)argv;
  kprintf("rebooting...\n");
  /* 8042 키보드 컨트롤러의 CPU 리셋 펄스 */
  outb(0x64, 0xFE);
  while (1) __asm__ volatile("hlt");
}

/* disk          — 디스크 정보
 * disk read N   — N 번 섹터를 16진수로 덤프
 * disk write N text — N 번 섹터 앞부분에 text 를 쓴다 (나머지는 0) */
static void cmd_disk(int argc, char **argv) {
  const ata_drive_t *d = ata_drive();
  if (!d->present) {
    kprintf("disk: no ATA drive on the primary bus (run QEMU with -hda <image>)\n");
    return;
  }
  if (argc < 3) {
    kprintf("  primary master: \"%s\", %u sectors (%u MB)\n", d->model, d->sectors, d->sectors / 2048);
    return;
  }
  uint32_t lba = strtoul(argv[2], 0, 0);
  uint8_t sector[ATA_SECTOR_SIZE];
  if (strcmp(argv[1], "write") == 0) {
    memset(sector, 0, sizeof(sector));
    for (int i = 3, o = 0; i < argc; i++) {
      for (const char *p = argv[i]; *p && o < ATA_SECTOR_SIZE - 1; p++) sector[o++] = (uint8_t)*p;
      if (i + 1 < argc && o < ATA_SECTOR_SIZE - 1) sector[o++] = ' ';
    }
    kprintf(ata_write(lba, 1, sector) == 0 ? "  wrote sector %u\n" : "  write of sector %u failed\n", lba);
    return;
  }
  if (ata_read(lba, 1, sector) < 0) {
    kprintf("  read of sector %u failed\n", lba);
    return;
  }
  /* 앞 128 바이트만: 16 바이트씩 hex + ASCII */
  for (int row = 0; row < 8; row++) {
    kprintf("  %03x:", row * 16);
    for (int i = 0; i < 16; i++) kprintf(" %02x", sector[row * 16 + i]);
    kprintf("  ");
    for (int i = 0; i < 16; i++) {
      uint8_t c = sector[row * 16 + i];
      kprintf("%c", isprint(c) ? c : '.');
    }
    kprintf("\n");
  }
}

#ifdef DEBUG
static void cmd_selftest(int argc, char **argv) {
  const char *which = argc > 1 ? argv[1] : "all";
  int all = strcmp(which, "all") == 0;
  if (all || strcmp(which, "lib") == 0) lib_selftest();
  if (all || strcmp(which, "user") == 0) usermode_selftest();
  if (all || strcmp(which, "kmalloc") == 0) kmalloc_selftest();
}
#endif

static const command_t commands[] = {
    {"help", "help", "show this list", cmd_help},
    {"clear", "clear", "clear the screen", cmd_clear},
    {"echo", "echo <text>", "print text", cmd_echo},
    {"ps", "ps", "list processes", cmd_ps},
    {"kill", "kill <pid>", "terminate a process", cmd_kill},
    {"ls", "ls [dir]", "list initrd files", cmd_ls},
    {"cat", "cat <file>", "print a file", cmd_cat},
    {"mem", "mem", "memory usage and layout", cmd_mem},
    {"frames", "frames", "PMM bitmap of low memory", cmd_frames},
    {"uptime", "uptime", "time since boot", cmd_uptime},
    {"date", "date", "RTC date and time", cmd_date},
    {"sleep", "sleep [ms]", "sleep (default 1000 ms)", cmd_sleep},
    {"disk", "disk [read|write N [text]]", "ATA disk info / sector I/O", cmd_disk},
#ifdef DEBUG
    {"selftest", "selftest [lib|user|kmalloc]", "run in-kernel tests", cmd_selftest},
#endif
    {"reboot", "reboot", "reset the machine", cmd_reboot},
    {"exit", "exit", "leave the shell", 0},
    {0, 0, 0, 0},
};

static const command_t *find_command(const char *name) {
  for (const command_t *c = commands; c->name; c++) {
    if (strcmp(c->name, name) == 0) return c;
  }
  return 0;
}

/* 내장 명령이 아니면 /bin/<이름> (또는 '/' 가 들어간 경로 그대로)을 유저 프로세스로 실행 */
static void run_program(int argc, char **argv) {
  int background = 0;
  if (strcmp(argv[argc - 1], "&") == 0) {
    background = 1;
    argc--;
  }
  if (argc == 0) return;

  char path[VFS_NAME_MAX + 1];
  if (strchr(argv[0], '/')) {
    strncpy(path, argv[0], sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
  } else {
    strcpy(path, "/bin/");
    strncat(path, argv[0], sizeof(path) - 6);
  }

  /* argv 를 다시 한 줄로 이어 exec 의 argline 으로 넘긴다 (split 이 '\0' 을 박아 넣었으므로) */
  char argline[SHELL_LINE_MAX];
  argline[0] = '\0';
  for (int i = 0; i < argc; i++) {
    if (i) strcat(argline, " ");
    strcat(argline, argv[i]);
  }

  int pid = exec_user(path, argline);
  if (pid < 0) {
    kprintf("%s: %s\n", argv[0], pid == -2 ? "command not found" : "cannot execute");
    return;
  }
  if (background) {
    kprintf("[%d] started in background\n", pid);
    return;
  }
  /* 기다리는 동안 Ctrl+C 가 오면 포그라운드 프로그램을 끝낸다 */
  while (process_alive((uint32_t)pid)) {
    if (console_take_interrupt()) {
      kprintf("^C\n");
      process_kill((uint32_t)pid);
      break;
    }
    process_sleep(10);
  }
  int code = process_wait((uint32_t)pid);
  if (code == PROCESS_EXIT_FAULT) kprintf("[%d] terminated by a fault\n", pid);
  else if (code == PROCESS_EXIT_KILLED) kprintf("[%d] killed\n", pid);
  else if (code != 0) kprintf("[%d] exit code %d\n", pid, code);
}

void shell_main(void) {
  char line[SHELL_LINE_MAX];
  char work[SHELL_LINE_MAX];
  char *argv[SHELL_MAX_ARGS];

  vfs_node_t *motd = vfs_lookup("/etc/motd");
  if (motd) cmd_cat(2, (char *[]){"cat", "/etc/motd", 0});

  for (;;) {
    terminal_setcolor(VGA_COLOR_LIGHT_GREEN | VGA_COLOR_BLACK << 4);
    kprintf("noob> ");
    terminal_setcolor(VGA_COLOR_LIGHT_GREY | VGA_COLOR_BLACK << 4);
    if (read_line(line, sizeof(line)) == 0) continue;
    remember(line);

    strcpy(work, line);
    int argc = split_args(work, argv);
    if (argc == 0) continue;
    if (strcmp(argv[0], "exit") == 0) return;

    const command_t *cmd = find_command(argv[0]);
    if (cmd && cmd->run) cmd->run(argc, argv);
    else run_program(argc, argv);
  }
}
