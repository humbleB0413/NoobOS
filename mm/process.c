#include "process.h"
#include "string.h"
#include "linux/spinlock.h"
#include "linux/panic.h"
#include "../driver/timer.h"
#include "vmm.h"
#include "linux/gdt.h"

#define KERNEL_CODE_SELECTOR 0x08
#define USER_CODE_SELECTOR   (0x18 | 3)
#define USER_DATA_SELECTOR   (0x20 | 3)
#define EFLAGS_RESERVED      0x002
#define EXIT_RECORDS         16

static process_t processes[PROCESS_MAX];
static uint32_t current = 0;
static uint32_t idle_index = 0;
static uint32_t next_pid = 0;
static uint32_t slice_ticks = 0;
static volatile uint32_t scheduler_ready = 0;
/* 잠든 프로세스들 — wake_tick 오름차순이라 앞에서부터 깨우다 아직 이른 것을 만나면 멈추면 된다 */
static list_node_t sleep_queue = LIST_HEAD_INIT(sleep_queue);
/* 슬롯은 종료 즉시 재사용되므로 종료 코드는 따로 최근 몇 개만 기억해 둔다 */
static struct { uint32_t pid; int code; } exit_records[EXIT_RECORDS];
static uint32_t exit_record_next = 0;

static void process_trap_return(void);
static void user_trap_return(void);
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
    processes[0].cr3 = vmm_kernel_cr3();
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
static process_t* alloc_slot(const char* name){
    for(int i = 0; i < PROCESS_MAX; i++){
        process_t* proc = &processes[i];
        if(proc->state == PROCESS_UNUSED){
            memset(&proc->pid, 0, sizeof(*proc) - offsetof(process_t, pid));
            list_init(&proc->sleep_node);
            proc->pid = next_pid++;
            proc->cr3 = vmm_kernel_cr3();
            set_name(proc, name);
            return proc;
        }
    }
    return 0;
}

process_t* create_process(uint32_t pc, const char* name){
    uint32_t flags = irq_save();
    process_t* proc = alloc_slot(name);
    if(!proc){
        irq_restore(flags);
        return 0;
    }

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
    proc->state = PROCESS_RUN;

    irq_restore(flags);
    return proc;
}

/*
 * 유저 프로세스도 커널 프로세스와 같은 "인터럽트로 멈춘 척" 프레임을 쓰되, CPL 이 바뀌는 iret 이라
 * useresp/ss 까지 포함한 전체 trap frame 을 쌓는다. 커널 스택(stack[])은 이 프로세스가 유저 모드에서
 * 인터럽트/시스템 콜을 받을 때 TSS.esp0 으로 쓰인다.
 *
 *   stack + SIZE -> | ss, useresp       |
 *                   | eflags/cs/eip ... |  trap frame (전체)
 *                   | switch frame      |  eip = user_trap_return
 *   proc->sp     -> +-------------------+
 */
process_t* create_user_process(const char* name, address_space_t* as, uint32_t entry, uint32_t user_esp){
    uint32_t flags = irq_save();
    process_t* proc = alloc_slot(name);
    if(!proc){
        irq_restore(flags);
        return 0;
    }

    context_t* tf = (context_t*)(proc->stack + PROCESS_STACK_SIZE) - 1;
    memset(tf, 0, sizeof(*tf));
    tf->eip = entry;
    tf->cs = USER_CODE_SELECTOR;
    tf->eflags = EFLAGS_IF | EFLAGS_RESERVED;
    tf->useresp = user_esp;
    tf->ss = USER_DATA_SELECTOR;

    switch_frame_t* sf = (switch_frame_t*)tf - 1;
    memset(sf, 0, sizeof(*sf));
    sf->eip = (uint32_t)user_trap_return;

    proc->sp = (uint32_t)sf;
    proc->as = as;
    proc->cr3 = as->pd_phys;
    proc->state = PROCESS_RUN;

    irq_restore(flags);
    return proc;
}

address_space_t* process_current_as(void){
    return processes[current].as;
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
        /* 다음 프로세스가 유저 모드에서 인터럽트를 받으면 자기 커널 스택으로 들어오도록 */
        tss_set_kernel_stack((uint32_t)(processes[next].stack + PROCESS_STACK_SIZE));
        vmm_switch(processes[next].cr3);
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

static void record_exit(uint32_t pid, int code){
    exit_records[exit_record_next].pid = pid;
    exit_records[exit_record_next].code = code;
    exit_record_next = (exit_record_next + 1) % EXIT_RECORDS;
}

static void release_slot(process_t* proc, int code){
    if(proc->state == PROCESS_SLEEP){
        list_remove(&proc->sleep_node);
    }
    if(proc->as){
        /* 지금 이 주소 공간 위에서 실행 중일 수 있으니 먼저 커널 페이지 디렉터리로 옮긴 뒤 해제 */
        if(proc == &processes[current]){
            vmm_switch(vmm_kernel_cr3());
            proc->cr3 = vmm_kernel_cr3();
        }
        vmm_destroy_address_space(proc->as);
        proc->as = 0;
    }
    record_exit(proc->pid, code);
    proc->state = PROCESS_UNUSED;
}

/* 슬롯을 비우고 다른 프로세스로 넘어간다. 다시는 돌아오지 않는다 */
void process_exit(int code){
    irq_save();
    KASSERT(current != 0 && current != idle_index);
    release_slot(&processes[current], code);
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
        process_exit(PROCESS_EXIT_KILLED);
    }
    /* 다른 프로세스는 지금 switch_context 안에 멈춰 있으므로 슬롯만 비우면 다시 선택되지 않는다 */
    release_slot(proc, PROCESS_EXIT_KILLED);
    irq_restore(flags);
    return 0;
}

int process_alive(uint32_t pid){
    return find_by_pid(pid) != 0;
}

int process_wait(uint32_t pid){
    while(process_alive(pid)){
        process_sleep(10);
    }
    /* 가장 최근 기록부터 찾는다 */
    for(int i = 1; i <= EXIT_RECORDS; i++){
        uint32_t idx = (exit_record_next + EXIT_RECORDS - i) % EXIT_RECORDS;
        if(exit_records[idx].pid == pid){
            return exit_records[idx].code;
        }
    }
    return PROCESS_EXIT_UNKNOWN;
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

/*
 * 유저 프로세스의 첫 진입점. iret 으로 ring3 에 갈 때 DPL 0 인 커널 데이터 셀렉터(0x10)가 ds/es 에 남아 있으면
 * CPU 가 그 레지스터를 0 으로 만들어 버리므로, 유저 데이터 셀렉터(0x23)를 미리 넣는다.
 * 세그먼트가 모두 flat(base 0, 4GB)이라 이후 커널 코드가 0x23 으로 데이터에 접근해도 동작은 같다.
 */
__attribute__((naked)) static void user_trap_return(void){
    __asm__ volatile(
        "mov $0x23, %ax\n"
        "mov %ax, %ds\n"
        "mov %ax, %es\n"
        "mov %ax, %fs\n"
        "mov %ax, %gs\n"
        "popal\n"
        "add $8, %esp\n"
        "iret\n"
    );
}

/* 프로세스 함수가 return 하면 도착 */
static void process_return_trampoline(void){
    process_exit(0);
}
