#include <criterion/criterion.h>
#include <stdint.h>
#include <string.h>

#include "arena/stack_arena.h"

#define CAPACITY 4096
#define MIN_ALIGN 8

static stack_arena_t arena;

static void setup(void) {
  cr_assert_eq(stack_arena_init(&arena, CAPACITY, MIN_ALIGN), 0);
}
static void teardown(void) { stack_arena_destroy(&arena); }

TestSuite(stack_arena, .init = setup, .fini = teardown);

/* ── basic allocation ───────────────────────────────────────────────────────
 */

Test(stack_arena, alloc_returns_nonnull) {
  void *p = mem_alloc(stack_arena_allocator(&arena), 16, 1);
  cr_assert_not_null(p);
}

Test(stack_arena, alloc_writes_are_readable) {
  allocator_t a = stack_arena_allocator(&arena);
  uint8_t *p = mem_alloc(a, 16, 1);
  cr_assert_not_null(p);
  memset(p, 0xAB, 16);
  for (int i = 0; i < 16; ++i)
    cr_assert_eq(p[i], (uint8_t)0xAB);
}

Test(stack_arena, alloc_oom_returns_null) {
  void *p = mem_alloc(stack_arena_allocator(&arena), CAPACITY + 1, 1);
  cr_assert_null(p);
}

/* ── slot rounding ──────────────────────────────────────────────────────────
 */

Test(stack_arena, slots_are_multiples_of_min_align) {
  allocator_t a = stack_arena_allocator(&arena);
  mem_alloc(a, 1, 1);
  /* With min_align=8, each slot is 8 bytes; offset must be a multiple. */
  cr_assert_eq(arena.offset % MIN_ALIGN, 0u);
  mem_alloc(a, 3, 1);
  cr_assert_eq(arena.offset % MIN_ALIGN, 0u);
  mem_alloc(a, 7, 1);
  cr_assert_eq(arena.offset % MIN_ALIGN, 0u);
}

Test(stack_arena, consecutive_slots_are_contiguous) {
  allocator_t a = stack_arena_allocator(&arena);
  uint8_t *p1 = mem_alloc(a, 1, 1); /* slot = 8 */
  uint8_t *p2 = mem_alloc(a, 1, 1); /* slot = 8 */
  cr_assert_eq((uintptr_t)p2 - (uintptr_t)p1, (uintptr_t)MIN_ALIGN);
}

/* ── LIFO free: single pop ──────────────────────────────────────────────────
 */

Test(stack_arena, free_top_rewinds_offset) {
  allocator_t a = stack_arena_allocator(&arena);
  void *p = mem_alloc(a, 16, 1);
  size_t after = arena.offset;
  (void)after;
  mem_free(a, p, 16);
  cr_assert_eq(arena.offset, 0u);
}

Test(stack_arena, freed_slot_is_reused) {
  allocator_t a = stack_arena_allocator(&arena);
  void *p1 = mem_alloc(a, 16, 1);
  mem_free(a, p1, 16);
  void *p2 = mem_alloc(a, 16, 1);
  cr_assert_eq(p1, p2);
}

/* ── LIFO free: multiple pops ───────────────────────────────────────────────
 */

Test(stack_arena, multi_level_lifo_free) {
  allocator_t a = stack_arena_allocator(&arena);
  uint8_t *p1 = mem_alloc(a, 3, 1); /* slot=8, offset=8  */
  uint8_t *p2 = mem_alloc(a, 5, 1); /* slot=8, offset=16 */
  uint8_t *p3 = mem_alloc(a, 7, 1); /* slot=8, offset=24 */

  memset(p1, 0x11, 3);
  memset(p2, 0x22, 5);
  memset(p3, 0x33, 7);

  mem_free(a, p3, 7);
  cr_assert_eq(arena.offset, 16u);

  mem_free(a, p2, 5);
  cr_assert_eq(arena.offset, 8u);

  mem_free(a, p1, 3);
  cr_assert_eq(arena.offset, 0u);
}

Test(stack_arena, freed_slots_restore_content) {
  allocator_t a = stack_arena_allocator(&arena);
  uint8_t *p1 = mem_alloc(a, 8, 1);
  uint8_t *p2 = mem_alloc(a, 8, 1);
  memset(p1, 0xAA, 8);
  memset(p2, 0xBB, 8);

  mem_free(a, p2, 8);
  /* p1 must still be intact after popping p2 */
  for (int i = 0; i < 8; ++i)
    cr_assert_eq(p1[i], (uint8_t)0xAA);
}

/* ── LIFO violation: non-top free is a no-op ─────────────────────────────── */

Test(stack_arena, free_non_top_is_noop) {
  allocator_t a = stack_arena_allocator(&arena);
  void *p1 = mem_alloc(a, 8, 1);
  void *p2 = mem_alloc(a, 8, 1);
  size_t before = arena.offset;

  mem_free(a, p1, 8); /* p1 is NOT the top — should be a no-op */
  cr_assert_eq(arena.offset, before);

  (void)p2;
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

Test(stack_arena, reset_rewinds_to_zero) {
  allocator_t a = stack_arena_allocator(&arena);
  for (int i = 0; i < 10; ++i)
    mem_alloc(a, 8, 1);
  stack_arena_reset(&arena);
  cr_assert_eq(arena.offset, 0u);
}

Test(stack_arena, reset_allows_full_reuse) {
  allocator_t a = stack_arena_allocator(&arena);
  void *p1 = mem_alloc(a, 32, 1);
  stack_arena_reset(&arena);
  void *p2 = mem_alloc(a, 32, 1);
  cr_assert_eq(p1, p2);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

Test(stack_arena, realloc_inplace_top) {
  allocator_t a = stack_arena_allocator(&arena);
  uint8_t *p = mem_alloc(a, 8, 1);
  memset(p, 0xCC, 8);
  uint8_t *p2 = mem_realloc(a, p, 8, 16, 1);
  cr_assert_eq(p, p2);
  for (int i = 0; i < 8; ++i)
    cr_assert_eq(p2[i], (uint8_t)0xCC);
}

Test(stack_arena, realloc_general_preserves_content) {
  allocator_t a = stack_arena_allocator(&arena);
  uint8_t *p1 = mem_alloc(a, 8, 1);
  memset(p1, 0xDD, 8);
  mem_alloc(a, 8, 1); /* p1 no longer the top */
  uint8_t *p2 = mem_realloc(a, p1, 8, 8, 1);
  cr_assert_not_null(p2);
  cr_assert_neq(p1, p2);
  for (int i = 0; i < 8; ++i)
    cr_assert_eq(p2[i], (uint8_t)0xDD);
}

/* ── capacity ───────────────────────────────────────────────────────────────
 */

Test(stack_arena, fills_to_capacity) {
  allocator_t a = stack_arena_allocator(&arena);
  /* alloc in MIN_ALIGN-sized steps until full */
  size_t steps = arena.capacity / MIN_ALIGN;
  for (size_t i = 0; i < steps; ++i) {
    void *p = mem_alloc(a, 1, 1);
    cr_assert_not_null(p, "alloc %zu returned NULL", i);
  }
  void *p = mem_alloc(a, 1, 1);
  cr_assert_null(p);
}
