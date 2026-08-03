.section .entry
.global _start

_start:
    # Mask all asynchronous interrupts globally
    csrw mstatus, zero
    csrw mie, zero

    # Point the stack pointer directly to our E51 stack limits
    la sp, __stack_top

    # Zero out the BSS section manually
    la t0, __bss_start
    la t1, __bss_end
bss_clear_loop:
    bge t0, t1, jump_to_main
    sd zero, 0(t0)
    addi t0, t0, 8
    j bss_clear_loop

jump_to_main:
    tail main