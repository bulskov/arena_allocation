#include "arena/virtual_arena.h"
#include "platform.h"
#include <string.h>

static inline size_t align_up(size_t val, size_t align) {
  return (val + align - 1) & ~(align - 1);
}

/* Ensure at least `required` bytes are committed. */
static int ensure_committed(virtual_arena_t *a, size_t required) {
  if (required <= a->committed)
    return 0;
  if (required > a->reserved)
    return -1;

  size_t want = align_up(required, a->commit_chunk);
  if (want > a->reserved)
    want = a->reserved;

  if (mem_commit(a->base + a->committed, want - a->committed) != 0)
    return -1;
  a->committed = want;
  return 0;
}

/* ---------- creator API -------------------------------------------------- */

int virtual_arena_init(virtual_arena_t *a, size_t reserved_size,
                       size_t commit_chunk) {
  size_t page = mem_page_size();
  a->reserved = align_up(reserved_size, page);
  a->commit_chunk = align_up(commit_chunk, page);
  a->committed = 0;
  a->offset = 0;
  a->base = (uint8_t *)mem_reserve(a->reserved);
  return a->base ? 0 : -1;
}

void virtual_arena_destroy(virtual_arena_t *a) {
  mem_release(a->base, a->reserved);
  a->base = NULL;
  a->reserved = 0;
  a->committed = 0;
  a->offset = 0;
}

void virtual_arena_reset(virtual_arena_t *a) {
  if (a->committed > 0) {
    mem_decommit(a->base, a->committed);
    a->committed = 0;
  }
  a->offset = 0;
}

/* ---------- vtable -------------------------------------------------------- */

static void *virtual_alloc(void *ctx, size_t size, size_t align) {
  virtual_arena_t *a = (virtual_arena_t *)ctx;
  size_t aligned = align_up(a->offset, align);
  size_t end = aligned + size;
  if (ensure_committed(a, end) != 0)
    return NULL;
  void *ptr = a->base + aligned;
  a->offset = end;
  return ptr;
}

static void *virtual_realloc(void *ctx, void *ptr, size_t old_size,
                             size_t new_size, size_t align) {
  virtual_arena_t *a = (virtual_arena_t *)ctx;
  uint8_t *p = (uint8_t *)ptr;

  /* In-place: ptr is the last allocation. */
  if (p + old_size == a->base + a->offset) {
    size_t base_off = (size_t)(p - a->base);
    size_t end = base_off + new_size;
    if (ensure_committed(a, end) != 0)
      return NULL;
    a->offset = end;
    return ptr;
  }

  void *dst = virtual_alloc(ctx, new_size, align);
  if (!dst)
    return NULL;
  size_t copy = old_size < new_size ? old_size : new_size;
  memcpy(dst, ptr, copy);
  return dst;
}

static void virtual_free(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)ptr;
  (void)size; /* bump allocator: no-op */
}

static const allocator_vtable_t virtual_arena_vtable = {
    .alloc = virtual_alloc,
    .realloc = virtual_realloc,
    .free = virtual_free,
};

allocator_t virtual_arena_allocator(virtual_arena_t *a) {
  return (allocator_t){.vt = &virtual_arena_vtable, .ctx = a};
}

/* ---------- scratch ------------------------------------------------------- */

/*
 * On pop: decommit any pages that were committed after the mark, then rewind
 * the offset. This returns physical memory to the OS immediately.
 */
static void virtual_pop(void *ctx, void *saved_block, size_t saved_offset) {
  virtual_arena_t *a = (virtual_arena_t *)ctx;
  size_t page = mem_page_size();
  (void)saved_block;

  /* Keep committed pages that cover [0, saved_offset). */
  size_t keep = align_up(saved_offset, page);
  if (keep < a->committed) {
    mem_decommit(a->base + keep, a->committed - keep);
    a->committed = keep;
  }
  a->offset = saved_offset;
}

void virtual_arena_scratch_begin(scratch_t *s, virtual_arena_t *a) {
  s->parent_alloc = virtual_arena_allocator(a);
  s->ctx = a;
  s->saved_block = NULL;
  s->saved_offset = a->offset;
  s->pop = virtual_pop;
}
