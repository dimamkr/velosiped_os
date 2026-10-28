ENTRY(_start)

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

        . = ALIGN(8);
        __heap_start = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
        *(.eh_frame_hdr*)
        *(.debug*)
    }
}