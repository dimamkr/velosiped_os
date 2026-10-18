ENTRY(_start)

HEAP_SIZE = 0x400000;   /* 4 MiB фиксированной кучи под picolibc malloc */

SECTIONS
{
    . = 0x08048000;

    .text : ALIGN(4096) {
        *(.text._start)
        *(.text*)
    }

    .rodata : ALIGN(4096) {
        *(.rodata*)
    }

    .data : ALIGN(4096) {
        *(.data*)
    }

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;

        /* статическая куча для picolibc TODO через brk сделать динамическую*/
        . = ALIGN(4096);
        __heap_start = .;
        . = . + HEAP_SIZE;
        __heap_end = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.eh_frame_hdr*)
        *(.debug*)
    }
}