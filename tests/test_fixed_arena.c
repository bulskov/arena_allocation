#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

extern "C"
{
#include "arena/fixed_arena.h"
}

/* C99 compound literals are not valid C++; provide a C++ equivalent */
#ifdef ALLOCATOR_NULL
#undef ALLOCATOR_NULL
#endif
static const allocator_t ALLOCATOR_NULL = {};

#define BUF_SIZE 1024

static uint8_t buf[BUF_SIZE];
static fixed_arena_t arena;

class fixed_arena : public ::testing::Test{
    protected : void SetUp() override{fixed_arena_init(&arena, buf, BUF_SIZE);
}
}
;

/* ── basic allocation ───────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, alloc_returns_nonnull)
{
    void *p = mem_alloc(fixed_arena_allocator(&arena), 16, 1);
    ASSERT_NE(p, nullptr);
}

TEST_F(fixed_arena, alloc_writes_are_readable)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 8, 1);
    ASSERT_NE(p, nullptr);
    memset(p, 0xAB, 8);
    for (int i = 0; i < 8; ++i)
        EXPECT_EQ(p[i], (uint8_t)0xAB);
}

TEST_F(fixed_arena, alloc_advances_sequentially)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 16, 1);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ((uintptr_t)p2 - (uintptr_t)p1, 16u);
}

/* ── alignment ──────────────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, alignment_1)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, 1, 1);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((uintptr_t)p % 1, 0u);
}

TEST_F(fixed_arena, alignment_4)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_alloc(a, 1, 1); /* knock alignment off */
    void *p = mem_alloc(a, 4, 4);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((uintptr_t)p % 4, 0u);
}

TEST_F(fixed_arena, alignment_16)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_alloc(a, 3, 1);
    void *p = mem_alloc(a, 32, 16);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((uintptr_t)p % 16, 0u);
}

/* ── OOM ────────────────────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, alloc_oom_returns_null)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, BUF_SIZE + 1, 1);
    EXPECT_EQ(p, nullptr);
}

TEST_F(fixed_arena, alloc_fills_to_exact_capacity)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, BUF_SIZE, 1);
    ASSERT_NE(p, nullptr);
    void *q = mem_alloc(a, 1, 1);
    EXPECT_EQ(q, nullptr);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, reset_rewinds_offset)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 64, 1);
    fixed_arena_reset(&arena);
    void *p2 = mem_alloc(a, 64, 1);
    EXPECT_EQ(p1, p2);
}

TEST_F(fixed_arena, reset_allows_full_reuse)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_alloc(a, BUF_SIZE, 1);
    fixed_arena_reset(&arena);
    void *p = mem_alloc(a, BUF_SIZE, 1);
    ASSERT_NE(p, nullptr);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, realloc_inplace_last_alloc)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p, 0xCD, 16);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 16, 32, 1);
    /* must be in-place */
    EXPECT_EQ(p, p2);
    /* original content preserved */
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(p2[i], (uint8_t)0xCD);
}

TEST_F(fixed_arena, realloc_general_copies_content)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p1, 0x11, 16);
    mem_alloc(a, 8, 1); /* bump so p1 is no longer last */
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p1, 16, 16, 1);
    ASSERT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(p2[i], (uint8_t)0x11);
}

TEST_F(fixed_arena, realloc_oom_returns_null)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, BUF_SIZE / 2, 1);
    mem_alloc(a, 1, 1); /* consume most of the rest */
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, BUF_SIZE / 2, BUF_SIZE, 1);
    EXPECT_EQ(p2, nullptr);
}

/* ── free ───────────────────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, free_is_noop_no_crash)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, 32, 1);
    mem_free(a, p, 32); /* must not crash */
    void *q = mem_alloc(a, 32, 1);
    ASSERT_NE(q, nullptr); /* arena still usable */
}

/* ── scratch ────────────────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, scratch_rewinds_on_end)
{
    allocator_t a = fixed_arena_allocator(&arena);
    size_t before = arena.offset;
    scratch_t s;
    fixed_arena_scratch_begin(&s, &arena);
    mem_alloc(scratch_allocator(&s), 128, 1);
    scratch_end(&s);
    EXPECT_EQ(arena.offset, before);
}

TEST_F(fixed_arena, scratch_alloc_is_visible_before_end)
{
    scratch_t s;
    fixed_arena_scratch_begin(&s, &arena);
    allocator_t sa = scratch_allocator(&s);
    uint8_t *p = (uint8_t *)mem_alloc(sa, 32, 1);
    ASSERT_NE(p, nullptr);
    memset(p, 0x55, 32);
    EXPECT_EQ(p[0], (uint8_t)0x55);
    scratch_end(&s);
}

/* ── nullptr-ptr contract
 * ──────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_realloc(a, nullptr, 0, 32, 1);
    ASSERT_NE(p, nullptr);
}

TEST_F(fixed_arena, free_null_is_noop)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_free(a, nullptr, 0); /* must not crash */
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, stats_reports_used_and_capacity)
{
    allocator_t a = fixed_arena_allocator(&arena);
    arena_stats_t s0 = fixed_arena_stats(&arena);
    EXPECT_EQ(s0.used, 0u);
    EXPECT_EQ(s0.capacity, (size_t)BUF_SIZE);

    mem_alloc(a, 64, 1);
    arena_stats_t s1 = fixed_arena_stats(&arena);
    EXPECT_GE(s1.used, 64u);
    EXPECT_LE(s1.used, (size_t)BUF_SIZE);
    EXPECT_EQ(s1.capacity, (size_t)BUF_SIZE);
}

/* ── mem_calloc ─────────────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, mem_calloc_returns_zeroed_memory)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_calloc(a, 64, 1);
    ASSERT_NE(p, nullptr);
    for (int i = 0; i < 64; ++i)
        EXPECT_EQ(p[i], (uint8_t)0);
}

/* ── ALLOCATOR_NULL ─────────────────────────────────────────────────────────
 */

TEST_F(fixed_arena, allocator_null_alloc_returns_null)
{
    void *p = mem_alloc(ALLOCATOR_NULL, 32, 1);
    EXPECT_EQ(p, nullptr);
}

TEST_F(fixed_arena, allocator_null_realloc_returns_null)
{
    void *p = mem_realloc(ALLOCATOR_NULL, nullptr, 0, 32, 1);
    EXPECT_EQ(p, nullptr);
}

TEST_F(fixed_arena, allocator_null_free_is_noop)
{
    uint8_t tmp[4];
    mem_free(ALLOCATOR_NULL, tmp, 4); /* vt==nullptr — must not crash */
}
/* \u2500\u2500 owned (mmap-backed) arena
 * \u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500\u2500
 */

class fixed_arena_owned : public ::testing::Test
{
protected:
    fixed_arena_t arena;
    void SetUp() override
    {
        ASSERT_EQ(fixed_arena_create(&arena, BUF_SIZE), 0);
    }
    void TearDown() override
    {
        fixed_arena_destroy(
            &arena); /* idempotent \u2014 safe even if test called it */
    }
};

TEST_F(fixed_arena_owned, create_gives_usable_backing)
{
    EXPECT_NE(arena.base, nullptr);
    EXPECT_EQ(arena.size, (size_t)BUF_SIZE);
    EXPECT_EQ(arena.offset, (size_t)0);
    EXPECT_EQ(arena.owned, 1);
}

TEST_F(fixed_arena_owned, alloc_works)
{
    void *p = mem_alloc(fixed_arena_allocator(&arena), 64, 8);
    EXPECT_NE(p, nullptr);
}

TEST_F(fixed_arena_owned, destroy_clears_struct)
{
    fixed_arena_destroy(&arena);
    EXPECT_EQ(arena.base, nullptr);
    EXPECT_EQ(arena.owned, 0);
}

TEST(fixed_arena_allocator_new_test, returns_valid_allocator)
{
    fixed_arena_t a;
    allocator_t alloc = fixed_arena_allocator_new(&a, BUF_SIZE);
    ASSERT_NE(alloc.vt, nullptr);
    void *p = mem_alloc(alloc, 64, 8);
    EXPECT_NE(p, nullptr);
    fixed_arena_destroy(&a);
}