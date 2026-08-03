OUTPUT_ARCH( "riscv" )
ENTRY(_start)

MEMORY
{
    envm (rx)   : ORIGIN = 0x20220100, LENGTH = 128k
    l2lim (rwx) : ORIGIN = 0x08000000, LENGTH = 512k
}

SECTIONS
{
    .text : {
        *(.entry)
        *(.text .text.*)
        *(.rodata .rodata.*)
    } > envm

    .data : {
        __data_start = .;
        *(.data .data.*)
        *(.sdata .sdata.*)
        __data_end = .;
    } > l2lim AT> envm

    .bss : {
        __bss_start = .;
        *(.bss .bss.*)
        *(.sbss .sbss.*)
        *(COMMON)
        __bss_end = .;
    } > l2lim

    /* Set up a clean 4KB stack memory space at the top of LIM area */
    __stack_top = ORIGIN(l2lim) + 4k;
}