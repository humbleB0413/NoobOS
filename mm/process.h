#ifndef PROCESS_H
#define PROCESS_H

#define PROCESS_MAX 8
#define PROCESS_STACK_SIZE 8192
#define PROCESS_NAME_LEN 16
/* 타이머 tick(1ms) 몇 번마다 다음 프로세스로 넘길지 */
#define PROCESS_QUANTUM_TICKS 10

/* 프로세스 상태 */
#define PROCESS_UNUSED 0   /* 빈 슬롯 */
#define PROCESS_RUN    1   /* 실행 중이거나 실행 가능 */
#define PROCESS_SLEEP  2   /* wake_tick 까지 잠듦 (sleep_queue 에 있음) */

#include <stdint.h>
#include "list.h"

typedef struct
{
    uint32_t pid;
    uint32_t state;
    uint32_t sp;                        /* 전환 시점의 esp — 레지스터는 전부 이 스택 위에 있음 */
    uint64_t wake_tick;                 /* PROCESS_SLEEP 일 때 깨어날 tick */
    uint64_t cpu_ticks;                 /* 이 프로세스가 실행 중일 때 지나간 타이머 tick 수 */
    list_node_t sleep_node;             /* wake_tick 오름차순 sleep_queue 연결 */
    char name[PROCESS_NAME_LEN];
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

/* ps 출력용 스냅샷 */
typedef struct{
    uint32_t pid;
    uint32_t state;
    uint64_t cpu_ticks;
    char name[PROCESS_NAME_LEN];
} process_info_t;

void init_scheduling(void);
process_t* create_process(uint32_t pc, const char* name);
void yield(void);
/* 타이머 IRQ 가 매 tick(EOI 이후) 호출 — CPU 시간 집계 + quantum 이 끝나면 yield */
void schedule_tick(void);
int scheduler_running(void);

uint32_t process_getpid(void);
void process_sleep(uint32_t ms);
__attribute__((noreturn)) void process_exit(void);
/* 0 성공, -1 없는 pid 이거나 죽일 수 없는 프로세스(pid 0 / idle) */
int process_kill(uint32_t pid);
int process_alive(uint32_t pid);
/* pid 가 끝날 때까지 잠들며 기다린다 */
void process_wait(uint32_t pid);
int process_snapshot(process_info_t* out, int max);

__attribute__((naked)) void switch_context(uint32_t* prev_sp, uint32_t* next_sp);

#endif
