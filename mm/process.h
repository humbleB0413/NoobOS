#ifndef PROCESS_H
#define PROCESS_H

#define PROCESS_MAX 8
#define PROCESS_STACK_SIZE 8192
#define PROCESS_NAME_LEN 16
/* 타이머 tick(1ms) 몇 번마다 다음 프로세스로 넘길지 */
#define PROCESS_QUANTUM_TICKS 10
/* fd 0~2 는 콘솔로 예약, 3 부터 파일 */
#define PROCESS_MAX_FILES 8
#define PROCESS_FIRST_FILE_FD 3

/* 프로세스 상태 */
#define PROCESS_UNUSED 0   /* 빈 슬롯 */
#define PROCESS_RUN    1   /* 실행 중이거나 실행 가능 */
#define PROCESS_SLEEP  2   /* wake_tick 까지 잠듦 (sleep_queue 에 있음) */

#include <stdint.h>
#include "list.h"
#include "vmm.h"
#include "../fs/vfs.h"

/* process_wait() 이 돌려주는 특수 종료 코드 */
#define PROCESS_EXIT_KILLED  (-9)    /* kill 로 종료 */
#define PROCESS_EXIT_FAULT   (-11)   /* 유저 모드 예외(#PF, #GP 등)로 종료 */
#define PROCESS_EXIT_UNKNOWN (-128)  /* 종료 기록이 이미 덮어써짐 */

/*
 * 페이지 정렬된 구조체 맨 앞에 가드 페이지(매핑 해제)와 커널 스택을 둔다. 스택은 아래로 자라므로
 * 넘치면 곧바로 가드 페이지를 건드려 fault 가 나고, 옆 슬롯의 메타데이터를 조용히 덮어쓰지 않는다.
 */
typedef struct __attribute__((aligned(4096)))
{
    uint8_t guard[4096];
    uint8_t stack[PROCESS_STACK_SIZE];
    uint32_t pid;
    uint32_t state;
    uint32_t sp;                        /* 전환 시점의 esp — 레지스터는 전부 이 스택 위에 있음 */
    uint64_t wake_tick;                 /* PROCESS_SLEEP 일 때 깨어날 tick */
    uint64_t cpu_ticks;                 /* 이 프로세스가 실행 중일 때 지나간 타이머 tick 수 */
    list_node_t sleep_node;             /* wake_tick 오름차순 sleep_queue 연결 */
    char name[PROCESS_NAME_LEN];
    address_space_t* as;                /* 유저 프로세스의 주소 공간, 커널 프로세스는 NULL */
    uint32_t cr3;                       /* 전환 시 로드할 페이지 디렉터리 물리 주소 */
    struct {
        vfs_node_t* node;               /* NULL 이면 빈 fd */
        uint32_t offset;
    } files[PROCESS_MAX_FILES];
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
/* as 를 넘겨받아 ring3 의 entry 에서 user_esp 스택으로 시작한다. 실패 시 NULL (as 는 호출자가 정리) */
process_t* create_user_process(const char* name, address_space_t* as, uint32_t entry, uint32_t user_esp);
address_space_t* process_current_as(void);
process_t* process_current(void);
void yield(void);
/* 타이머 IRQ 가 매 tick(EOI 이후) 호출 — CPU 시간 집계 + quantum 이 끝나면 yield */
void schedule_tick(void);
int scheduler_running(void);

uint32_t process_getpid(void);
void process_sleep(uint32_t ms);
__attribute__((noreturn)) void process_exit(int code);
/* 0 성공, -1 없는 pid 이거나 죽일 수 없는 프로세스(pid 0 / idle) */
int process_kill(uint32_t pid);
int process_alive(uint32_t pid);
/* pid 가 끝날 때까지 잠들며 기다리고 종료 코드를 돌려준다 */
int process_wait(uint32_t pid);
int process_snapshot(process_info_t* out, int max);
/* addr 가 어떤 프로세스의 스택 가드 페이지면 "pid N (name)" 문자열, 아니면 NULL */
const char* process_guard_owner(uint32_t addr);

__attribute__((naked)) void switch_context(uint32_t* prev_sp, uint32_t* next_sp);

#endif
