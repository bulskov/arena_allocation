#include "arena/fixed_arena.h"
#include <string.h>

static inline size_t align_up(size_t val, size_t align) {
  return (val + align - 1) & ~(align - 1);
}

/* ---------- creator API -------------------------------------------------- */

void fixed_arena_init(fixed_arena_t *a, void *buf, size_t size) {
  a->base = (uint8_t *)buf;
  a->size = size;
  a->offset = 0;
}

void fixed_arena_reset(fixed_arena_t *a) { a->offset = 0; }

/* ---------- vtable -------------------------------------------------------- */

static void *fixed_alloc(void *ctx, size_t size, size_t align) {
  fixed_arena_t *a = (fixed_arena_t *)ctx;
  uintptr_t base = (uintptr_t)a->base;
  uintptr_t adr = align_up(base + a->offset, align);
  size_t off = (size_t)(adr - base) + size;
  if (off > a->size)
    return NULL;
  a->offset = off;
  return (void *)adr;
}

static void *fixed_realloc(void *ctx, void *ptr, size_t old_size,
                           size_t new_size, size_t align) {
  if (!ptr)
    return fixed_alloc(ctx, new_size, align);
  fixed_arena_t *a = (fixed_arena_t *)ctx;
  uint8_t *p = (uint8_t *)ptr;

  /* In-place: ptr is the last allocation — just extend or shrink. */
  if (p + old_size == a->base + a->offset) {
    size_t base_off = (size_t)(p - a->base);
    if (base_off + new_size > a->size)
      return NULL;
    a->offset = base_off + new_size;
    return ptr;
  }

  /* General: alloc + copy; old allocation is abandoned (no-op free). */
  void *dst = fixed_alloc(ctx, new_size, align);
  if (!dst)
    return NULL;
  size_t copy = old_size < new_size ? old_size : new_size;
  memcpy(dst, ptr, copy);
  return dst;
}

static void fixed_free(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)ptr;
  (void)size; /* bump allocator: no-op */
}

static const allocator_vtable_t fixed_arena_vtable = {
    .alloc = fixed_alloc,
    .realloc = fixed_realloc,
    .free = fixed_free,
};

allocator_t fixed_arena_allocator(fixed_arena_t *a) {
  return (allocator_t){.vt = &fixed_arena_vtable, .ctx = a};
}

arena_stats_t fixed_arena_stats(const fixed_arena_t *a) {
  return (arena_stats_t){
      .used = a->offset,
      .capacity = a->size,
      .committed = a->size,
      .reserved = a->size,
  };
}

/* ---------- scratch ------------------------------------------------------- */

static void fixed_pop(void *ctx, void *saved_block, size_t saved_offset) {
  (void)saved_block;
  ((fixed_arena_t *)ctx)->offset = saved_offset;
}

void fixed_arena_scratch_begin(scratch_t *s, fixed_arena_t *a) {
  s->parent_alloc = fixed_arena_allocator(a);
  s->ctx = a;
  s->saved_block = NULL;
  s->saved_offset = a->offset;
  s->pop = fixed_pop;
}
