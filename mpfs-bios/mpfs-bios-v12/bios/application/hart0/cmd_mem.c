#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "command.h"

// Memory Read Handler
void mem_read_handler(int nb_params, char **params)
{
    char *c;
    uintptr_t addr;
    unsigned int length = 16;

    if (nb_params < 1) {
        printf("Usage: mem_read <address> [length]\n");
        return;
    }

    addr = (uintptr_t)strtoull(params[0], &c, 0);
    if (*c != 0) {
        printf("Error: invalid address\n");
        return;
    }

    if (nb_params >= 2) {
        length = (unsigned int)strtoul(params[1], &c, 0);
    }

    volatile uint32_t *p = (volatile uint32_t *)addr;
    printf("Reading %u bytes from 0x%016lx:\n", length, (unsigned long)addr);

    for (unsigned int i = 0; i < (length / 4); i++) {
        if (i % 4 == 0) {
            printf("\n0x%016lx: ", (unsigned long)(addr + (i * 4)));
        }
        printf("%08lx ", (unsigned long)p[i]);
    }
    printf("\n\n");
}

// Memory Write Handler
void mem_write_handler(int nb_params, char **params)
{
    char *c;
    uintptr_t addr;
    uint32_t value;

    if (nb_params < 2) {
        printf("Usage: mem_write <address> <value>\n");
        return;
    }

    addr = (uintptr_t)strtoull(params[0], &c, 0);
    value = (uint32_t)strtoul(params[1], &c, 0);

    volatile uint32_t *p = (volatile uint32_t *)addr;
    *p = value;
    printf("Wrote 0x%08x to 0x%016lx\n", value, (unsigned long)addr);
}

// Memory Copy Handler
void mem_copy_handler(int nb_params, char **params)
{
    char *c;
    uint32_t *dst;
    uint32_t *src;
    unsigned int count = 1;

    if (nb_params < 2) {
        printf("Usage: mem_copy <dst> <src> [count (32-bit words)]\n");
        return;
    }

    dst = (uint32_t *)(uintptr_t)strtoull(params[0], &c, 0);
    src = (uint32_t *)(uintptr_t)strtoull(params[1], &c, 0);

    if (nb_params >= 3) {
        count = (unsigned int)strtoul(params[2], &c, 0);
    }

    for (unsigned int i = 0; i < count; i++) {
        dst[i] = src[i];
    }
    printf("Copied %u words from %p to %p\n", count, src, dst);
}

// Simple RAM Test Handler
void mem_test_handler(int nb_params, char **params)
{
    char *c;
    uint32_t *addr;
    unsigned int size = 1024; // Default 1KB test

    if (nb_params < 1) {
        printf("Usage: mem_test <addr> [size_in_bytes]\n");
        return;
    }

    addr = (uint32_t *)(uintptr_t)strtoull(params[0], &c, 0);
    if (nb_params >= 2) {
        size = (unsigned int)strtoul(params[1], &c, 0);
    }

    printf("Testing memory at %p (%u bytes)...\n", addr, size);
    unsigned int words = size / 4;
    int errors = 0;

    // Pattern 1: Address as data
    for (unsigned int i = 0; i < words; i++) addr[i] = (uint32_t)(uintptr_t)&addr[i];
    for (unsigned int i = 0; i < words; i++) {
        if (addr[i] != (uint32_t)(uintptr_t)&addr[i]) errors++;
    }

    if (errors == 0) {
        printf("Memory test PASSED!\n");
    } else {
        printf("Memory test FAILED with %d errors!\n", errors);
    }
}

// Memory Compare Handler
void mem_cmp_handler(int nb_params, char **params)
{
    char *c;
    uint32_t *addr1;
    uint32_t *addr2;
    unsigned int count;
    bool same = true;

    if (nb_params < 3) {
        printf("Usage: mem_cmp <addr1> <addr2> <count_words>\n");
        return;
    }

    addr1 = (uint32_t *)(uintptr_t)strtoull(params[0], &c, 0);
    addr2 = (uint32_t *)(uintptr_t)strtoull(params[1], &c, 0);
    count = (unsigned int)strtoul(params[2], &c, 0);

    for (unsigned int i = 0; i < count; i++) {
        if (addr1[i] != addr2[i]) {
            printf("Mismatch at word %u: [0x%p]=0x%08x vs [0x%p]=0x%08x\n",
                   i, &addr1[i], addr1[i], &addr2[i], addr2[i]);
            same = false;
        }
    }

    if (same) {
        printf("Memory region contents are IDENTICAL.\n");
    }
}