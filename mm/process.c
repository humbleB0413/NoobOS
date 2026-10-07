#include "process.h"
#include "string.h"

#define KERNEL_CODE_SELECTOR 0x08
#define EFLAGS_IF            0x200
#define EFLAGS_RESERVED      0x002

static process_t processes[PROCESS_MAX];
static uint32_t current = 0;
static uint32_t next_pid = 0;
static volatile uint32_t scheduler_ready = 0;

static void process_trap_return(void);
static void process_exit(void);

static inline uint32_t irq_save(void){
    uint32_t flags;
    __asm__ volatile("pushf; pop %0; cli" : "=r"(flags) :: "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags){
    if(flags & EFLAGS_IF){
        __asm__ volatile("sti" ::: "memory");
    }
}

/*
 * 현재 실행 흐름(kernel_main, 부팅 스택)을 processes[0] 으로 등록한다.
 * 첫 yield 때 부팅 스택의 esp 가 processes[0].sp 에 저장되므로 별도의 초기 프레임이 필요 없다.
 */
void init_scheduling(void){
    uint32_t flags = irq_save();

    memset(processes, 0, sizeof(processes));
    processes[0].pid = next_pid++;
    processes[0].state = PROCESS_RUN;
    current = 0;
    scheduler_ready = 1;

    irq_restore(flags);
}

/*
 * 새 프로세스의 스택을 "이미 타이머 인터럽트로 멈춘 상태"처럼 꾸민다.
 *
 *   stack + SIZE -> | process_exit      |  pc 함수가 return 하면 여기로
 *                   | trap frame        |  eflags/cs/eip, err_code, int_no, pushal (useresp/ss 제외)
 *                   | switch frame      |  edi esi ebx ebp, eip = process_trap_return
 *   proc->sp     -> +-------------------+
 *
 * 첫 switch_context 의 ret -> process_trap_return -> popal; add $8; iret -> IF=1 로 pc 에서 시작.
 */
process_t* create_process(uint32_t pc){
    process_t* proc = 0;
    uint32_t flags = irq_save();

    for(int i = 0; i < PROCESS_MAX; i++){
        if(processes[i].state == PROCESS_IDLE){
            proc = &processes[i];
            break;
        }
    }

    if(!proc){
        irq_restore(flags);
        return 0;
    }

    uint32_t* top = (uint32_t*)(proc->stack + PROCESS_STACK_SIZE);
    *--top = (uint32_t)process_exit;

    /* ring0 -> ring0 iret 은 useresp/ss 를 pop 하지 않으므로 마지막 두 필드는 스택에 두지 않는다 */
    context_t* tf = (context_t*)((uint8_t*)top - (sizeof(context_t) - 2 * sizeof(uint32_t)));
    memset(tf, 0, sizeof(context_t) - 2 * sizeof(uint32_t));
    tf->eip = pc;
    tf->cs = KERNEL_CODE_SELECTOR;
    tf->eflags = EFLAGS_IF | EFLAGS_RESERVED;

    switch_frame_t* sf = (switch_frame_t*)tf - 1;
    memset(sf, 0, sizeof(*sf));
    sf->eip = (uint32_t)process_trap_return;

    proc->sp = (uint32_t)sf;
    proc->pid = next_pid++;
    proc->state = PROCESS_RUN;

    irq_restore(flags);
    return proc;
}

/*
 * 라운드 로빈으로 다음 RUN 프로세스에 CPU 를 넘긴다.
 * 타이머 IRQ 핸들러(EOI 이후)에서 호출되며, 일반 코드에서 불러도 되도록 인터럽트를 막고 전환한다.
 */
void yield(void){
    if(!scheduler_ready){
        return;
    }

    uint32_t flags = irq_save();
    uint32_t prev = current;
    uint32_t next = prev;

    for(int i = 1; i <= PROCESS_MAX; i++){
        uint32_t candidate = (prev + i) % PROCESS_MAX;
        if(processes[candidate].state == PROCESS_RUN){
            next = candidate;
            break;
        }
    }

    if(next != prev){
        current = next;
        switch_context(&processes[prev].sp, &processes[next].sp);
    }

    irq_restore(flags);
}

/* cdecl: 4(%esp) = prev_sp, 8(%esp) = next_sp */
__attribute__((naked)) void switch_context(__attribute__((unused)) uint32_t* prev_sp, __attribute__((unused)) uint32_t* next_sp){
    __asm__ volatile(
        "mov 4(%esp), %eax\n"
        "mov 8(%esp), %edx\n"
        "push %ebp\n"
        "push %ebx\n"
        "push %esi\n"
        "push %edi\n"
        "mov %esp, (%eax)\n"
        "mov (%edx), %esp\n"
        "pop %edi\n"
        "pop %esi\n"
        "pop %ebx\n"
        "pop %ebp\n"
        "ret\n"
    );
}

/* 새 프로세스의 첫 진입점: timer stub 의 복귀 경로와 동일하게 trap frame 을 풀고 iret */
__attribute__((naked)) static void process_trap_return(void){
    __asm__ volatile(
        "popal\n"
        "add $8, %esp\n"
        "iret\n"
    );
}

/* 프로세스 함수가 return 하면 도착. 슬롯을 비우고 다시는 돌아오지 않는다 */
static void process_exit(void){
    irq_save();
    processes[current].state = PROCESS_IDLE;
    yield();
    while(1){
        __asm__ volatile("hlt");
    }
}
