#ifndef PROCESS_H
#define PROCESS_H

#define PROCESS_MAX 8
#define PROCESS_RUN 1
#define PROCESS_IDLE 0
#define PROCESS_STACK_SIZE 8192

#include <stdint.h>

typedef struct
{
    uint32_t pid;
    uint32_t state;
    uint32_t sp;                        /* 전환 시점의 esp — 레지스터는 전부 이 스택 위에 있음 */
    uint8_t stack[PROCESS_STACK_SIZE];
} process_t;

/* 인터럽트 진입 시 스택에 쌓이는 trap frame (linux/idt.h 의 pt_regs 와 동일한 배치) */
typedef struct{
    /* pushad 저장 순서 */
    uint32_t edi, esi, ebp, esp;
    uint32_t ebx, edx, ecx, eax;
    /* 수동 push */
    uint32_t int_no;
    uint32_t err_code;
    /* CPU 자동 push */
    uint32_t eip, cs, eflags;
    /* 특권 레벨 전환 시만 */
    uint32_t useresp, ss;
} context_t;

/* switch_context 가 push/pop 하는 callee-saved 레지스터 + 복귀 주소 (pop 순서대로) */
typedef struct{
    uint32_t edi, esi, ebx, ebp;
    uint32_t eip;
} switch_frame_t;

void init_scheduling(void);
process_t* create_process(uint32_t pc);
void yield(void);
__attribute__((naked)) void switch_context(uint32_t* prev_sp, uint32_t* next_sp);

#endif
