.section .entry
.global _start

_start:
    # 1. Mask all asynchronous interrupts globally for this core
    csrw mstatus, zero
    csrw mie, zero

    # 2. Read the unique Hardware Thread ID (Hart ID)
    # 0 = E51 Core, 1 = U54_1, 2 = U54_2, 3 = U54_3, 4 = U54_4
    csrr a0, mhartid
    bnez a0, .u54_parking_lot

    # ==========================================================================
    # MASTER CORE PATH (E51 Core - Hart 0)
    # ==========================================================================
    la sp, __stack_top   # Assign the local 4KB stack space for E51 execution
    
    # Manually zero-out the narrow BSS region
    la t0, __bss_start
    la t1, __bss_end
.bss_clear_loop:
    bge t0, t1, .jump_to_main
    sd zero, 0(t0)
    addi t0, t0, 8
    j .bss_clear_loop

.jump_to_main:
    tail main            # Branch straight to our custom bootloader C environment

    # ==========================================================================
    # SECONDARY CORES PATH (U54 Cores - Harts 1 to 4)
    # ==========================================================================
.u54_parking_lot:
    # Calculate the exact CLINT MSIP register offset for this specific Hart
    # Register Address = 0x02000000 + (hartid * 4)
    li t0, 0x02000000
    slli t1, a0, 2
    add t0, t0, t1

.u54_hardware_spin:
    lw t2, 0(t0)                  # Sample the hardware MMIO register state
    beqz t2, .u54_hardware_spin   # If flag is 0, continue holding core execution

    # WOKEN UP BY MASTER!
    # Safely load the application entry vector pointer variable out of RAM
    la t3, g_u54_jump_target
    ld t4, 0(t3)

    # Reset our local hardware MSIP flag bit back to zero
    sw zero, 0(t0)

    # Purge pipeline, synchronize execution contexts, and branch to the downloaded app
    fence.i
    jr t4