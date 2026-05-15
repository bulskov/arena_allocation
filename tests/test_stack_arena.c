#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

extern "C" {
#include "arena/stack_arena.h"
}

#define CAPACITY 4096
#define MIN_ALIGN 8

static stack_arena_t arena;

class stack_arena : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(stack_arena_init(&arena, CAPACITY, MIN_ALIGN), 0);
    }
    void TearDown() override { stack_arena_destroy(&arena); }
};

/* ── basic allocation ───────────────────────────────────────────────────────
 */

TEST_F(stack_arena, alloc_returns_nonnull)
{
    void *p = mem_alloc(stack_arena_allocator(&arena), 16, 1);
    ASSERT_NE(p, nullptr);
}

TEST_F(stack_arena, alloc_writes_are_readable)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    ASSERT_NE(p, nullptr);
    memset(p, 0xAB, 16);
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(p[i], (uint8_t)0xAB);
}

TEST_F(stack_arena, alloc_oom_returns_null)
{
    void *p = mem_alloc(stack_arena_allocator(&arena), CAPACITY + 1, 1);
    EXPECT_EQ(p, nullptr);
}

/* ── slot rounding ──────────────────────────────────────────────────────────
 */

TEST_F(stack_arena, slots_are_multiples_of_min_align)
{
    allocator_t a = stack_arena_allocator(&arena);
    mem_alloc(a, 1, 1);
    /* With min_align=8, each slot is 8 bytes; offset must be a multiple. */
    EXPECT_EQ(arena.offset % MIN_ALIGN, 0u);
    mem_alloc(a, 3, 1);
    EXPECT_EQ(arena.offset % MIN_ALIGN, 0u);
    mem_alloc(a, 7, 1);
    EXPECT_EQ(arena.offset % MIN_ALIGN, 0u);
}

TEST_F(stack_arena, consecutive_slots_are_contiguous)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 1, 1); /* slot = 8 */
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 1, 1); /* slot = 8 */
    EXPECT_EQ((uintptr_t)p2 - (uintptr_t)p1, (uintptr_t)MIN_ALIGN);
}

/* ── LIFO free: single pop ──────────────────────────────────────────────────
 */

TEST_F(stack_arena, free_top_rewinds_offset)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p = mem_alloc(a, 16, 1);
    size_t after = arena.offset;
    (void)after;
    mem_free(a, p, 16);
    EXPECT_EQ(arena.offset, 0u);
}

TEST_F(stack_arena, freed_slot_is_reused)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 16, 1);
    mem_free(a, p1, 16);
    void *p2 = mem_alloc(a, 16, 1);
    EXPECT_EQ(p1, p2);
}

/* ── LIFO free: multiple pops ───────────────────────────────────────────────
 */

TEST_F(stack_arena, multi_level_lifo_free)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 3, 1); /* slot=8, offset=8  */
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 5, 1); /* slot=8, offset=16 */
    uint8_t *p3 = (uint8_t *)mem_alloc(a, 7, 1); /* slot=8, offset=24 */

    memset(p1, 0x11, 3);
    memset(p2, 0x22, 5);
    memset(p3, 0x33, 7);

    mem_free(a, p3, 7);
    EXPECT_EQ(arena.offset, 16u);

    mem_free(a, p2, 5);
    EXPECT_EQ(arena.offset, 8u);

    mem_free(a, p1, 3);
    EXPECT_EQ(arena.offset, 0u);
}

TEST_F(stack_arena, freed_slots_restore_content)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 8, 1);
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 8, 1);
    memset(p1, 0xAA, 8);
    memset(p2, 0xBB, 8);

    mem_free(a, p2, 8);
    /* p1 must still be intact after popping p2 */
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(p1[i], (uint8_t)0xAA);
}

/* ── LIFO violation: non-top free is a no-op ─────────────────────────────── */

TEST_F(stack_arena, free_non_top_is_noop)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 8, 1);
    void *p2 = mem_alloc(a, 8, 1);
    size_t before = arena.offset;

    mem_free(a, p1, 8); /* p1 is NOT the top — should be a no-op */
    EXPECT_EQ(arena.offset, before);

    (void)p2;
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

TEST_F(stack_arena, reset_rewinds_to_zero)
{
    allocator_t a = stack_arena_allocator(&arena);
    for (int i = 0; i < 10; ++i)
        mem_alloc(a, 8, 1);
    stack_arena_reset(&arena);
    EXPECT_EQ(arena.offset, 0u);
}

TEST_F(stack_arena, reset_allows_full_reuse)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 32, 1);
    stack_arena_reset(&arena);
    void *p2 = mem_alloc(a, 32, 1);
    EXPECT_EQ(p1, p2);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

TEST_F(stack_arena, realloc_inplace_top)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 8, 1);
    memset(p, 0xCC, 8);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 8, 16, 1);
    EXPECT_EQ(p, p2);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(p2[i], (uint8_t)0xCC);
}

TEST_F(stack_arena, realloc_general_preserves_content)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 8, 1);
    memset(p1, 0xDD, 8);
    mem_alloc(a, 8, 1); /* p1 no longer the top */
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p1, 8, 8, 1);
    ASSERT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(p2[i], (uint8_t)0xDD);
}

/* ── capacity ───────────────────────────────────────────────────────────────
 */

TEST_F(stack_arena, fills_to_capacity)
{
    allocator_t a = stack_arena_allocator(&arena);
    /* alloc in MIN_ALIGN-sized steps until full */
    size_t steps = arena.capacity / MIN_ALIGN;
    for (size_t i = 0; i < steps; ++i)
    {
        void *p = mem_alloc(a, 1, 1);
        ASSERT_NE(p, nullptr);
    }
    void *p = mem_alloc(a, 1, 1);
    EXPECT_EQ(p, nullptr);
}

/* ── nullptr-ptr contract ──────────────────────────────────────────────────────
 */

TEST_F(stack_arena, realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p = mem_realloc(a, nullptr, 0, 16, 1);
    ASSERT_NE(p, nullptr);
}

TEST_F(stack_arena, free_null_is_noop)
{
    allocator_t a = stack_arena_allocator(&arena);
    mem_free(a, nullptr, 0); /* stack_free reads ptr+size — guard is critical */
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

TEST_F(stack_arena, stats_reports_used_and_capacity)
{
    allocator_t a = stack_arena_allocator(&arena);
    arena_stats_t s0 = stack_arena_stats(&arena);
    EXPECT_EQ(s0.used, 0u);
    EXPECT_EQ(s0.capacity, (size_t)CAPACITY);

    mem_alloc(a, 16, 1);
    arena_stats_t s1 = stack_arena_stats(&arena);
    EXPECT_GE(s1.used, 16u);
    EXPECT_EQ(s1.capacity, (size_t)CAPACITY);

    stack_arena_reset(&arena);
    arena_stats_t s2 = stack_arena_stats(&arena);
    EXPECT_EQ(s2.used, 0u);
}
