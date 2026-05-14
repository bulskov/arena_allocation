#include <criterion/criterion.h>
#include <stdint.h>
#include <string.h>

#include "arena/fixed_arena.h"
#include "arena/growing_arena.h"
#include "arena/virtual_arena.h"

/* ── scratch over fixed_arena ───────────────────────────────────────────────
 */

TestSuite(scratch_fixed);

Test(scratch_fixed, begin_end_roundtrip) {
  uint8_t buf[512];
  fixed_arena_t a;
  fixed_arena_init(&a, buf, sizeof(buf));

  scratch_t s;
  fixed_arena_scratch_begin(&s, &a);
  mem_alloc(scratch_allocator(&s), 256, 1);
  cr_assert_gt(a.offset, 0u);
  scratch_end(&s);
  cr_assert_eq(a.offset, 0u);
}

Test(scratch_fixed, nested_lifo) {
  uint8_t buf[512];
  fixed_arena_t a;
  fixed_arena_init(&a, buf, sizeof(buf));

  scratch_t outer, inner;
  fixed_arena_scratch_begin(&outer, &a);
  mem_alloc(scratch_allocator(&outer), 64, 1);
  size_t after_outer_alloc = a.offset;

  fixed_arena_scratch_begin(&inner, &a);
  mem_alloc(scratch_allocator(&inner), 64, 1);

  scratch_end(&inner);
  cr_assert_eq(a.offset, after_outer_alloc); /* inner rewound */

  scratch_end(&outer);
  cr_assert_eq(a.offset, 0u); /* outer rewound */
}

Test(scratch_fixed, parent_alloc_unaffected_after_end) {
  uint8_t buf[512];
  fixed_arena_t a;
  fixed_arena_init(&a, buf, sizeof(buf));

  allocator_t main_alloc = fixed_arena_allocator(&a);
  uint8_t *p_before = mem_alloc(main_alloc, 32, 1);
  memset(p_before, 0xAB, 32);

  scratch_t s;
  fixed_arena_scratch_begin(&s, &a);
  mem_alloc(scratch_allocator(&s), 100, 1);
  scratch_end(&s);

  /* The pre-scratch allocation must be intact. */
  for (int i = 0; i < 32; ++i)
    cr_assert_eq(p_before[i], (uint8_t)0xAB);
}

/* ── scratch over growing_arena ─────────────────────────────────────────────
 */

TestSuite(scratch_growing);

Test(scratch_growing, begin_end_empty_arena) {
  growing_arena_t a;
  growing_arena_init(&a, 256);

  scratch_t s;
  growing_arena_scratch_begin(&s, &a);
  /* Force block allocation inside scratch. */
  mem_alloc(scratch_allocator(&s), 512, 1);
  scratch_end(&s);

  /* Arena should be back to empty. */
  cr_assert_null(a.head);

  growing_arena_destroy(&a);
}

Test(scratch_growing, blocks_added_in_scratch_are_freed) {
  growing_arena_t a;
  growing_arena_init(&a, 64);

  allocator_t main_alloc = growing_arena_allocator(&a);
  mem_alloc(main_alloc, 32, 1); /* one live block */
  growing_arena_block_t *head_before = a.head;

  scratch_t s;
  growing_arena_scratch_begin(&s, &a);
  for (int i = 0; i < 6; ++i)
    mem_alloc(scratch_allocator(&s), 64, 1); /* forces new blocks */
  scratch_end(&s);

  cr_assert_eq(a.head, head_before);
  cr_assert_null(a.head->next);

  growing_arena_destroy(&a);
}

/* ── scratch over virtual_arena ─────────────────────────────────────────────
 */

TestSuite(scratch_virtual);

Test(scratch_virtual, begin_end_roundtrip) {
  virtual_arena_t a;
  cr_assert_eq(virtual_arena_init(&a, 4 * 1024 * 1024, 64 * 1024), 0);

  scratch_t s;
  virtual_arena_scratch_begin(&s, &a);
  mem_alloc(scratch_allocator(&s), 128 * 1024, 1);
  size_t committed_peak = a.committed;
  cr_assert_gt(committed_peak, 0u);

  scratch_end(&s);
  cr_assert_eq(a.offset, 0u);
  cr_assert_lt(a.committed, committed_peak);

  virtual_arena_destroy(&a);
}

Test(scratch_virtual, parent_retains_its_pages) {
  virtual_arena_t a;
  cr_assert_eq(virtual_arena_init(&a, 4 * 1024 * 1024, 64 * 1024), 0);

  allocator_t main_alloc = virtual_arena_allocator(&a);
  uint8_t *p = mem_alloc(main_alloc, 64, 1);
  memset(p, 0x9E, 64);
  scratch_t s;
  virtual_arena_scratch_begin(&s, &a);
  mem_alloc(scratch_allocator(&s), 200 * 1024, 1);
  scratch_end(&s);

  /*
   * scratch_end decommits pages beyond the saved offset — correct behaviour.
   * The page covering p is still committed, so the data must be intact.
   */
  cr_assert_geq(a.committed, (size_t)((p + 64) - a.base));
  for (int i = 0; i < 64; ++i)
    cr_assert_eq(p[i], (uint8_t)0x9E);

  virtual_arena_destroy(&a);
}
