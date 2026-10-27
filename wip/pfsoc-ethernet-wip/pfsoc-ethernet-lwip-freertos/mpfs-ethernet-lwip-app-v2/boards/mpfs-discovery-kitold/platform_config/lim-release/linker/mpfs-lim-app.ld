/*******************************************************************************
 * Copyright 2019-2023 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * MPFS HAL Embedded Software - High LIM Application Linker Script
 * Derived from official mpfs-lim-lma-scratchpad-vma.ld for bootloader payloads.
 * Maps application origin to 0x08040000 (256KB offset from LIM base).
 *
 ******************************************************************************/

OUTPUT_ARCH( "riscv" )
ENTRY(_start)

MEMORY
{
    envm (rx)                  : ORIGIN = 0x20220100, LENGTH = 128k - 0x100
    dtim (rwx)                 : ORIGIN = 0x01000000, LENGTH = 7k       
    switch_code_dtim (rx)      : ORIGIN = 0x01001c00, LENGTH = 1k 
    e51_itim (rwx)             : ORIGIN = 0x01800000, LENGTH = 28k
    u54_1_itim (rwx)           : ORIGIN = 0x01808000, LENGTH = 28k
    u54_2_itim (rwx)           : ORIGIN = 0x01810000, LENGTH = 28k
    u54_3_itim (rwx)           : ORIGIN = 0x01818000, LENGTH = 28k
    u54_4_itim (rwx)           : ORIGIN = 0x01820000, LENGTH = 28k
    
    /* High LIM target: 256KB offset within 512KB LIM region */
    l2lim (rwx)                : ORIGIN = 0x08040000, LENGTH = 256k
    
    ddr_cached_32bit (rwx)     : ORIGIN = 0x80000000, LENGTH = 128M
    ddr_non_cached_32bit (rwx) : ORIGIN = 0xC0000000, LENGTH = 128M
    ddr_wcb_32bit (rwx)        : ORIGIN = 0xD0000000, LENGTH = 256M
}

HEAP_SIZE               = 8k;
SIZE_OF_COMMON_HART_MEM = 4k;

STACK_SIZE_E51_APPLICATION   = 8k;
STACK_SIZE_U54_1_APPLICATION = 8k;
STACK_SIZE_U54_2_APPLICATION = 8k;
STACK_SIZE_U54_3_APPLICATION = 8k;
STACK_SIZE_U54_4_APPLICATION = 8k;

SECTIONS
{
    PROVIDE(__envm_start = ORIGIN(envm));
    PROVIDE(__envm_end = ORIGIN(envm) + LENGTH(envm));
    PROVIDE(__l2lim_start = ORIGIN(l2lim));
    PROVIDE(__l2lim_end = ORIGIN(l2lim) + LENGTH(l2lim));
    PROVIDE(__ddr_cached_32bit_start = ORIGIN(ddr_cached_32bit));
    PROVIDE(__ddr_cached_32bit_end = ORIGIN(ddr_cached_32bit) + LENGTH(ddr_cached_32bit));
    PROVIDE(__ddr_non_cached_32bit_start = ORIGIN(ddr_non_cached_32bit));
    PROVIDE(__ddr_non_cached_32bit_end = ORIGIN(ddr_non_cached_32bit) + LENGTH(ddr_non_cached_32bit));
    PROVIDE(__dtim_start = ORIGIN(dtim));
    PROVIDE(__dtim_end = ORIGIN(dtim) + LENGTH(dtim));

    . = __l2lim_start;

    .text_init : ALIGN(0x10)
    {
        *(.text.init)
        *system_startup.o (.text .text* .rodata .rodata* .srodata*)
        *mtrap.o (.text .text* .rodata .rodata* .srodata*)
        *mss_h2f.o (.text .text* .rodata .rodata* .srodata*)
        *mss_l2_cache.o (.text .text* .rodata .rodata* .srodata*)
        . = ALIGN(0x10); 
    } > l2lim 
  
    .text : ALIGN(0x10)
    {
        __text_load = LOADADDR(.text);
        . = ALIGN(0x10);
        __text_start = .;
        __l2_scratchpad_vma_start = .; 
        *(.text .text.* .gnu.linkonce.t.*)
        *(.plt)
        . = ALIGN(0x10);

        KEEP (*crtbegin.o(.ctors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
        KEEP (*(SORT(.ctors.*)))
        KEEP (*crtend.o(.ctors))
        KEEP (*crtbegin.o(.dtors))
        KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
        KEEP (*(SORT(.dtors.*)))
        KEEP (*crtend.o(.dtors))

        *(.rodata .rodata.* .gnu.linkonce.r.*)
        *(.sdata2 .sdata2.* .gnu.linkonce.s2.*)
        *(.gcc_except_table) 
        *(.eh_frame_hdr)
        *(.eh_frame)

        KEEP (*(.init))
        KEEP (*(.fini))

        PROVIDE_HIDDEN (__preinit_array_start = .);
        KEEP (*(.preinit_array))
        PROVIDE_HIDDEN (__preinit_array_end = .);
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT(.init_array.*)))
        KEEP (*(.init_array))
        PROVIDE_HIDDEN (__init_array_end = .);
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(.fini_array))
        KEEP (*(SORT(.fini_array.*)))
        PROVIDE_HIDDEN (__fini_array_end = .);

        *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2)
        *(.srodata*)

        . = ALIGN(0x10);
        __text_end = .;
    } > l2lim

    .sdata : ALIGN(0x10)
    {
        __sdata_load = LOADADDR(.sdata);
        __sdata_start = .; 
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        . = ALIGN(0x10);
        __sdata_end = .;
    } > l2lim

    .data : ALIGN(0x10)
    { 
        __data_load = LOADADDR(.data);
        __data_start = .; 
        *(.got.plt) *(.got)
        *(.shdata)
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(0x10);
        __data_end = .;
    } > l2lim

    .sbss : ALIGN(0x10)
    {
        __sbss_start = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon)
        . = ALIGN(0x10);
        __sbss_end = .;
    } > l2lim

    .bss : ALIGN(0x10)
    { 
        __bss_start = .;
        *(.shbss)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(0x10);
        __bss_end = .;
    } > l2lim

    _end = .;

    .heap : ALIGN(0x10)
    {
        __heap_start = .;
        . += HEAP_SIZE;
        __heap_end = .;
        . = ALIGN(0x10);
        _heap_end = __heap_end;
    } > l2lim

    /* Application & Startup Stack Aliases for All Harts */
    .app_stack_e51 : ALIGN(0x10)
    {
        PROVIDE(__app_stack_bottom_h0 = .);
        PROVIDE(__stack_bottom_h0$ = .);
        . += STACK_SIZE_E51_APPLICATION;
        PROVIDE(__app_stack_top_h0 = .);
        PROVIDE(__stack_top_h0$ = .);
    } > l2lim

    .app_stack_u54_1 : ALIGN(0x10)
    {
        PROVIDE(__app_stack_bottom_h1 = .);
        PROVIDE(__stack_bottom_h1$ = .);
        . += STACK_SIZE_U54_1_APPLICATION;
        PROVIDE(__app_stack_top_h1 = .);
        PROVIDE(__stack_top_h1$ = .);
    } > l2lim

    .app_stack_u54_2 : ALIGN(0x10)
    {
        PROVIDE(__app_stack_bottom_h2 = .);
        PROVIDE(__stack_bottom_h2$ = .);
        . += STACK_SIZE_U54_2_APPLICATION;
        PROVIDE(__app_stack_top_h2 = .);
        PROVIDE(__stack_top_h2$ = .);
    } > l2lim

    .app_stack_u54_3 : ALIGN(0x10)
    {
        PROVIDE(__app_stack_bottom_h3 = .);
        PROVIDE(__stack_bottom_h3$ = .);
        . += STACK_SIZE_U54_3_APPLICATION;
        PROVIDE(__app_stack_top_h3 = .);
        PROVIDE(__stack_top_h3$ = .);
    } > l2lim

    .app_stack_u54_4 : ALIGN(0x10)
    {
        PROVIDE(__app_stack_bottom_h4 = .);
        PROVIDE(__stack_bottom_h4$ = .);
        . += STACK_SIZE_U54_4_APPLICATION;
        PROVIDE(__app_stack_top_h4 = .);
        PROVIDE(__stack_top_h4$ = .);
    } > l2lim

    .app_hart_common : ALIGN(0x10)
    {
        PROVIDE(__app_hart_common_start = .);
        . += SIZE_OF_COMMON_HART_MEM;
        PROVIDE(__app_hart_common_end = .);
        __l2_scratchpad_vma_end = .; 
    } > l2lim
}