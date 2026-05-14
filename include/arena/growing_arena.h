#ifndef ARENA_GROWING_ARENA_H
#define ARENA_GROWING_ARENA_H

#include "allocator.h"
#include "scratch.h"
#include <stddef.h>
#include <stdint.h>

/*
 * growing_arena_t — chained bump allocator that grows by mmap'ing new blocks.
 *
 * When the current block is exhausted a new block of at least block_size bytes
 * is mmap'd and prepended to the chain (head = newest block). All blocks are
 * released on destroy.
 *
 * Creator API:
 *   growing_arena_init         — initialise (no mmap yet; first on first alloc)
 *   growing_arena_destroy      — munmap all blocks
 *   growing_arena_reset        — rewind; keeps the head block, releases the
 * rest growing_arena_allocator    — produce an allocator_t for user code
 *   growing_arena_scratch_begin — begin a temporary sub-scope
 */

typedef struct growing_arena_block
{
    struct growing_arena_block *next;
    size_t size;   /* usable bytes after the header */
    size_t offset; /* bump pointer within usable bytes */
                   /* uint8_t data[] follows in memory */
} growing_arena_block_t;

typedef struct
{
    growing_arena_block_t *head; /* newest (current) block          */
    size_t block_size;           /* minimum usable bytes per block   */
} growing_arena_t;

void growing_arena_init(growing_arena_t *a, size_t block_size);
void growing_arena_destroy(growing_arena_t *a);
void growing_arena_reset(growing_arena_t *a);
void growing_arena_reset_full(growing_arena_t *a);
allocator_t growing_arena_allocator(growing_arena_t *a);
allocator_t growing_arena_allocator_new(growing_arena_t *a, size_t block_size);
void growing_arena_scratch_begin(scratch_t *s, growing_arena_t *a);
arena_stats_t growing_arena_stats(const growing_arena_t *a);

#endif /* ARENA_GROWING_ARENA_H */
