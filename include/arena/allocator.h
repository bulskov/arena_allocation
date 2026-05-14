#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#include <stddef.h>
#include <string.h>

/*
 * allocator_t — the only interface users ever see.
 *
 * A fat pointer: vtable + opaque context. Passed by value (two words).
 * The creator owns the concrete arena and its lifetime; users only alloc,
 * realloc, and free through this handle.
 *
 * Contract:
 *   alloc(size, align)
 *     Return size bytes aligned to align, or NULL on failure.
 *
 *   realloc(ptr, old_size, new_size, align)
 *     Resize ptr, preserving min(old_size, new_size) bytes of content.
 *     ptr == NULL behaves as alloc(new_size, align).
 *     Returns NULL on failure; ptr remains valid in that case.
 *     Pointer may change; callers must not rely on pointer stability.
 *     Implementations may extend in-place when ptr is the most recent
 *     allocation.
 *
 *   free(ptr, size)
 *     Release ptr (size bytes).  ptr == NULL is a no-op.
 *     May be a no-op for bump allocators.
 *
 * align must be a power of two and >= 1.
 * size / new_size must be > 0.
 *
 * A zero-initialised allocator_t (vt == NULL) is a valid null allocator:
 * alloc/realloc return NULL, free is a no-op.  Use ALLOCATOR_NULL.
 */

typedef struct
{
    void *(*alloc)(void *ctx, size_t size, size_t align);
    void *(*realloc)(
        void *ctx, void *ptr, size_t old_size, size_t new_size, size_t align);
    void (*free)(void *ctx, void *ptr, size_t size);
} allocator_vtable_t;

typedef struct
{
    const allocator_vtable_t *vt;
    void *ctx;
} allocator_t;

/*
 * arena_stats_t — snapshot returned by per-arena *_stats() functions.
 *
 *   used      — bytes currently in active use.
 *   capacity  — total backing capacity in bytes.
 *   committed — committed physical pages; equals capacity except for
 *               virtual_arena where pages are committed on demand.
 *   reserved  — reserved virtual address range; equals capacity except for
 *               virtual_arena.
 */
typedef struct
{
    size_t used;
    size_t capacity;
    size_t committed;
    size_t reserved;
} arena_stats_t;

/* Zero-initialised allocator: alloc/realloc return NULL, free is a no-op. */
#define ALLOCATOR_NULL ((allocator_t){0})

/* ---------- inline wrappers ----------------------------------------------- */

static inline void *mem_alloc(allocator_t a, size_t size, size_t align)
{
    if (!a.vt)
        return NULL;
    return a.vt->alloc(a.ctx, size, align);
}

static inline void *mem_realloc(
    allocator_t a, void *ptr, size_t old_size, size_t new_size, size_t align)
{
    if (!a.vt)
        return NULL;
    return a.vt->realloc(a.ctx, ptr, old_size, new_size, align);
}

static inline void mem_free(allocator_t a, void *ptr, size_t size)
{
    if (a.vt)
        a.vt->free(a.ctx, ptr, size);
}

/* Allocate size bytes, zero-initialised. */
static inline void *mem_calloc(allocator_t a, size_t size, size_t align)
{
    void *p = mem_alloc(a, size, align);
    if (p)
        memset(p, 0, size);
    return p;
}

#endif /* ARENA_ALLOCATOR_H */
