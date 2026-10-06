#include "ctt.h"

#include <stdint.h>
#include <string.h>

#include "arena/growing_arena.h"

#define BLOCK_SIZE 256

static growing_arena_t arena;

void ctt_before_each(void)
{
    growing_arena_init(&arena, BLOCK_SIZE);
}

void ctt_after_each(void)
{
    growing_arena_destroy(&arena);
}

/* ── basic allocation ───────────────────────────────────────────────────────
 */

TEST(alloc_returns_nonnull)
{
    void *p = mem_alloc(growing_arena_allocator(&arena), 16, 1);
    ASSERT_NOT_NULL(p);
}

TEST(alloc_writes_are_readable)
{
    allocator_t a = growing_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 8, 1);
    ASSERT_NOT_NULL(p);
    memset(p, 0xBE, 8);
    for (int i = 0; i < 8; ++i)
        ASSERT_EQ((uint8_t)0xBE, p[i]);
}

TEST(alloc_advances_sequentially_within_block)
{
    allocator_t a = growing_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 16, 1);
    ASSERT_NOT_NULL(p1);
    ASSERT_NOT_NULL(p2);
    ASSERT_EQ(16u, (uintptr_t)p2 - (uintptr_t)p1);
}

/* ── alignment ──────────────────────────────────────────────────────────────
 */

TEST(alignment_8)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_alloc(a, 3, 1);
    void *p = mem_alloc(a, 8, 8);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(0u, (uintptr_t)p % 8);
}

TEST(alignment_16)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_alloc(a, 5, 1);
    void *p = mem_alloc(a, 32, 16);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(0u, (uintptr_t)p % 16);
}

/* ── growth across blocks ───────────────────────────────────────────────────
 */

TEST(grows_beyond_initial_block)
{
    allocator_t a = growing_arena_allocator(&arena);
    /* Allocate more than one block's worth. */
    for (int i = 0; i < 20; ++i)
    {
        void *p = mem_alloc(a, BLOCK_SIZE / 4, 1);
        ASSERT_NOT_NULL(p);
    }
}

TEST(multiple_blocks_are_chained)
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
    ASSERT_NOT_NULL(arena.head);
    ASSERT_NOT_NULL(arena.head->next); /* at least two blocks */
}

TEST(large_single_alloc)
{
    allocator_t a = growing_arena_allocator(&arena);
    /* Larger than block_size — should trigger a dedicated oversized block. */
    void *p = mem_alloc(a, BLOCK_SIZE * 4, 1);
    ASSERT_NOT_NULL(p);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

TEST(reset_keeps_head_block)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_alloc(a, BLOCK_SIZE, 1);
    mem_alloc(a, BLOCK_SIZE, 1); /* two blocks */
    growing_arena_block_t *head_before = arena.head;
    growing_arena_reset(&arena);
    ASSERT_NOT_NULL(arena.head);
    ASSERT_NULL(arena.head->next); /* only one block remains */
    ASSERT_EQ(0u, arena.head->offset);
    (void)head_before;
}

TEST(reset_allows_reuse)
{
    allocator_t a = growing_arena_allocator(&arena);
    for (int i = 0; i < 10; ++i)
        mem_alloc(a, BLOCK_SIZE / 2, 1);
    growing_arena_reset(&arena);
    void *p = mem_alloc(a, BLOCK_SIZE / 2, 1);
    ASSERT_NOT_NULL(p);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

TEST(realloc_inplace_last_alloc)
{
    allocator_t a = growing_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p, 0xAA, 16);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 16, 32, 1);
    ASSERT_PTR_EQ(p2, p);
    for (int i = 0; i < 16; ++i)
        ASSERT_EQ((uint8_t)0xAA, p2[i]);
}

TEST(realloc_general_preserves_content)
{
    allocator_t a = growing_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p1, 0x22, 16);
    mem_alloc(a, 8, 1); /* p1 is no longer last */
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p1, 16, 16, 1);
    ASSERT_NOT_NULL(p2);
    ASSERT_PTR_NE(p2, p1);
    for (int i = 0; i < 16; ++i)
        ASSERT_EQ((uint8_t)0x22, p2[i]);
}

/* ── free ───────────────────────────────────────────────────────────────────
 */

TEST(free_is_noop_no_crash)
{
    allocator_t a = growing_arena_allocator(&arena);
    void *p = mem_alloc(a, 64, 1);
    mem_free(a, p, 64);
    void *q = mem_alloc(a, 64, 1);
    ASSERT_NOT_NULL(q);
}

/* ── scratch ────────────────────────────────────────────────────────────────
 */

TEST(scratch_rewinds_to_saved_block_and_offset)
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

    ASSERT_PTR_EQ(head_at_mark, arena.head);
    ASSERT_EQ(offset_at_mark, arena.head->offset);
}

TEST(scratch_frees_extra_blocks)
{
    scratch_t s;
    growing_arena_scratch_begin(&s, &arena);

    /* Force several new blocks. */
    for (int i = 0; i < 5; ++i)
        mem_alloc(scratch_allocator(&s), BLOCK_SIZE, 1);

    scratch_end(&s);

    /* Only the original NULL head (or the pre-scratch head) should remain. */
    ASSERT_NULL(arena.head); /* arena was empty before scratch */
}

/* ── NULL-ptr contract ──────────────────────────────────────────────────────
 */

TEST(realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = growing_arena_allocator(&arena);
    void *p = mem_realloc(a, NULL, 0, 32, 1);
    ASSERT_NOT_NULL(p);
}

TEST(free_null_is_noop)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_free(a, NULL, 0); /* must not crash */
}

/* ── reset_full ─────────────────────────────────────────────────────────────
 */

TEST(reset_full_releases_all_blocks)
{
    allocator_t a = growing_arena_allocator(&arena);
    for (int i = 0; i < 5; ++i)
        mem_alloc(a, arena.block_size + 1, 1); /* one block per alloc */
    ASSERT_NOT_NULL(arena.head);

    growing_arena_reset_full(&arena);
    ASSERT_NULL(arena.head);
}

TEST(reset_full_arena_is_reusable)
{
    allocator_t a = growing_arena_allocator(&arena);
    mem_alloc(a, BLOCK_SIZE * 4, 1);
    growing_arena_reset_full(&arena);

    void *p = mem_alloc(a, 64, 1);
    ASSERT_NOT_NULL(p);
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

TEST(stats_used_grows_with_allocations)
{
    allocator_t a = growing_arena_allocator(&arena);
    arena_stats_t s0 = growing_arena_stats(&arena);
    ASSERT_EQ(0u, s0.used);
    ASSERT_EQ(0u, s0.capacity); /* no block yet */

    mem_alloc(a, 64, 1);
    arena_stats_t s1 = growing_arena_stats(&arena);
    ASSERT_GE(s1.used, 64u);
    ASSERT_GE(s1.capacity, (size_t)BLOCK_SIZE);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "growing_arena");
}
