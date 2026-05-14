#include "arena/growing_arena.h"
#include "platform.h"
#include <string.h>

static inline size_t align_up(size_t val, size_t align) {
  return (val + align - 1) & ~(align - 1);
}

static inline uint8_t *block_data(growing_arena_block_t *b) {
  return (uint8_t *)(b + 1);
}

static growing_arena_block_t *new_block(size_t min_data, size_t page_size) {
  size_t header = sizeof(growing_arena_block_t);
  size_t total = align_up(header + min_data, page_size);
  growing_arena_block_t *b = (growing_arena_block_t *)mem_map(total);
  if (!b)
    return NULL;
  b->next = NULL;
  b->size = total - header;
  b->offset = 0;
  return b;
}

static void free_block(growing_arena_block_t *b) {
  mem_unmap(b, sizeof(growing_arena_block_t) + b->size);
}

/* ---------- creator API -------------------------------------------------- */

void growing_arena_init(growing_arena_t *a, size_t block_size) {
  a->head = NULL;
  a->block_size = align_up(block_size, mem_page_size());
}

void growing_arena_destroy(growing_arena_t *a) {
  growing_arena_block_t *b = a->head;
  while (b) {
    growing_arena_block_t *next = b->next;
    free_block(b);
    b = next;
  }
  a->head = NULL;
}

void growing_arena_reset(growing_arena_t *a) {
  if (!a->head)
    return;
  /* Free all blocks except the current head; reset head's offset. */
  growing_arena_block_t *b = a->head->next;
  while (b) {
    growing_arena_block_t *next = b->next;
    free_block(b);
    b = next;
  }
  a->head->next = NULL;
  a->head->offset = 0;
}

/* ---------- vtable -------------------------------------------------------- */

static void *growing_alloc(void *ctx, size_t size, size_t align) {
  growing_arena_t *a = (growing_arena_t *)ctx;

  /* Try current block. */
  if (a->head) {
    uintptr_t base = (uintptr_t)block_data(a->head);
    uintptr_t adr  = align_up(base + a->head->offset, align);
    size_t    off  = (size_t)(adr - base) + size;
    if (off <= a->head->size) {
      a->head->offset = off;
      return (void *)adr;
    }
  }

  /* Current block exhausted — allocate a new one. */
  size_t min = size > a->block_size ? size : a->block_size;
  growing_arena_block_t *b = new_block(min, mem_page_size());
  if (!b)
    return NULL;

  b->next = a->head;
  a->head = b;

  uintptr_t base = (uintptr_t)block_data(b);
  uintptr_t adr  = align_up(base, align);
  b->offset      = (size_t)(adr - base) + size;
  return (void *)adr;
}

static void *growing_realloc(void *ctx, void *ptr, size_t old_size,
                             size_t new_size, size_t align) {
  growing_arena_t *a = (growing_arena_t *)ctx;

  /* In-place: ptr is the last allocation in the current block. */
  if (a->head) {
    uint8_t *data = block_data(a->head);
    uint8_t *p = (uint8_t *)ptr;
    if (p + old_size == data + a->head->offset) {
      size_t base_off = (size_t)(p - data);
      if (base_off + new_size <= a->head->size) {
        a->head->offset = base_off + new_size;
        return ptr;
      }
    }
  }

  void *dst = growing_alloc(ctx, new_size, align);
  if (!dst)
    return NULL;
  size_t copy = old_size < new_size ? old_size : new_size;
  memcpy(dst, ptr, copy);
  return dst;
}

static void growing_free(void *ctx, void *ptr, size_t size) {
  (void)ctx;
  (void)ptr;
  (void)size; /* bump allocator: no-op */
}

static const allocator_vtable_t growing_arena_vtable = {
    .alloc = growing_alloc,
    .realloc = growing_realloc,
    .free = growing_free,
};

allocator_t growing_arena_allocator(growing_arena_t *a) {
  return (allocator_t){.vt = &growing_arena_vtable, .ctx = a};
}

/* ---------- scratch ------------------------------------------------------- */

/*
 * On pop: free every block that was prepended after the mark, then restore the
 * saved block's offset. The chain before saved_block is left untouched because
 * it existed before the scratch was opened.
 */
static void growing_pop(void *ctx, void *saved_block, size_t saved_offset) {
  growing_arena_t *a = (growing_arena_t *)ctx;
  growing_arena_block_t *mark = (growing_arena_block_t *)saved_block;

  growing_arena_block_t *b = a->head;
  while (b && b != mark) {
    growing_arena_block_t *next = b->next;
    free_block(b);
    b = next;
  }

  a->head = mark;
  if (mark)
    mark->offset = saved_offset;
}

void growing_arena_scratch_begin(scratch_t *s, growing_arena_t *a) {
  s->parent_alloc = growing_arena_allocator(a);
  s->ctx = a;
  s->saved_block = a->head;
  s->saved_offset = a->head ? a->head->offset : 0;
  s->pop = growing_pop;
}
