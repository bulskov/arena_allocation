#ifndef ARENA_FIXED_ARENA_H
#define ARENA_FIXED_ARENA_H

#include "allocator.h"
#include "scratch.h"
#include <stddef.h>
#include <stdint.h>

/*
 * fixed_arena_t — bump allocator over an externally-provided buffer.
 *
 * The caller owns the buffer and is responsible for its lifetime.
 * The buffer itself is typically obtained via mem_map() from platform.h,
 * but may also be a stack array or any other contiguous region.
 *
 * Creator API:
 *   fixed_arena_init      — attach to an existing buffer
 *   fixed_arena_reset     — rewind offset to zero (buffer is untouched)
 *   fixed_arena_allocator — produce an allocator_t for user code
 *   fixed_arena_scratch_begin — begin a temporary sub-scope
 */

typedef struct
{
    uint8_t *base;
    size_t size;
    size_t offset;
} fixed_arena_t;

void fixed_arena_init(fixed_arena_t *a, void *buf, size_t size);
void fixed_arena_reset(fixed_arena_t *a);
allocator_t fixed_arena_allocator(fixed_arena_t *a);
void fixed_arena_scratch_begin(scratch_t *s, fixed_arena_t *a);
arena_stats_t fixed_arena_stats(const fixed_arena_t *a);

#endif /* ARENA_FIXED_ARENA_H */
