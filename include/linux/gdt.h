#pragma once

#include <stdint.h>

typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint16_t base_1;
    uint8_t  base_2;
    uint8_t  access;
    uint8_t  limit_flag;
    uint8_t  base_3;
} gdtr_t;

typedef struct __attribute__((packed))
{
    uint16_t limit;
    uint32_t address;
} gdtr_descriptor_t;

typedef struct __attribute__((packed))  {
	uint32_t reserved0;   /* prev_tss — 하드웨어 스위칭 안 쓰므로 항상 0 */
    uint32_t esp0;        /* Ring0 진입 시 커널 스택 포인터 ← 유일한 핵심 */
    uint32_t ss0;         /* Ring0 스택 세그먼트 (보통 KERNEL_DS, 고정) */
    uint32_t reserved1[23]; /* esp1~ldt — 사용 안 함, 0으로 채움 */
    uint16_t reserved2;
    uint16_t iomap_base;  /* IOPB 없으면 sizeof(tss)로 설정 → 전부 차단 */
} tss_entry_t;

extern void asm_load_gdt(void*);
void init_gdt();
void set_gdt_descriptor(gdtr_t* gdt, uint32_t base, uint32_t limit, uint8_t access, uint8_t flag);