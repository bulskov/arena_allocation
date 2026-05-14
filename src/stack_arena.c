#include "arena/stack_arena.h"
#include "platform.h"
#include <string.h>

static inline size_t align_up(size_t v, size_t a) {
  return (v + a - 1) & ~(a - 1);
}

/* ---------- creator API -------------------------------------------------- */

int stack_arena_init(stack_arena_t *a, size_t capacity, size_t min_align) {
  size_t page = mem_page_size();
  size_t total = align_up(capacity, page);
  uint8_t *base = (uint8_t *)mem_map(total);
  if (!base)
    return -1;
  a->base = base;
  a->capacity = total;
  a->offset = 0;
  a->min_align = min_align < 1 ? 1 : min_align;
  return 0;
}

void stack_arena_destroy(stack_arena_t *a) {
  mem_unmap(a->base, a->capacity);
  a->base = NULL;
  a->offset = 0;
}

void stack_arena_reset(stack_arena_t *a) { a->offset = 0; }

/* ---------- vtable -------------------------------------------------------- */

/*
 * Both start address and slot size are rounded to min_align.
 * This guarantees consecutive slots are contiguous (no inter-slot gaps),
 * which makes the LIFO free check exact without any per-allocation header.
 */

static void *stack_alloc(void *ctx, size_t size, size_t align) {
  stack_arena_t *a = (stack_arena_t *)ctx;
  (void)align; /* clamped to min_align */
  uintptr_t base = (uintptr_t)a->base;
  uintptr_t adr = align_up(base + a->offset, a->min_align);
  size_t slot = align_up(size, a->min_align);
  size_t new_off = (size_t)(adr - base) + slot;
  if (new_off > a->capacity)
    return NULL;
  a->offset = new_off;
  return (void *)adr;
}

static void *stack_realloc(void *ctx, void *ptr, size_t old_size,
                           size_t new_size, size_t align) {
  if (!ptr) return stack_alloc(ctx, new_size, align);
  stack_arena_t *a = (stack_arena_t *)ctx;
  (void)align;
  uintptr_t base = (uintptr_t)a->base;
  uintptr_t p = (uintptr_t)ptr;
  size_t old_slot = align_up(old_size, a->min_align);

  /* In-place: ptr is the current top of the stack. */
  if (p + old_slot == base + a->offset) {
    size_t new_slot = align_up(new_size, a->min_align);
    size_t new_off = (size_t)(p - base) + new_slot;
    if (new_off > a->capacity)
      return NULL;
    a->offset = new_off;
    return ptr;
  }

  /* General: alloc new block, copy; old slot stays (not the top). */
  void *dst = stack_alloc(ctx, new_size, 1);
  if (!dst)
    return NULL;
  size_t copy = old_size < new_size ? old_size : new_size;
  memcpy(dst, ptr, copy);
  return dst;
}

static void stack_free(void *ctx, void *ptr, size_t size) {
  if (!ptr) return;
  stack_arena_t *a = (stack_arena_t *)ctx;
  uintptr_t base = (uintptr_t)a->base;
  uintptr_t p = (uintptr_t)ptr;
  size_t slot = align_up(size, a->min_align);

  if (p + slot == base + a->offset)
    a->offset = (size_t)(p - base);
  /* else: not the top — LIFO violation, silent no-op */
}

static const allocator_vtable_t stack_arena_vtable = {
    .alloc = stack_alloc,
    .realloc = stack_realloc,
    .free = stack_free,
};

allocator_t stack_arena_allocator(stack_arena_t *a) {
  return (allocator_t){.vt = &stack_arena_vtable, .ctx = a};
}

arena_stats_t stack_arena_stats(const stack_arena_t *a) {
  return (arena_stats_t){
      .used      = a->offset,
      .capacity  = a->capacity,
      .committed = a->capacity,
      .reserved  = a->capacity,
  };
}
