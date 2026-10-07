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

/*
 * 32비트 TSS 전체 배치. 소프트웨어 컨텍스트 스위칭을 쓰므로 평소엔 esp0/ss0 만 의미가 있지만,
 * double fault 는 하드웨어 task switch 로 처리하기 때문에 CPU 가 나머지 필드에 이전 상태를 저장/복원한다.
 */
typedef struct __attribute__((packed))  {
    uint32_t prev_tss;    /* task switch 시 CPU 가 이전 TSS 셀렉터를 기록 */
    uint32_t esp0;        /* Ring0 진입 시 커널 스택 포인터 */
    uint32_t ss0;         /* Ring0 스택 세그먼트 (KERNEL_DS 고정) */
    uint32_t esp1, ss1, esp2, ss2;
    uint32_t cr3;
    uint32_t eip, eflags;
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;  /* IOPB 없으면 sizeof(tss)로 설정 → 전부 차단 */
} tss_entry_t;

#define GDT_KERNEL_CODE 0x08
#define GDT_KERNEL_DATA 0x10
#define GDT_USER_CODE   0x18
#define GDT_USER_DATA   0x20
#define GDT_TSS         0x28
#define GDT_DF_TSS      0x30

extern void asm_load_gdt(void*);
void init_gdt();
/* 유저 모드에서 인터럽트가 들어오면 CPU 가 이 스택으로 갈아탄다 — 프로세스 전환 때마다 갱신 */
void tss_set_kernel_stack(uint32_t esp0);
/* 페이징이 켜진 뒤(CR3 확정 후)에 호출: #DF 를 별도 스택의 하드웨어 태스크로 받는다 */
void init_double_fault_task(void);
void set_gdt_descriptor(gdtr_t* gdt, uint32_t base, uint32_t limit, uint8_t access, uint8_t flag);