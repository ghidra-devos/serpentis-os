ENTRY(_start)

USER_CODE = 0x400000;
HEADER_SIZE = 80;
STACK_SIZE = 0x10000;

SECTIONS
{
    . = USER_CODE - HEADER_SIZE;
    .serp : {
        LONG(0x53525045)
        SHORT(1)
        SHORT(1)
        QUAD(HEADER_SIZE)
        QUAD(_start - USER_CODE)
        QUAD(HEADER_SIZE)
        QUAD(__code_end - USER_CODE)
        QUAD(0)
        QUAD(0)
        QUAD(__bss_end - __code_end)
        QUAD(STACK_SIZE)
        QUAD((__code_end - USER_CODE + 0xFFF) / 0x1000)
    }
    . = USER_CODE;
    .text : { *(.text.start) *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    __code_end = .;
    .bss (NOLOAD) : { *(.bss .bss.*) *(COMMON) }
    __bss_end = .;
    /DISCARD/ : { *(.comment) *(.note*) *(.eh_frame*) *(.debug*) }
}
