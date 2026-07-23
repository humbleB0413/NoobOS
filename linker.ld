OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH("i386")
ENTRY(_start)

SECTIONS
{
    /* 1 MiB — standard Multiboot load address */
    . = 1M;

    .text ALIGN(4K) :
    {
        /*
         * boot/boot.S is placed first by the Makefile (link order) so the
         * Multiboot header lands within the first 8 KiB of the image.
         * KEEP(.multiboot) is reserved for a future dedicated section.
         */
        KEEP(*(.multiboot))
        *(.text .text.*)
    }

    .rodata ALIGN(4K) :
    {
        *(.rodata .rodata.*)
    }

    .data ALIGN(4K) :
    {
        *(.data .data.*)
    }

    /* _edata marks the end of initialised data (used by a.out kludge fields) */
    _edata = .;

    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss .bss.*)
    }

    /* _end is used by early heap / memory-map initialisation */
    _end = ALIGN(4K);

    /DISCARD/ :
    {
        *(.comment)
        *(.note.GNU-stack)
    }
}
