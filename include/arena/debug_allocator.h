#ifndef ARENA_DEBUG_ALLOCATOR_H
#define ARENA_DEBUG_ALLOCATOR_H

#include "allocator.h"
#include <stddef.h>

/*
 * debug_allocator_t — wraps any allocator_t with diagnostic instrumentation.
 *
 * Fills allocated memory with 0xCD (uninitialised sentinel) so unintentional
 * reads of uninitialised data produce a recognisable pattern.  Fills freed
 * memory with 0xDD (use-after-free sentinel) so accesses through stale
 * pointers are detectable.
 *
 * Tracks five counters that form a lightweight allocation profile:
 *   alloc_count  — number of successful alloc / realloc(NULL) calls
 *   free_count   — number of free calls (excluding free(NULL) no-ops)
 *   bytes_total  — cumulative bytes requested via alloc / realloc(NULL)
 *   bytes_live   — bytes currently in active use
 *   bytes_peak   — high-watermark of bytes_live
 *
 * The wrapper does not own the inner allocator; the inner arena must outlive
 * the debug_allocator_t.  There is no destroy function.
 *
 * Creator API:
 *   debug_allocator_init      — attach to an inner allocator
 *   debug_allocator_reset     — zero all counters (does not touch inner arena)
 *   debug_allocator_allocator — produce an allocator_t for user code
 *   debug_allocator_stats     — snapshot of instrumentation data
 */

typedef struct
{
    allocator_t inner;
    size_t alloc_count;
    size_t free_count;
    size_t bytes_total; /* cumulative bytes from alloc / realloc(NULL) */
    size_t bytes_live;  /* currently live bytes */
    size_t bytes_peak;  /* high-watermark of bytes_live */
} debug_allocator_t;

typedef struct
{
    size_t alloc_count;
    size_t free_count;
    size_t bytes_total;
    size_t bytes_live;
    size_t bytes_peak;
} debug_allocator_stats_t;

void debug_allocator_init(debug_allocator_t *d, allocator_t inner);
void debug_allocator_reset(debug_allocator_t *d);
allocator_t debug_allocator_allocator(debug_allocator_t *d);
debug_allocator_stats_t debug_allocator_stats(const debug_allocator_t *d);

#endif /* ARENA_DEBUG_ALLOCATOR_H */
