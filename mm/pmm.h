#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "multiboot.h"

#define PAGE_SIZE 4096
#define PMM_MAX_FRAME_SIZE (4096 * 1024 / PAGE_SIZE * 1024)

void init_pmm(unsigned long mbi_address);
uint32_t alloc_frame();
void free_frame(uint32_t frame);

#if 1
#include "../include/linux/vga.h"
uint32_t pmm_free_count();
void print_frame_state();
#endif

#endif