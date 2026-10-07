#include "pmm.h"

#include "string.h"
#include "linux/spinlock.h"
#define ERROR_CODE 0xFFFFFFFF

static uint32_t pmm_bitmap[PMM_MAX_FRAME_SIZE / 32];
extern char _end;

/* [start, end) 를 덮는 모든 프레임을 사용 중으로 표시 */
static void reserve_range(uint32_t start, uint32_t end) {
  for (uint32_t frame = start / PAGE_SIZE; frame < (end + PAGE_SIZE - 1) / PAGE_SIZE; frame++) {
    if (frame < PMM_MAX_FRAME_SIZE) {
      pmm_bitmap[frame / 32] |= (1u << (frame % 32));
    }
  }
}

void init_pmm(unsigned long mbi_address) {
  // bitmap 초기화
  memset(pmm_bitmap, 0xFF, sizeof(pmm_bitmap));

  multiboot_info_t *mbi = (multiboot_info_t *)mbi_address;
  multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mbi->mmap_addr;

  while ((uint32_t)mmap < mbi->mmap_addr + mbi->mmap_length) {
    if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
      for (uint64_t off = 0; off < mmap->len; off += PAGE_SIZE) {
        uint32_t current_page = (mmap->addr + off) >> 12;
        if (current_page < PMM_MAX_FRAME_SIZE) {
          pmm_bitmap[current_page / 32] &= ~(1u << (current_page % 32));
        }
      }
    }

    mmap = (multiboot_memory_map_t *)((uint32_t)mmap + mmap->size +
                                      sizeof(mmap->size));
  }

  reserve_range(0x100000, (uint32_t)&_end);

  /* 첫 1MB 전체(BIOS 데이터, Multiboot 정보 구조체, VGA 메모리 0xA0000~)를 사용 중으로 둔다.
   * Multiboot 정보는 부팅 후에도(initrd 위치 등) 읽어야 하므로 프레임이 재사용되면 안 된다 */
  reserve_range(0, 0x100000);

  /* 부트 모듈(initrd)은 커널 뒤 어딘가에 올라와 있는데 mmap 에는 그냥 AVAILABLE 로 보고된다 */
  if (mbi->flags & MULTIBOOT_INFO_MODS) {
    multiboot_module_t *mods = (multiboot_module_t *)mbi->mods_addr;
    for (uint32_t i = 0; i < mbi->mods_count; i++) {
      reserve_range(mods[i].mod_start, mods[i].mod_end);
    }
  }

  return;
}

static spinlock_t pmm_lock = SPINLOCK_INIT("pmm_bitmap");

static uint32_t alloc_frame_unlocked(uint32_t count, allocated_frame_info_t *info) {
  if (count > PMM_MAX_FRAMES_COUNT || count <= 0) return ERROR_CODE;

  memset(info, 0, sizeof(*info));

  for (int i = 0; i < PMM_MAX_FRAME_SIZE / 32 && info->count < count; i++) {
    if (pmm_bitmap[i] >= 0xFFFFFFFF) {
      continue;
    }
    for (int j = 0; j < 32 && info->count < count; j++) {
      if (!(pmm_bitmap[i] & (1u << j))) {
        info->frames[info->count++] = 32 * i + j;
      }
    }
  }

  if (info->count < count)
    return ERROR_CODE;
  else {
    for (uint32_t frame = 0; frame < info->count; frame++) {
      pmm_bitmap[info->frames[frame] / 32] |=
          (1u << (info->frames[frame] % 32));
    }

    return info->count;
  }
}

uint32_t alloc_frame(uint32_t count, allocated_frame_info_t *info) {
  spin_lock(&pmm_lock);
  uint32_t result = alloc_frame_unlocked(count, info);
  spin_unlock(&pmm_lock);
  return result;
}

static uint32_t alloc_frame_contiguous_unlocked(uint32_t count, allocated_frame_info_t *info) {
  if (count > PMM_MAX_FRAMES_COUNT || count == 0) return ERROR_CODE;

  memset(info, 0, sizeof(*info));

  uint32_t run_start = 0;
  uint32_t run_len = 0;

  for (uint32_t frame = 0; frame < PMM_MAX_FRAME_SIZE; frame++) {
    if (run_len == 0 && pmm_bitmap[frame / 32] == 0xFFFFFFFF) {
      frame += 31;  // skip a fully-used word at once
      continue;
    }

    if (pmm_bitmap[frame / 32] & (1u << (frame % 32))) {
      run_len = 0;
      continue;
    }

    if (run_len == 0) run_start = frame;

    if (++run_len == count) {
      for (uint32_t i = 0; i < count; i++) {
        uint32_t f = run_start + i;
        pmm_bitmap[f / 32] |= (1u << (f % 32));
        info->frames[i] = f;
      }
      info->count = count;
      return count;
    }
  }

  return ERROR_CODE;
}

uint32_t alloc_frame_contiguous(uint32_t count, allocated_frame_info_t *info) {
  spin_lock(&pmm_lock);
  uint32_t result = alloc_frame_contiguous_unlocked(count, info);
  spin_unlock(&pmm_lock);
  return result;
}

void free_frame(allocated_frame_info_t *info) {
  spin_lock(&pmm_lock);
  for (uint32_t i = 0; i < info->count; i++) {
    uint32_t frame = info->frames[i];
    if (pmm_bitmap[frame / 32] & (1u << (frame % 32))) {
      pmm_bitmap[frame / 32] &= ~(1u << (frame % 32));
    }
  }
  spin_unlock(&pmm_lock);
}

#if 1
#include "../include/linux/vga.h"

uint32_t pmm_free_count() {
  uint32_t free_count = 0;
  for (int i = 0; i < PMM_MAX_FRAME_SIZE / 32; i++) {
    for (int j = 0; j < 32; j++) {
      if (!(pmm_bitmap[i] & (1u << j))) {
        free_count++;
      }
    }
  }
  return free_count;
}

void print_frame_state() {
  terminal_initialize();

  uint32_t free_count = pmm_free_count();
  kprintf("free: %u / %u frames (%u KB)\n\n", free_count,
          (uint32_t)(PMM_MAX_FRAME_SIZE), free_count * (PAGE_SIZE / 1024));

  /* 저수준 메모리 ~ 커널 이미지 경계 구간만 출력 (화면 한 번에 들어오는 분량)
   */
  for (int i = 0; i < 20; i++) {
    kprintf("word %d (frame %d-%d): %b\n", i, i * 32, i * 32 + 31,
            pmm_bitmap[i]);
  }
}
#endif