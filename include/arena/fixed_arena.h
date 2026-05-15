#ifndef ARENA_FIXED_ARENA_H
#define ARENA_FIXED_ARENA_H

#include "allocator.h"
#include "scratch.h"
#include <stddef.h>
#include <stdint.h>

/*
 * fixed_arena_t — bump allocator over a contiguous memory region.
 *
 * Two construction modes:
 *   fixed_arena_init   — attach to an externally-provided buffer; caller owns
 *                        the buffer and is responsible for its lifetime.
 *   fixed_arena_create — allocate the backing region via mmap (or VirtualAlloc
 *                        on Windows); the arena owns the memory and
 *                        fixed_arena_destroy will release it.
 *
 * Creator API:
 *   fixed_arena_init      — attach to an existing buffer (not owned)
 *   fixed_arena_create    — allocate owned backing memory
 *   fixed_arena_destroy   — release backing memory if owned (no-op otherwise)
 *   fixed_arena_reset     — rewind offset to zero (buffer is untouched)
 *   fixed_arena_allocator — produce an allocator_t for user code
 *   fixed_arena_allocator_new — create owned arena + return allocator_t
 *   fixed_arena_scratch_begin — begin a temporary sub-scope
 */

typedef struct
{
    uint8_t *base;
    size_t size;
    size_t offset;
    int owned; /* 1 if base was allocated by fixed_arena_create */
} fixed_arena_t;

void fixed_arena_init(fixed_arena_t *a, void *buf, size_t size);
int fixed_arena_create(fixed_arena_t *a, size_t size);
void fixed_arena_destroy(fixed_arena_t *a);
void fixed_arena_reset(fixed_arena_t *a);
allocator_t fixed_arena_allocator(fixed_arena_t *a);
allocator_t fixed_arena_allocator_new(fixed_arena_t *a, size_t size);
void fixed_arena_scratch_begin(scratch_t *s, fixed_arena_t *a);
arena_stats_t fixed_arena_stats(const fixed_arena_t *a);

#endif /* ARENA_FIXED_ARENA_H */
