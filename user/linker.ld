/* NoobOS 유저 프로그램: 커널 유저 영역(0x08000000~) 안의 전통적인 i386 시작 주소 */
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH("i386")
ENTRY(_start)

/* 세그먼트를 권한별로 나눠, 커널 ELF 로더가 코드/읽기전용 데이터 페이지를 쓰기 금지로 매핑하게 한다 */
PHDRS
{
    text   PT_LOAD FLAGS(5);   /* R-X */
    rodata PT_LOAD FLAGS(4);   /* R-- */
    data   PT_LOAD FLAGS(6);   /* RW- */
}

SECTIONS
{
    . = 0x08048000;
    .text : { *(.text.start) *(.text .text.*) } :text
    . = ALIGN(4K);
    .rodata : { *(.rodata .rodata.*) } :rodata
    /* .data 가 비어 있어도 .bss 가 읽기 전용 페이지에 붙지 않도록 위치 카운터 자체를 정렬 */
    . = ALIGN(4K);
    .data : { *(.data .data.*) } :data
    .bss : { *(COMMON) *(.bss .bss.*) } :data
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) }
}
