#ifndef ARENA_STACK_ARENA_H
#define ARENA_STACK_ARENA_H

#include "allocator.h"
#include <stddef.h>
#include <stdint.h>

/*
 * stack_arena_t — bump allocator with O(1) LIFO free, no per-allocation
 * headers.
 *
 * All allocations are aligned to min_align and their sizes are rounded up to
 * min_align.  This eliminates padding gaps between consecutive allocations,
 * which makes the LIFO free check exact:
 *
 *   free(ptr, size) rewinds offset to ptr when
 *   ptr + align_up(size, min_align) == base + offset
 *
 * If free is called on an allocation that is not the current top of the stack
 * (LIFO violation) it is a silent no-op.
 *
 * The align parameter passed to alloc/realloc is honoured only up to
 * min_align.  If you need stricter per-allocation alignment, set min_align
 * to the required value at creation time.
 *
 * The backing buffer is mmap'd at init time and munmap'd on destroy.
 * min_align must be a power of two >= 1.
 *
 * Creator API:
 *   stack_arena_init      — mmap backing buffer
 *   stack_arena_destroy   — munmap backing buffer
 *   stack_arena_reset     — rewind offset to zero
 *   stack_arena_allocator — produce an allocator_t for user code
 */

typedef struct
{
    uint8_t *base;
    size_t capacity;
    size_t offset;
    size_t min_align;
} stack_arena_t;

int stack_arena_init(stack_arena_t *a, size_t capacity, size_t min_align);
void stack_arena_destroy(stack_arena_t *a);
void stack_arena_reset(stack_arena_t *a);
allocator_t stack_arena_allocator(stack_arena_t *a);
allocator_t stack_arena_allocator_new(
    stack_arena_t *a, size_t capacity, size_t min_align);
arena_stats_t stack_arena_stats(const stack_arena_t *a);

#endif /* ARENA_STACK_ARENA_H */
