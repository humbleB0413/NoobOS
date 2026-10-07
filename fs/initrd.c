#include "vfs.h"
#include "string.h"
#include "multiboot.h"
#include "linux/vga.h"
#include "../mm/vmm.h"

#define TAR_BLOCK 512

/* POSIX ustar 헤더 (512 바이트 블록의 앞부분) */
typedef struct {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];   /* 8진수 ASCII */
  char mtime[12];
  char checksum[8];
  char typeflag;   /* '0' 또는 '\0' = 파일, '5' = 디렉터리 */
  char linkname[100];
  char magic[6];   /* "ustar" */
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
} __attribute__((packed)) tar_header_t;

static uint32_t parse_octal(const char *s, int len) {
  uint32_t value = 0;
  for (int i = 0; i < len && s[i] >= '0' && s[i] <= '7'; i++) {
    value = value * 8 + (s[i] - '0');
  }
  return value;
}

/* initrd 는 메모리에 통째로 올라와 있으므로 읽기는 단순 복사 */
static int initrd_read(vfs_node_t *node, uint32_t offset, void *buf, uint32_t len) {
  memcpy(buf, (const uint8_t *)node->fs_data + offset, len);
  return (int)len;
}

static const vfs_ops_t initrd_ops = {.read = initrd_read};

static int parse_tar(const uint8_t *image, uint32_t size) {
  int files = 0;
  uint32_t offset = 0;

  while (offset + TAR_BLOCK <= size) {
    const tar_header_t *h = (const tar_header_t *)(image + offset);
    if (h->name[0] == '\0') break;  /* 아카이브 끝의 빈 블록 */
    if (memcmp(h->magic, "ustar", 5) != 0) {
      kprintf("initrd: bad tar header at offset %u\n", offset);
      return -1;
    }

    uint32_t file_size = parse_octal(h->size, sizeof(h->size));
    const uint8_t *data = image + offset + TAR_BLOCK;
    if (h->typeflag == '5') {
      vfs_add_node(h->name, VFS_DIR, 0, &initrd_ops, 0);
    } else if (h->typeflag == '0' || h->typeflag == '\0') {
      if (vfs_add_node(h->name, VFS_FILE, file_size, &initrd_ops, (void *)data)) files++;
    }
    offset += TAR_BLOCK + ((file_size + TAR_BLOCK - 1) / TAR_BLOCK) * TAR_BLOCK;
  }
  return files;
}

int init_initrd(unsigned long mbi_address) {
  multiboot_info_t *mbi = (multiboot_info_t *)mbi_address;
  if (!(mbi->flags & MULTIBOOT_INFO_MODS) || mbi->mods_count == 0) {
    kprintf("initrd: no boot module\n");
    return -1;
  }

  multiboot_module_t *mod = (multiboot_module_t *)mbi->mods_addr;
  uint32_t size = mod->mod_end - mod->mod_start;
  /* 모듈이 identity 매핑(첫 4MB) 밖에 놓일 수도 있으므로 위치와 무관하게 힙 가상 주소로 매핑해서 쓴다 */
  const uint8_t *image = vmm_map_physical(mod->mod_start, size);
  if (!image) {
    kprintf("initrd: cannot map module (%u bytes)\n", size);
    return -1;
  }
  int files = parse_tar(image, size);
  kprintf("initrd: %d files (%u bytes at phys 0x%x)\n", files, size, mod->mod_start);
  return files;
}
