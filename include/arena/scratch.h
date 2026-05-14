#ifndef ARENA_SCRATCH_H
#define ARENA_SCRATCH_H

#include "allocator.h"
#include <stddef.h>

/*
 * scratch_t — a temporary sub-allocation scope.
 *
 * A scratch borrows the current position of a parent bump-style arena.
 * All allocations made through scratch_allocator() go into the parent arena
 * as normal. When scratch_end() is called, the arena is rewound to the saved
 * position, effectively freeing everything allocated since scratch_begin().
 *
 * The parent arena must outlive the scratch. Scratches must not be nested on
 * the same arena unless they are strictly LIFO (innermost ended first).
 *
 * Creator API — not visible to users:
 *
 *   Each arena type provides its own scratch_begin:
 *     fixed_arena_scratch_begin(scratch_t*, fixed_arena_t*)
 *     growing_arena_scratch_begin(scratch_t*, growing_arena_t*)
 *     virtual_arena_scratch_begin(scratch_t*, virtual_arena_t*)
 *
 *   scratch_end(scratch_t*)   — rewind the parent arena
 *   scratch_allocator(scratch_t*) — get an allocator_t for user code
 */

typedef struct
{
    allocator_t parent_alloc; /* forwarded to users unchanged              */
    void *ctx;                /* same ctx as the parent arena              */
    void *saved_block;        /* block ptr at mark time (growing arena)    */
    size_t saved_offset;      /* offset at mark time                       */
    void (*pop)(void *ctx, void *saved_block, size_t saved_offset);
} scratch_t;

/* Rewind the parent arena to the position saved at scratch_begin. */
void scratch_end(scratch_t *s);

/* Return the parent allocator_t; pass this to user code. */
allocator_t scratch_allocator(scratch_t *s);

#endif /* ARENA_SCRATCH_H */
