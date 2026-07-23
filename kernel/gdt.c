#include "linux/gdt.h"
#include "string.h"

static gdtr_t gdt[6];
static tss_entry_t tss_entry; //1 for single cpu

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

    gdtr_descriptor_t gdtr_desc;
    gdtr_desc.limit = sizeof(gdt) - 1;
    gdtr_desc.address = (uint32_t)gdt;

    asm_load_gdt(&gdtr_desc);

    return;
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
