#include "pmm.h"

#include "string.h"

static uint32_t pmm_bitmap[PMM_MAX_FRAME_SIZE / 32];
extern char _end;

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

  for (uint32_t m = 0x100000; m < (uint32_t)&_end; m += PAGE_SIZE) {
    uint32_t current_page = m >> 12;
    if (current_page < PMM_MAX_FRAME_SIZE) {
      pmm_bitmap[current_page / 32] |= (1u << (current_page % 32));
    }
  }
}

uint32_t alloc_frame(){
  for(int i = 0; i < PMM_MAX_FRAME_SIZE/32; i++){
    if(pmm_bitmap[i] >= 0xFFFFFFFF){
      continue;
    }
    else{
      for(int j = 0; j < 32; j++){
        if(!(pmm_bitmap[i] & (1u << j))){
          pmm_bitmap[i] |= (1u << j);
          return i*32 + j;
        }
      }
    }
  }

  return 0;
}

void free_frame(uint32_t frame){
  if(pmm_bitmap[frame/32] & (1u << (frame % 32))){
    pmm_bitmap[frame/32] &= ~(1u << (frame % 32));
  }
}

#if 1
#include "../include/linux/vga.h"

uint32_t pmm_free_count(){
  uint32_t free_count = 0;
  for(int i = 0; i < PMM_MAX_FRAME_SIZE/32; i++){
    for(int j = 0; j < 32; j++){
      if(!(pmm_bitmap[i] & (1u << j))){
        free_count++;
      }
    }
  }
  return free_count;
}

void print_frame_state(){
  terminal_initialize();

  uint32_t free_count = pmm_free_count();
  kprintf("free: %u / %u frames (%u KB)\n\n",
          free_count, (uint32_t)(PMM_MAX_FRAME_SIZE), free_count * (PAGE_SIZE / 1024));

  /* 저수준 메모리 ~ 커널 이미지 경계 구간만 출력 (화면 한 번에 들어오는 분량) */
  for(int i = 0; i < 20; i++){
    kprintf("word %d (frame %d-%d): %b\n", i, i*32, i*32+31, pmm_bitmap[i]);
  }
}
#endif