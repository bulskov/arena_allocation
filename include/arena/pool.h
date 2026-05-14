#ifndef ARENA_POOL_H
#define ARENA_POOL_H

#include "allocator.h"
#include <stddef.h>
#include <stdint.h>

/*
 * pool_t — O(1) fixed-size object allocator backed by a single mmap'd block.
 *
 * All slots are the same size (rounded up to pointer alignment). A free-list
 * threads through unused slots, so alloc and free are both O(1).
 *
 * realloc:
 *   new_size <= slot_size  → in-place, returns the same pointer.
 *   new_size >  slot_size  → returns NULL; pools do not cross size boundaries.
 *
 * Creator API:
 *   pool_init      — mmap backing memory and build free list
 *   pool_destroy   — munmap backing memory
 *   pool_reset     — rebuild free list (all objects become invalid)
 *   pool_allocator — produce an allocator_t for user code
 */

typedef struct pool_free_node
{
    struct pool_free_node *next;
} pool_free_node_t;

typedef struct
{
    uint8_t *base;
    size_t slot_size; /* actual size of each slot (aligned)    */
    size_t capacity;  /* maximum simultaneous live objects     */
    size_t count;     /* currently allocated objects           */
    pool_free_node_t *free_list;
} pool_t;

/*
 * object_size — desired per-object size (>= 1).
 * capacity    — maximum number of simultaneously live objects.
 * Returns 0 on success, -1 on mmap failure.
 */
int pool_init(pool_t *p, size_t object_size, size_t capacity);
void pool_destroy(pool_t *p);
void pool_reset(pool_t *p);
allocator_t pool_allocator(pool_t *p);
allocator_t pool_allocator_new(pool_t *p, size_t object_size, size_t capacity);
arena_stats_t pool_stats(const pool_t *p);

#endif /* ARENA_POOL_H */
