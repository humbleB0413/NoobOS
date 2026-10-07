#include "linux/gdt.h"
#include "linux/idt.h"
#include "linux/panic.h"
#include "string.h"

#define DF_STACK_SIZE 4096

static gdtr_t gdt[7];
static tss_entry_t tss_entry; //1 for single cpu
static tss_entry_t df_tss;    // double fault 전용 태스크
static uint8_t df_stack[DF_STACK_SIZE] __attribute__((aligned(16)));

extern __attribute__((aligned(0x10))) idtr_t KERNEL_IDT[IDT_ENTRIES_SIZE];
extern const char* process_guard_owner(uint32_t addr);

void init_gdt(){
    memset(gdt, 0, sizeof(gdt));
    memset(&tss_entry, 0, sizeof(tss_entry));

    tss_entry.ss0      = 0x10;   /* 0x10 — 고정, 절대 바뀌지 않음 */
    tss_entry.esp0     = 0;                 /* 첫 태스크 스케줄 전까지 임시 0 */
    tss_entry.iomap_base = sizeof(tss_entry_t);   /* IOPB 없음 → 모든 I/O 포트 차단 */

    set_gdt_descriptor(gdt + 0, 0, 0x00000, 0x00, 0x0); // null
    set_gdt_descriptor(gdt + 1, 0, 0xFFFFF, 0x9A, 0xC); // kernel code
    set_gdt_descriptor(gdt + 2, 0, 0xFFFFF, 0x92, 0xC); // kernel data
    set_gdt_descriptor(gdt + 3, 0, 0xFFFFF, 0xFA, 0xC); // user code
    set_gdt_descriptor(gdt + 4, 0, 0xFFFFF, 0xF2, 0xC); // user data
    set_gdt_descriptor(gdt + 5, (uint32_t)&tss_entry, sizeof(tss_entry)-1 , 0x89, 0x0); //tss for cpu 1
    set_gdt_descriptor(gdt + 6, (uint32_t)&df_tss, sizeof(df_tss)-1 , 0x89, 0x0);       //double fault task

    gdtr_descriptor_t gdtr_desc;
    gdtr_desc.limit = sizeof(gdt) - 1;
    gdtr_desc.address = (uint32_t)gdt;

    asm_load_gdt(&gdtr_desc);

    return;
}

void tss_set_kernel_stack(uint32_t esp0){
    tss_entry.esp0 = esp0;
}

/*
 * 커널 스택이 가드 페이지까지 넘치면 #PF 를 전달하려고 같은 스택에 push 하다 또 실패해 #DF 가 되고,
 * #DF 도 같은 스택을 쓰면 triple fault 로 리셋된다. 그래서 #DF 는 task gate 로 받아 CPU 가
 * df_tss 의 깨끗한 스택으로 갈아타게 한다. 이전 상태는 CPU 가 tss_entry 에 저장해 둔다.
 */
static void double_fault_task(void){
    uint32_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    const char* owner = process_guard_owner(tss_entry.esp);
    if(!owner){
        owner = process_guard_owner(cr2);
    }
    if(owner){
        kpanic("kernel stack overflow in %s (esp=0x%x eip=0x%x cr2=0x%x)",
               owner, tss_entry.esp, tss_entry.eip, cr2);
    }
    kpanic("Double Fault (esp=0x%x eip=0x%x cr2=0x%x)", tss_entry.esp, tss_entry.eip, cr2);
}

void init_double_fault_task(void){
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    memset(&df_tss, 0, sizeof(df_tss));
    df_tss.cr3 = cr3;
    df_tss.eip = (uint32_t)double_fault_task;
    df_tss.eflags = 0x2;                                  /* IF=0 */
    df_tss.esp = (uint32_t)(df_stack + DF_STACK_SIZE);
    df_tss.cs = GDT_KERNEL_CODE;
    df_tss.ds = df_tss.es = df_tss.fs = df_tss.gs = df_tss.ss = GDT_KERNEL_DATA;
    df_tss.esp0 = df_tss.esp;
    df_tss.ss0 = GDT_KERNEL_DATA;
    df_tss.iomap_base = sizeof(tss_entry_t);

    /* task gate: offset 은 무시되고 셀렉터만 의미가 있다 */
    set_idt_descriptor(KERNEL_IDT + 8, 0, GDT_DF_TSS, IDT_P | IDT_DPL_KERNEL | IDT_GATE_TYPE_TASK);
}

void set_gdt_descriptor(gdtr_t* gdt, uint32_t base, uint32_t limit, 
    uint8_t access, uint8_t flag){
        gdt->limit = limit & 0xFFFF;
        gdt -> base_1 = base & 0xFFFF;
        gdt -> base_2 = (base & 0xFF0000) >> 16;
        gdt -> base_3 = (base & 0xFF000000) >> 24;
        gdt -> access = access;
        gdt->limit_flag = ((limit & 0xF0000) >> 16) | ((flag & 0xF) << 4);

        return ;
    }
