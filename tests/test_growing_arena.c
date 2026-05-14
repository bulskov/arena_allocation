#include <criterion/criterion.h>
#include <stdint.h>
#include <string.h>

#include "arena/growing_arena.h"

#define BLOCK_SIZE 256

static growing_arena_t arena;

static void setup(void)
{
    growing_arena_init(&arena, BLOCK_SIZE);
}
static void teardown(void)
{
    growing_arena_destroy(&arena);
}

TestSuite(growing_arena, .init = setup, .fini = teardown);

/* ── basic allocation ───────────────────────────────────────────────────────
 */

Test(growing_arena, alloc_returns_nonnull)
{
    void *p = mem_alloc(growing_arena_allocator(&arena), 16, 1);
    cr_assert_not_null(p);
}

Test(growing_arena, alloc_writes_are_readable)
{
    allocator_t a = growing_arena_allocator(&arena);
    uint8_t *p = mem_alloc(a, 8, 1);
    cr_assert_not_null(p);
    memset(p, 0xBE, 8);
    for (int i = 0; i < 8; ++i)
        cr_assert_eq(p[i], (uint8_t)0xBE);
}

Test(growing_arena, alloc_advances_sequentially_within_block)
{
    allocator_t a = growing_arena_allocator(&arena);
    uint8_t *p1 = mem_alloc(a, 16, 1);
    uint8_t *p2 = mem_alloc(a, 16, 1);
    cr_assert_not_null(p1);
    cr_assert_not_null(p2);
    cr_assert_eq((uintptr_t)p2 - (uintptr_t)p1, 16u);
}

/* ── alignment ──────────────────────────────────────────────────────────────
 */

Test(growing_arena, alignment_8)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_alloc(a, 3, 1);
    void *p = mem_alloc(a, 8, 8);
    cr_assert_not_null(p);
    cr_assert_eq((uintptr_t)p % 8, 0u);
}

Test(growing_arena, alignment_16)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_alloc(a, 5, 1);
    void *p = mem_alloc(a, 32, 16);
    cr_assert_not_null(p);
    cr_assert_eq((uintptr_t)p % 16, 0u);
}

/* ── growth across blocks ───────────────────────────────────────────────────
 */

Test(growing_arena, grows_beyond_initial_block)
{
    allocator_t a = growing_arena_allocator(&arena);
    /* Allocate more than one block's worth. */
    for (int i = 0; i < 20; ++i)
    {
        void *p = mem_alloc(a, BLOCK_SIZE / 4, 1);
        cr_assert_not_null(p, "alloc %d returned NULL", i);
    }
}

Test(growing_arena, multiple_blocks_are_chained)
{
    allocator_t a = growing_arena_allocator(&arena);
    /*
     * block_size rounds up to page_size (~4 KB). Each new_block call produces
     * a usable region of (2*page - header) bytes, so a single page-sized alloc
     * fits in one block. Requesting block_size+1 guarantees each allocation
     * gets its own dedicated block.
     */
    mem_alloc(a, arena.block_size + 1, 1);
    mem_alloc(a, arena.block_size + 1, 1);
    cr_assert_not_null(arena.head);
    cr_assert_not_null(arena.head->next); /* at least two blocks */
}

Test(growing_arena, large_single_alloc)
{
    allocator_t a = growing_arena_allocator(&arena);
    /* Larger than block_size — should trigger a dedicated oversized block. */
    void *p = mem_alloc(a, BLOCK_SIZE * 4, 1);
    cr_assert_not_null(p);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

Test(growing_arena, reset_keeps_head_block)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_alloc(a, BLOCK_SIZE, 1);
    mem_alloc(a, BLOCK_SIZE, 1); /* two blocks */
    growing_arena_t *head_before = (void *)arena.head;
    growing_arena_reset(&arena);
    cr_assert_not_null(arena.head);
    cr_assert_null(arena.head->next); /* only one block remains */
    cr_assert_eq(arena.head->offset, 0u);
    (void)head_before;
}

Test(growing_arena, reset_allows_reuse)
{
    allocator_t a = growing_arena_allocator(&arena);
    for (int i = 0; i < 10; ++i)
        mem_alloc(a, BLOCK_SIZE / 2, 1);
    growing_arena_reset(&arena);
    void *p = mem_alloc(a, BLOCK_SIZE / 2, 1);
    cr_assert_not_null(p);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

Test(growing_arena, realloc_inplace_last_alloc)
{
    allocator_t a = growing_arena_allocator(&arena);
    uint8_t *p = mem_alloc(a, 16, 1);
    memset(p, 0xAA, 16);
    uint8_t *p2 = mem_realloc(a, p, 16, 32, 1);
    cr_assert_eq(p, p2);
    for (int i = 0; i < 16; ++i)
        cr_assert_eq(p2[i], (uint8_t)0xAA);
}

Test(growing_arena, realloc_general_preserves_content)
{
    allocator_t a = growing_arena_allocator(&arena);
    uint8_t *p1 = mem_alloc(a, 16, 1);
    memset(p1, 0x22, 16);
    mem_alloc(a, 8, 1); /* p1 is no longer last */
    uint8_t *p2 = mem_realloc(a, p1, 16, 16, 1);
    cr_assert_not_null(p2);
    cr_assert_neq(p1, p2);
    for (int i = 0; i < 16; ++i)
        cr_assert_eq(p2[i], (uint8_t)0x22);
}

/* ── free ───────────────────────────────────────────────────────────────────
 */

Test(growing_arena, free_is_noop_no_crash)
{
    allocator_t a = growing_arena_allocator(&arena);
    void *p = mem_alloc(a, 64, 1);
    mem_free(a, p, 64);
    void *q = mem_alloc(a, 64, 1);
    cr_assert_not_null(q);
}

/* ── scratch ────────────────────────────────────────────────────────────────
 */

Test(growing_arena, scratch_rewinds_to_saved_block_and_offset)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_alloc(a, BLOCK_SIZE / 2, 1); /* establish some state */

    scratch_t s;
    growing_arena_scratch_begin(&s, &arena);
    growing_arena_block_t *head_at_mark = arena.head;
    size_t offset_at_mark = arena.head ? arena.head->offset : 0;

    /* Force a new block inside the scratch. */
    mem_alloc(scratch_allocator(&s), BLOCK_SIZE, 1);
    mem_alloc(scratch_allocator(&s), BLOCK_SIZE, 1);

    scratch_end(&s);

    cr_assert_eq(arena.head, head_at_mark);
    cr_assert_eq(arena.head->offset, offset_at_mark);
}

Test(growing_arena, scratch_frees_extra_blocks)
{
    allocator_t a = growing_arena_allocator(&arena);
    scratch_t s;
    growing_arena_scratch_begin(&s, &arena);

    /* Force several new blocks. */
    for (int i = 0; i < 5; ++i)
        mem_alloc(scratch_allocator(&s), BLOCK_SIZE, 1);

    scratch_end(&s);

    /* Only the original NULL head (or the pre-scratch head) should remain. */
    cr_assert_null(arena.head); /* arena was empty before scratch */
}

/* ── NULL-ptr contract ──────────────────────────────────────────────────────
 */

Test(growing_arena, realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = growing_arena_allocator(&arena);
    void *p = mem_realloc(a, NULL, 0, 32, 1);
    cr_assert_not_null(p);
}

Test(growing_arena, free_null_is_noop)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_free(a, NULL, 0); /* must not crash */
}

/* ── reset_full ─────────────────────────────────────────────────────────────
 */

Test(growing_arena, reset_full_releases_all_blocks)
{
    allocator_t a = growing_arena_allocator(&arena);
    for (int i = 0; i < 5; ++i)
        mem_alloc(a, arena.block_size + 1, 1); /* one block per alloc */
    cr_assert_not_null(arena.head);

    growing_arena_reset_full(&arena);
    cr_assert_null(arena.head);
}

Test(growing_arena, reset_full_arena_is_reusable)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_alloc(a, BLOCK_SIZE * 4, 1);
    growing_arena_reset_full(&arena);

    void *p = mem_alloc(a, 64, 1);
    cr_assert_not_null(p);
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

Test(growing_arena, stats_used_grows_with_allocations)
{
    allocator_t a = growing_arena_allocator(&arena);
    arena_stats_t s0 = growing_arena_stats(&arena);
    cr_assert_eq(s0.used, 0u);
    cr_assert_eq(s0.capacity, 0u); /* no block yet */

    mem_alloc(a, 64, 1);
    arena_stats_t s1 = growing_arena_stats(&arena);
    cr_assert_geq(s1.used, 64u);
    cr_assert_geq(s1.capacity, (size_t)BLOCK_SIZE);
}
