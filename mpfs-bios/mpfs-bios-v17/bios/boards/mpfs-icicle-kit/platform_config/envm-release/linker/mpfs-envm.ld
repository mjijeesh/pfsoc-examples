/*******************************************************************************
 * Copyright 2019-2023 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * MPFS HAL Embedded Software
 *
 * File Name: mpfs-envm.ld
 * Description: Linker script configured for direct Execute-In-Place (XIP) 
 *              from eNVM. Executable code (.text) and read-only data (.rodata) 
 *              remain in eNVM, while writable sections (.data, .bss, stacks) 
 *              are mapped to L2-LIM RAM.
 *******************************************************************************/

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
    l2lim (rwx)                : ORIGIN = 0x08000000, LENGTH = 512k
    scratchpad(rwx)            : ORIGIN = 0x0A000000, LENGTH = 512k
    ddr_cached_32bit (rwx)     : ORIGIN = 0x80000000, LENGTH = 128M
    ddr_non_cached_32bit (rwx) : ORIGIN = 0xC0000000, LENGTH = 128M
    ddr_wcb_32bit (rwx)        : ORIGIN = 0xD0000000, LENGTH = 256M
    ddr_cached_38bit (rwx)     : ORIGIN = 0x1000000000, LENGTH = 896M
    ddr_non_cached_38bit (rwx) : ORIGIN = 0x1400000000, LENGTH = 896M
    ddr_wcb_38bit (rwx)        : ORIGIN = 0x1800000000, LENGTH = 0k
}

HEAP_SIZE = 8k;
SIZE_OF_COMMON_HART_MEM = 4k;

/* Startup stack sizes (executing in L2 LIM) */
STACK_SIZE_E51_STARTUP   = 4k;
STACK_SIZE_U54_1_STARTUP = 4k;
STACK_SIZE_U54_2_STARTUP = 4k;
STACK_SIZE_U54_3_STARTUP = 4k;
STACK_SIZE_U54_4_STARTUP = 4k;

/* Application stack sizes */
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
    PROVIDE(__ddr_wcb_32bit_start = ORIGIN(ddr_wcb_32bit));
    PROVIDE(__ddr_wcb_32bit_end = ORIGIN(ddr_wcb_32bit) + LENGTH(ddr_wcb_32bit));
    PROVIDE(__ddr_cached_38bit_start = ORIGIN(ddr_cached_38bit));
    PROVIDE(__ddr_cached_38bit_end = ORIGIN(ddr_cached_38bit) + LENGTH(ddr_cached_38bit));
    PROVIDE(__ddr_non_cached_38bit_start = ORIGIN(ddr_non_cached_38bit));
    PROVIDE(__ddr_non_cached_38bit_end = ORIGIN(ddr_non_cached_38bit) + LENGTH(ddr_non_cached_38bit));
    PROVIDE(__ddr_wcb_38bit_start = ORIGIN(ddr_wcb_38bit));
    PROVIDE(__ddr_wcb_38bit_end = ORIGIN(ddr_wcb_38bit) + LENGTH(ddr_wcb_38bit));
    PROVIDE(__dtim_start = ORIGIN(dtim));
    PROVIDE(__dtim_end = ORIGIN(dtim) + LENGTH(dtim));
    PROVIDE(__e51itim_start = ORIGIN(e51_itim));
    PROVIDE(__e51itim_end = ORIGIN(e51_itim) + LENGTH(e51_itim));
    PROVIDE(__u54_1_itim_start = ORIGIN(u54_1_itim));
    PROVIDE(__u54_1_itim_end = ORIGIN(u54_1_itim) + LENGTH(u54_1_itim));
    PROVIDE(__u54_2_itim_start = ORIGIN(u54_2_itim));
    PROVIDE(__u54_2_itim_end = ORIGIN(u54_2_itim) + LENGTH(u54_2_itim));
    PROVIDE(__u54_3_itim_start = ORIGIN(u54_3_itim));
    PROVIDE(__u54_3_itim_end = ORIGIN(u54_3_itim) + LENGTH(u54_3_itim));
    PROVIDE(__u54_4_itim_start = ORIGIN(u54_4_itim));
    PROVIDE(__u54_4_itim_end = ORIGIN(u54_4_itim) + LENGTH(u54_4_itim));
    
    . = __envm_start;

    /* Boot & Startup routines executed directly in eNVM */
    .text_init : ALIGN(0x10)
    {
        *(.text.init)
        *system_startup.o (.text .text* .rodata .rodata* .srodata*)
        *mtrap.o (.text .text* .rodata .rodata* .srodata*)
        *mss_h2f.o (.text .text* .rodata .rodata* .srodata*)
        *mss_l2_cache.o (.text .text* .rodata .rodata* .srodata*)
        . = ALIGN(0x10);
    } >envm

    /* Application code (.text) & Read-Only constants (.rodata) execute directly from eNVM (XIP) */
    .text : ALIGN(0x10)
    {
        __text_load = LOADADDR(.text);
        . = ALIGN(0x10);
        __text_start = .;
        
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
    } >envm

    /* Initialized small data section - Loaded in eNVM, executed in L2 LIM */
    .sdata : ALIGN(0x10)
    {
        __sdata_load = LOADADDR(.sdata);
        __sdata_start = .; 
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        . = ALIGN(0x10);
        __sdata_end = .;
    } >l2lim AT> envm

    /* Initialized global data section - Loaded in eNVM, executed in L2 LIM */
    .data : ALIGN(0x10)
    { 
        __data_load = LOADADDR(.data);
        __data_start = .; 
        *(.got.plt) *(.got)
        *(.shdata)
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(0x10);
        __data_end = .;
    } >l2lim AT> envm

    /* Uninitialized small global data section in L2 LIM */
    .sbss : ALIGN(0x10)
    {
        __sbss_start = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon)
        . = ALIGN(0x10);
        __sbss_end = .;
    } >l2lim

    /* Uninitialized BSS section in L2 LIM */
    .bss : ALIGN(0x10)
    { 
        __bss_start = .;
        *(.shbss)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(0x10);
        __bss_end = .;
    } >l2lim

    _end = .;

    /* Heap section in L2 LIM */
    .heap : ALIGN(0x10)
    {
        __heap_start = .;
        . += HEAP_SIZE;
        __heap_end = .;
        . = ALIGN(0x10);
        _heap_end = __heap_end;
    } >l2lim

    /* Startup Stacks in L2 LIM */
    .stack_e51 :
    {
        PROVIDE(__stack_bottom_h0$ = .);
        . += STACK_SIZE_E51_STARTUP;
        PROVIDE(__stack_top_h0$ = .);
    } >l2lim

    .stack_u54_1 :
    {
        PROVIDE(__stack_bottom_h1$ = .);
        . += STACK_SIZE_U54_1_STARTUP;
        PROVIDE(__stack_top_h1$ = .);
    } >l2lim

    .stack_u54_2 :
    {
        PROVIDE(__stack_bottom_h2$ = .);
        . += STACK_SIZE_U54_2_STARTUP;
        PROVIDE(__stack_top_h2$ = .);
    } >l2lim

    .stack_u54_3 :
    {
        PROVIDE(__stack_bottom_h3$ = .);
        . += STACK_SIZE_U54_3_STARTUP;
        PROVIDE(__stack_top_h3$ = .);
    } >l2lim

    .stack_u54_4 :
    {
        PROVIDE(__stack_bottom_h4$ = .);
        . += STACK_SIZE_U54_4_STARTUP;
        PROVIDE(__stack_top_h4$ = .);
    } >l2lim

    /* Application Stacks in L2 LIM */
    .app_stack_e51 :
    {
        PROVIDE(__app_stack_bottom_h0 = .);
        . += STACK_SIZE_E51_APPLICATION;
        PROVIDE(__app_stack_top_h0 = .);
    } >l2lim

    .app_stack_u54_1 :
    {
        PROVIDE(__app_stack_bottom_h1$ = .);
        . += STACK_SIZE_U54_1_APPLICATION;
        PROVIDE(__app_stack_top_h1 = .);
    } >l2lim

    .app_stack_u54_2 :
    {
        PROVIDE(__app_stack_bottom_h2 = .);
        . += STACK_SIZE_U54_2_APPLICATION;
        PROVIDE(__app_stack_top_h2 = .);
    } >l2lim

    .app_stack_u54_3 :
    {
        PROVIDE(__app_stack_bottom_h3 = .);
        . += STACK_SIZE_U54_3_APPLICATION;
        PROVIDE(__app_stack_top_h3 = .);
    } >l2lim

    .app_stack_u54_4 :
    {
        PROVIDE(__app_stack_bottom_h4 = .);
        . += STACK_SIZE_U54_4_APPLICATION;
        PROVIDE(__app_stack_top_h4 = .);
    } >l2lim

    /* Shared Memory across harts */
    .app_hart_common :
    {
        PROVIDE(__app_hart_common_start = .);
        . += SIZE_OF_COMMON_HART_MEM;
        PROVIDE(__app_hart_common_end = .);
    } >l2lim

    /* Clock-switching code running in DTIM (Required when modifying eNVM/System Clocks) */
    .ram_code : ALIGN(0x10)
    {
        . = ALIGN (4);
        __sc_load = LOADADDR (.ram_code);
        __sc_start = .;
        *(.ram_codetext)
        *(.ram_codetext*)
        *(.ram_coderodata)
        *(.ram_coderodata*)
        . = ALIGN (4);
        __sc_end = .;
        PROVIDE(__start_of_free_lim$ = .);
    } >switch_code_dtim AT> envm
}