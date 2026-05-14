#ifndef ARENA_ALLOCATOR_H
#define ARENA_ALLOCATOR_H

#include <stddef.h>

/*
 * allocator_t — the only interface users ever see.
 *
 * A fat pointer: vtable + opaque context. Passed by value (two words).
 * The creator owns the concrete arena and its lifetime; users only alloc,
 * realloc, and free through this handle.
 *
 * Contract:
 *   alloc   — return size bytes aligned to align, or NULL on failure.
 *   realloc — return new_size bytes preserving min(old_size, new_size) bytes
 *             of content. Pointer may change. Returns NULL on failure.
 *             Implementations may extend in-place when ptr is the most recent
 *             allocation; callers must never rely on pointer stability.
 *   free    — release ptr (size bytes). May be a no-op for bump allocators.
 *
 * align must be a power of two and >= 1.
 * size / new_size must be > 0.
 */

typedef struct {
  void *(*alloc)(void *ctx, size_t size, size_t align);
  void *(*realloc)(void *ctx, void *ptr, size_t old_size, size_t new_size,
                   size_t align);
  void (*free)(void *ctx, void *ptr, size_t size);
} allocator_vtable_t;

typedef struct {
  const allocator_vtable_t *vt;
  void *ctx;
} allocator_t;

static inline void *mem_alloc(allocator_t a, size_t size, size_t align) {
  return a.vt->alloc(a.ctx, size, align);
}

static inline void *mem_realloc(allocator_t a, void *ptr, size_t old_size,
                                size_t new_size, size_t align) {
  return a.vt->realloc(a.ctx, ptr, old_size, new_size, align);
}

static inline void mem_free(allocator_t a, void *ptr, size_t size) {
  a.vt->free(a.ctx, ptr, size);
}

#endif /* ARENA_ALLOCATOR_H */
