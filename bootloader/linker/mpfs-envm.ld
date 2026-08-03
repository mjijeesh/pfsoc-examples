/*******************************************************************************
 * Copyright 2026 Microchip Corporation.
 * SPDX-License-Identifier: MIT
 * Optimized GNU Linker Script for Bare-Metal Modular Application in eNVM
 ******************************************************************************/

OUTPUT_ARCH( "riscv" )
ENTRY(_start)

PROVIDE(HEAP_SIZE = 64k);
PROVIDE(STACK_SIZE_PER_HART = 16k);

MEMORY
{
    envm (rx)          : ORIGIN = 0x20220100, LENGTH = (128k-256)
    dtim (rwx)         : ORIGIN = 0x01000000, LENGTH = 8k
    e51_itim (rwx)     : ORIGIN = 0x01800000, LENGTH = 28k
    u54_1_itim (rwx)   : ORIGIN = 0x01808000, LENGTH = 28k
    u54_2_itim (rwx)   : ORIGIN = 0x01810000, LENGTH = 28k
    u54_3_itim (rwx)   : ORIGIN = 0x01818000, LENGTH = 28k
    u54_4_itim (rwx)   : ORIGIN = 0x01820000, LENGTH = 28k
    l2lim (rwx)        : ORIGIN = 0x08000000, LENGTH = 512k
    l2zerodevice (rwx) : ORIGIN = 0x0A000000, LENGTH = 512k
    ddr (rwx)          : ORIGIN = 0x80000000, LENGTH = 1024m
}

SECTIONS
{
    /* --- 1. PHYSICAL HARDWARE PLATFORM BOUNDARY MARKERS --- */
    /* Required by platform/mpfs_hal/common/mss_l2_cache.c */
    __envm_start = ORIGIN(envm);
    __envm_end   = ORIGIN(envm) + LENGTH(envm);
    
    __l2lim_start = ORIGIN(l2lim);
    __l2lim_end   = ORIGIN(l2lim) + LENGTH(l2lim);
    
    __l2_scratchpad_vma_start = ORIGIN(l2zerodevice);
    __l2_scratchpad_vma_end   = ORIGIN(l2zerodevice) + LENGTH(l2zerodevice);

    . = __envm_start;

    /* --- 2. CODE TEXT SECTION --- */
    .text : ALIGN(0x10)
    {
        __text_start = .;
        *(.entry)
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
        *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2)
        *(.srodata*)
        *(.sdata2*)
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
        
        . = ALIGN(0x10);
        __text_end = .;
    } >envm

    __text_load = LOADADDR(.text);

    .gnu_build_id : ALIGN(8) {
        PROVIDE(gnu_build_id = .);
        *(.note.gnu.build-id)
    } >envm

    /* --- 3. HARDWARE CONFIG SWITCH CODE MARKERS --- */
    /* Required by platform/mpfs_hal/startup_gcc/mss_entry.S */
    /* Setting these equal lets the boot-assembly safely bypass the loop */
    __sc_start = .;
    __sc_end = .;
    __sc_load = .;

    /* --- 4. VOLATILE DATA SECTIONS (RAM EXECUTION) --- */
    .sdata : ALIGN(0x40)
    {
        __sdata_load = LOADADDR(.sdata);
        __sdata_start = .;
        __global_pointer$ = . + 0x800;
        *(.sdata .sdata.* .gnu.linkonce.s.*)
        . = ALIGN(0x10);
        __sdata_end = .;
    } >l2lim AT>envm

    .data : ALIGN(0x40)
    {
        __data_load = LOADADDR(.data);
        __data_start = .;
        *(.got.plt) *(.got)
        *(.shdata)
        *(.data .data.* .gnu.linkonce.d.*)
        . = ALIGN(0x10);
        __data_end = .;
    } >l2lim AT>envm
    _end = .;

    .sbss : ALIGN(0x40)
    {
        __sbss_start = .;
        *(.sbss .sbss.* .gnu.linkonce.sb.*)
        *(.scommon)
        . = ALIGN(0x10);
        __sbss_end = .;
    } >l2lim

    .bss : ALIGN(0x40)
    {
        __bss_start = .;
        *(.shbss)
        *(.bss .bss.* .gnu.linkonce.b.*)
        *(COMMON)
        . = ALIGN(0x10);
        __bss_end = .;
    } >l2lim

    /* --- 5. EXECUTION STACK BOUNDS --- */
    .stack : ALIGN(0x40)
    {
        __stack_bottom = .;
        __stack_bottom_h0$ = .;
        . += STACK_SIZE_PER_HART;
        __stack_top_h0$ = . - 8;
        __stack_bottom_h1$ = .;
        . += STACK_SIZE_PER_HART;
        __stack_top_h1$ = . - 8;
        __stack_bottom_h2$ = .;
        . += STACK_SIZE_PER_HART;
        __stack_top_h2$ = . - 8;
        __stack_bottom_h3$ = .;
        . += STACK_SIZE_PER_HART;
        __stack_top_h3$ = . - 8;
        __stack_bottom_h4$ = .;
        . += STACK_SIZE_PER_HART;
        __stack_top_h4$ = . - 8;
        __stack_top = . - 8;
    } >l2lim

    PROVIDE(__app_stack_top_h0 = __stack_top_h0$);
    PROVIDE(__app_stack_top_h1 = __stack_top_h1$);
    PROVIDE(__app_stack_top_h2 = __stack_top_h2$);
    PROVIDE(__app_stack_top_h3 = __stack_top_h3$);
    PROVIDE(__app_stack_top_h4 = __stack_top_h4$);

    /* --- 6. HEAP SECTION --- */
    .heap : ALIGN(0x40)
    {
        __heap_start = .;
        PROVIDE(__l2lim_heap_base = .);
        . += HEAP_SIZE;
        __heap_end = .;
    } >l2lim
    PROVIDE(__l2lim_heap_end = .);

    _hss_end = .;
}