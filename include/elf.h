#pragma once

#include <stdint.h>

/* ELF32 실행 파일에서 로더가 실제로 읽는 부분만 정의 */
#define ELF_MAGIC     0x464C457Fu  /* "\x7FELF" (little endian) */
#define ELFCLASS32    1
#define ELFDATA2LSB   1
#define ET_EXEC       2
#define EM_386        3
#define PT_LOAD       1
#define PF_X          0x1
#define PF_W          0x2
#define PF_R          0x4

typedef struct {
  uint32_t e_magic;
  uint8_t e_class, e_data, e_version, e_osabi;
  uint8_t e_pad[8];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version2;
  uint32_t e_entry;
  uint32_t e_phoff;
  uint32_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
} __attribute__((packed)) elf32_header_t;

typedef struct {
  uint32_t p_type;
  uint32_t p_offset;
  uint32_t p_vaddr;
  uint32_t p_paddr;
  uint32_t p_filesz;
  uint32_t p_memsz;
  uint32_t p_flags;
  uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;
