#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "multiboot.h"

#define PAGE_SIZE 4096
#define PMM_MAX_FRAME_SIZE (4096 * 1024 / PAGE_SIZE * 1024)
#define PMM_MAX_FRAMES_COUNT 512   // 512 * 4KB = 2MB


typedef struct {
    uint32_t count;
    uint32_t frames[PMM_MAX_FRAMES_COUNT];  // 사용 가능한 물리주소
} allocated_frame_info_t; //max 2MB

void init_pmm(unsigned long mbi_address);
uint32_t alloc_frame(uint32_t count, allocated_frame_info_t *info);
uint32_t alloc_frame_contiguous(uint32_t count, allocated_frame_info_t *info);
void free_frame(allocated_frame_info_t *info);

#if 1
#include "../include/linux/vga.h"
uint32_t pmm_free_count();
void print_frame_state();
#endif

#endif