#include "process.h"
#include "string.h"
#include "linux/spinlock.h"
#include "linux/panic.h"
#include "../driver/timer.h"
#include "vmm.h"

#define KERNEL_CODE_SELECTOR 0x08
#define EFLAGS_RESERVED      0x002

static process_t processes[PROCESS_MAX];
static uint32_t current = 0;
static uint32_t idle_index = 0;
static uint32_t next_pid = 0;
static uint32_t slice_ticks = 0;
static volatile uint32_t scheduler_ready = 0;
/* 잠든 프로세스들 — wake_tick 오름차순이라 앞에서부터 깨우다 아직 이른 것을 만나면 멈추면 된다 */
static list_node_t sleep_queue = LIST_HEAD_INIT(sleep_queue);

static void process_trap_return(void);
static void process_return_trampoline(void);

static void idle_task(void){
    while(1){
        __asm__ volatile("sti; hlt");
    }
}

static void set_name(process_t* proc, const char* name){
    strncpy(proc->name, name ? name : "?", PROCESS_NAME_LEN - 1);
    proc->name[PROCESS_NAME_LEN - 1] = '\0';
}

static process_t* find_by_pid(uint32_t pid){
    for(int i = 0; i < PROCESS_MAX; i++){
        if(processes[i].state != PROCESS_UNUSED && processes[i].pid == pid){
            return &processes[i];
        }
    }
    return 0;
}

/*
 * 현재 실행 흐름(kernel_main, 부팅 스택)을 processes[0] 으로 등록하고, 실행할 프로세스가
 * 하나도 없을 때 CPU 를 받을 idle 프로세스를 만든다.
 * 첫 yield 때 부팅 스택의 esp 가 processes[0].sp 에 저장되므로 pid 0 은 별도의 초기 프레임이 필요 없다.
 */
void init_scheduling(void){
    uint32_t flags = irq_save();

    memset(processes, 0, sizeof(processes));
    list_init(&sleep_queue);
    for(int i = 0; i < PROCESS_MAX; i++){
        vmm_unmap_identity_page((uint32_t)processes[i].guard);
    }
    processes[0].pid = next_pid++;
    processes[0].state = PROCESS_RUN;
    set_name(&processes[0], "kernel_main");
    current = 0;

    process_t* idle = create_process((uint32_t)idle_task, "idle");
    KASSERT(idle != 0);
    idle_index = idle - processes;
    scheduler_ready = 1;

    irq_restore(flags);
}

int scheduler_running(void){
    return scheduler_ready;
}

/*
 * 새 프로세스의 스택을 "이미 타이머 인터럽트로 멈춘 상태"처럼 꾸민다.
 *
 *   stack + SIZE -> | return trampoline |  pc 함수가 return 하면 여기로
 *                   | trap frame        |  eflags/cs/eip, err_code, int_no, pushal (useresp/ss 제외)
 *                   | switch frame      |  edi esi ebx ebp, eip = process_trap_return
 *   proc->sp     -> +-------------------+
 *
 * 첫 switch_context 의 ret -> process_trap_return -> popal; add $8; iret -> IF=1 로 pc 에서 시작.
 */
process_t* create_process(uint32_t pc, const char* name){
    process_t* proc = 0;
    uint32_t flags = irq_save();

    for(int i = 0; i < PROCESS_MAX; i++){
        if(processes[i].state == PROCESS_UNUSED){
            proc = &processes[i];
            break;
        }
    }

    if(!proc){
        irq_restore(flags);
        return 0;
    }

    memset(&proc->pid, 0, sizeof(*proc) - offsetof(process_t, pid));
    list_init(&proc->sleep_node);

    uint32_t* top = (uint32_t*)(proc->stack + PROCESS_STACK_SIZE);
    *--top = (uint32_t)process_return_trampoline;

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
    set_name(proc, name);
    proc->state = PROCESS_RUN;

    irq_restore(flags);
    return proc;
}

static void wake_sleepers(void){
    uint64_t now = get_ticks();
    while(!list_empty(&sleep_queue)){
        process_t* proc = list_entry(sleep_queue.next, process_t, sleep_node);
        if(proc->wake_tick > now){
            break;
        }
        list_remove(&proc->sleep_node);
        proc->state = PROCESS_RUN;
    }
}

/*
 * 현재 다음 순서부터 RUN 상태인 프로세스를 라운드 로빈으로 고른다.
 * idle 은 다른 후보가 하나도 없을 때만 고른다.
 */
static uint32_t pick_next(void){
    for(int i = 1; i <= PROCESS_MAX; i++){
        uint32_t candidate = (current + i) % PROCESS_MAX;
        if(candidate != idle_index && processes[candidate].state == PROCESS_RUN){
            return candidate;
        }
    }
    return idle_index;
}

/*
 * 다음 RUN 프로세스에 CPU 를 넘긴다. 타이머 IRQ(schedule_tick) 와 sleep/exit 경로에서 불리며,
 * 일반 코드에서 불러도 되도록 인터럽트를 막고 전환한다.
 */
void yield(void){
    if(!scheduler_ready){
        return;
    }

    uint32_t flags = irq_save();
    uint32_t prev = current;

    wake_sleepers();
    uint32_t next = pick_next();
    /* 아무도 없는데 지금 프로세스가 계속 실행 가능하면 idle 로 넘어가지 않고 그대로 간다 */
    if(next == idle_index && prev != idle_index && processes[prev].state == PROCESS_RUN){
        next = prev;
    }

    slice_ticks = 0;
    if(next != prev){
        current = next;
        switch_context(&processes[prev].sp, &processes[next].sp);
    }

    irq_restore(flags);
}

void schedule_tick(void){
    if(!scheduler_ready){
        return;
    }
    processes[current].cpu_ticks++;
    /* idle 은 잠든 프로세스가 깨어나는 즉시 양보해야 하므로 매 tick 확인한다 */
    if(current == idle_index || ++slice_ticks >= PROCESS_QUANTUM_TICKS){
        yield();
    }
}

uint32_t process_getpid(void){
    return processes[current].pid;
}

void process_sleep(uint32_t ms){
    if(!scheduler_ready){
        return;
    }
    uint32_t flags = irq_save();
    process_t* self = &processes[current];
    self->wake_tick = get_ticks() + ms;
    self->state = PROCESS_SLEEP;

    list_node_t* pos;
    list_for_each(pos, &sleep_queue){
        if(list_entry(pos, process_t, sleep_node)->wake_tick > self->wake_tick){
            break;
        }
    }
    /* pos 앞에 끼워 넣어 오름차순 유지 (pos == head 면 맨 뒤) */
    list_insert_between(&self->sleep_node, pos->prev, pos);

    yield();
    irq_restore(flags);
}

static void release_slot(process_t* proc){
    if(proc->state == PROCESS_SLEEP){
        list_remove(&proc->sleep_node);
    }
    proc->state = PROCESS_UNUSED;
}

/* 슬롯을 비우고 다른 프로세스로 넘어간다. 다시는 돌아오지 않는다 */
void process_exit(void){
    irq_save();
    KASSERT(current != 0 && current != idle_index);
    release_slot(&processes[current]);
    yield();
    kpanic("exited process resumed");
}

int process_kill(uint32_t pid){
    uint32_t flags = irq_save();
    process_t* proc = find_by_pid(pid);
    if(!proc || proc == &processes[0] || proc == &processes[idle_index]){
        irq_restore(flags);
        return -1;
    }
    if(proc == &processes[current]){
        process_exit();
    }
    /* 다른 프로세스는 지금 switch_context 안에 멈춰 있으므로 슬롯만 비우면 다시 선택되지 않는다 */
    release_slot(proc);
    irq_restore(flags);
    return 0;
}

int process_alive(uint32_t pid){
    return find_by_pid(pid) != 0;
}

void process_wait(uint32_t pid){
    while(process_alive(pid)){
        process_sleep(10);
    }
}

int process_snapshot(process_info_t* out, int max){
    int n = 0;
    uint32_t flags = irq_save();
    for(int i = 0; i < PROCESS_MAX && n < max; i++){
        if(processes[i].state == PROCESS_UNUSED){
            continue;
        }
        out[n].pid = processes[i].pid;
        out[n].state = processes[i].state;
        out[n].cpu_ticks = processes[i].cpu_ticks;
        memcpy(out[n].name, processes[i].name, PROCESS_NAME_LEN);
        n++;
    }
    irq_restore(flags);
    return n;
}

const char* process_guard_owner(uint32_t addr){
    static char desc[48];
    for(int i = 0; i < PROCESS_MAX; i++){
        uint32_t guard = (uint32_t)processes[i].guard;
        if(addr >= guard && addr < guard + sizeof(processes[i].guard)){
            /* kprintf 계열이 없어 직접 조립: "pid N (name)" */
            char num[12];
            int len = 0;
            uint32_t pid = processes[i].pid;
            do { num[len++] = '0' + pid % 10; pid /= 10; } while(pid);
            int o = 0;
            for(const char* p = "pid "; *p; p++) desc[o++] = *p;
            while(len) desc[o++] = num[--len];
            desc[o++] = ' ';
            desc[o++] = '(';
            for(int k = 0; k < PROCESS_NAME_LEN && processes[i].name[k]; k++) desc[o++] = processes[i].name[k];
            desc[o++] = ')';
            desc[o] = '\0';
            return desc;
        }
    }
    return 0;
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

/* 프로세스 함수가 return 하면 도착 */
static void process_return_trampoline(void){
    process_exit();
}
