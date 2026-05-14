#ifndef ARENA_VIRTUAL_ARENA_H
#define ARENA_VIRTUAL_ARENA_H

#include "allocator.h"
#include "scratch.h"
#include <stddef.h>
#include <stdint.h>

/*
 * virtual_arena_t — bump allocator over a large reserved virtual address range.
 *
 * The full virtual range is reserved (PROT_NONE / MEM_RESERVE) at init time
 * but no physical pages are committed. Pages are committed in chunks of
 * commit_chunk bytes as allocations demand them. On reset, committed pages are
 * decommitted so the physical memory is returned to the OS while the virtual
 * reservation is kept.
 *
 * This is the preferred arena for large, unpredictably-sized working sets: the
 * address space is cheap; physical pages are only paid for while in use.
 *
 * Creator API:
 *   virtual_arena_init         — reserve VA range (no physical pages yet)
 *   virtual_arena_destroy      — decommit + release the entire VA range
 *   virtual_arena_reset        — decommit all pages, keep VA reservation
 *   virtual_arena_allocator    — produce an allocator_t for user code
 *   virtual_arena_scratch_begin — begin a temporary sub-scope
 */

typedef struct
{
    uint8_t *base;
    size_t reserved;     /* total VA bytes reserved                  */
    size_t committed;    /* bytes currently backed by physical pages */
    size_t offset;       /* bump pointer                             */
    size_t commit_chunk; /* granularity for committing new pages     */
} virtual_arena_t;

/*
 * reserved_size  — total virtual address range to reserve.
 * commit_chunk   — how many bytes to commit at a time (rounded to page size).
 * Returns 0 on success, -1 on failure.
 */
int virtual_arena_init(
    virtual_arena_t *a, size_t reserved_size, size_t commit_chunk);
void virtual_arena_destroy(virtual_arena_t *a);
void virtual_arena_reset(virtual_arena_t *a);
allocator_t virtual_arena_allocator(virtual_arena_t *a);
allocator_t virtual_arena_allocator_new(
    virtual_arena_t *a, size_t reserved_size, size_t commit_chunk);
void virtual_arena_scratch_begin(scratch_t *s, virtual_arena_t *a);
arena_stats_t virtual_arena_stats(const virtual_arena_t *a);

#endif /* ARENA_VIRTUAL_ARENA_H */
