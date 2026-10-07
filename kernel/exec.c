#include "linux/exec.h"
#include "linux/vga.h"
#include "elf.h"
#include "string.h"
#include "uapi/syscall_nr.h"
#include "../fs/vfs.h"
#include "../mm/vmm.h"
#include "../mm/process.h"

#define E_NOMEM (-12)

static int elf_valid(const elf32_header_t *h, uint32_t size) {
  return size >= sizeof(*h) && h->e_magic == ELF_MAGIC && h->e_class == ELFCLASS32 &&
         h->e_data == ELFDATA2LSB && h->e_type == ET_EXEC && h->e_machine == EM_386 &&
         h->e_phentsize == sizeof(elf32_phdr_t) &&
         h->e_phoff + (uint32_t)h->e_phnum * sizeof(elf32_phdr_t) <= size;
}

static int load_segments(address_space_t *as, const uint8_t *image, uint32_t size) {
  const elf32_header_t *h = (const elf32_header_t *)image;
  const elf32_phdr_t *ph = (const elf32_phdr_t *)(image + h->e_phoff);
  int loaded = 0;

  for (uint32_t i = 0; i < h->e_phnum; i++) {
    if (ph[i].p_type != PT_LOAD || ph[i].p_memsz == 0) continue;
    uint32_t start = ph[i].p_vaddr, end = start + ph[i].p_memsz;
    /* 세그먼트가 유저 영역 밖(커널 주소)을 요구하거나 파일 범위를 벗어나면 거부 */
    if (start < USER_SPACE_BASE || end > USER_STACK_LIMIT || end < start ||
        ph[i].p_filesz > ph[i].p_memsz || ph[i].p_offset + ph[i].p_filesz > size) {
      return E_INVAL;
    }
    if (vmm_map_user_range(as, start, ph[i].p_memsz, ph[i].p_flags & PF_W) < 0) return E_NOMEM;
    /* .bss 처럼 filesz < memsz 인 나머지는 vmm_map_user_range 가 이미 0 으로 채웠다 */
    vmm_copy_to_user(as, start, image + ph[i].p_offset, ph[i].p_filesz);
    loaded++;
  }
  return loaded > 0 ? 0 : E_INVAL;
}

/*
 * 유저 스택 맨 위에 인자 문자열들과 argv[] 를 만들고, crt0 의 _start 가 보는 초기 esp 를 돌려준다.
 *   esp -> | argc | argv | ... argv[0..argc-1], NULL ... | 문자열들 | USER_STACK_TOP
 */
static uint32_t build_user_stack(address_space_t *as, const char *argline) {
  char line[EXEC_ARGLINE_MAX];
  char *args[EXEC_MAX_ARGS];
  uint32_t argc = 0;

  strncpy(line, argline, sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';
  for (char *p = line; *p && argc < EXEC_MAX_ARGS;) {
    while (*p == ' ') *p++ = '\0';
    if (!*p) break;
    args[argc++] = p;
    while (*p && *p != ' ') p++;
  }

  /* 커널 쪽에서 스택 이미지를 만든 뒤 한 번에 복사한다 */
  uint8_t image[EXEC_ARGLINE_MAX + (EXEC_MAX_ARGS + 3) * sizeof(uint32_t) + 16];
  uint32_t used = 0;
  uint32_t uptr[EXEC_MAX_ARGS];

  for (int i = (int)argc - 1; i >= 0; i--) {
    uint32_t len = (uint32_t)strlen(args[i]) + 1;
    used += len;
    memcpy(image + sizeof(image) - used, args[i], len);
    uptr[i] = USER_STACK_TOP - used;
  }
  used = (used + 3) & ~3u;

  uint32_t words[EXEC_MAX_ARGS + 3];
  uint32_t nwords = 0;
  uint32_t argv_addr = USER_STACK_TOP - used - (argc + 1) * sizeof(uint32_t);
  words[nwords++] = argc;
  words[nwords++] = argv_addr;
  for (uint32_t i = 0; i < argc; i++) words[nwords++] = uptr[i];
  words[nwords++] = 0;

  used += nwords * sizeof(uint32_t);
  memcpy(image + sizeof(image) - used, words, nwords * sizeof(uint32_t));
  vmm_copy_to_user(as, USER_STACK_TOP - used, image + sizeof(image) - used, used);
  return USER_STACK_TOP - used;
}

int exec_user(const char *path, const char *argline) {
  vfs_node_t *node = vfs_lookup(path);
  if (!node || node->type != VFS_FILE) return E_NOENT;

  uint8_t *image = kmalloc(node->size);
  if (!image) return E_NOMEM;
  if (vfs_read(node, 0, image, node->size) != (int)node->size || !elf_valid((elf32_header_t *)image, node->size)) {
    kfree(image);
    return E_INVAL;
  }

  int err = E_NOMEM;
  address_space_t *as = vmm_create_address_space();
  if (as) {
    err = load_segments(as, image, node->size);
    if (err == 0 && vmm_map_user_range(as, USER_STACK_TOP - USER_STACK_PAGES * 4096, USER_STACK_PAGES * 4096, 1) < 0) {
      err = E_NOMEM;
    }
  }
  if (err == 0) {
    uint32_t entry = ((elf32_header_t *)image)->e_entry;
    uint32_t esp = build_user_stack(as, argline && *argline ? argline : path);
    /* 프로세스 이름은 경로의 마지막 부분 */
    const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;
    process_t *proc = create_user_process(name, as, entry, esp);
    kfree(image);
    if (!proc) {
      vmm_destroy_address_space(as);
      return E_NOMEM;
    }
    return (int)proc->pid;
  }

  vmm_destroy_address_space(as);
  kfree(image);
  return err;
}
