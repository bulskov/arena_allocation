#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

extern "C" {
#include "arena/virtual_arena.h"
#include "platform.h" /* mem_page_size */
}

/* Reserve 16 MB, commit in 64 KB chunks. */
#define RESERVED (16u * 1024u * 1024u)
#define COMMIT_CHUNK (64u * 1024u)

static virtual_arena_t arena;

class virtual_arena : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(virtual_arena_init(&arena, RESERVED, COMMIT_CHUNK), 0);
    }
    void TearDown() override { virtual_arena_destroy(&arena); }
};

/* ── basic allocation ───────────────────────────────────────────────────────
 */

TEST_F(virtual_arena, alloc_returns_nonnull)
{
    void *p = mem_alloc(virtual_arena_allocator(&arena), 64, 1);
    ASSERT_NE(p, nullptr);
}

TEST_F(virtual_arena, alloc_writes_are_readable)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    ASSERT_NE(p, nullptr);
    memset(p, 0xF0, 16);
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(p[i], (uint8_t)0xF0);
}

TEST_F(virtual_arena, alloc_advances_sequentially)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 32, 1);
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 32, 1);
    ASSERT_NE(p1, nullptr);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ((uintptr_t)p2 - (uintptr_t)p1, 32u);
}

/* ── alignment ──────────────────────────────────────────────────────────────
 */

TEST_F(virtual_arena, alignment_8)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, 3, 1);
    void *p = mem_alloc(a, 8, 8);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((uintptr_t)p % 8, 0u);
}

TEST_F(virtual_arena, alignment_64)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, 7, 1);
    void *p = mem_alloc(a, 64, 64);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ((uintptr_t)p % 64, 0u);
}

/* ── commit on demand ───────────────────────────────────────────────────────
 */

TEST_F(virtual_arena, nothing_committed_before_first_alloc)
{
    EXPECT_EQ(arena.committed, 0u);
}

TEST_F(virtual_arena, commits_on_first_alloc)
{
    mem_alloc(virtual_arena_allocator(&arena), 1, 1);
    EXPECT_GT(arena.committed, 0u);
}

TEST_F(virtual_arena, committed_grows_in_chunks)
{
    allocator_t a = virtual_arena_allocator(&arena);
    /* First alloc triggers the first commit chunk. */
    mem_alloc(a, 1, 1);
    size_t committed_after_first = arena.committed;
    EXPECT_EQ(committed_after_first, COMMIT_CHUNK);

    /* Stay within the first chunk — committed must not change. */
    mem_alloc(a, COMMIT_CHUNK / 2, 1);
    EXPECT_EQ(arena.committed, committed_after_first);

    /* Cross into the next chunk. */
    mem_alloc(a, COMMIT_CHUNK, 1);
    EXPECT_GT(arena.committed, committed_after_first);
}

/* ── OOM ────────────────────────────────────────────────────────────────────
 */

TEST_F(virtual_arena, alloc_beyond_reserved_returns_null)
{
    virtual_arena_t small;
    ASSERT_EQ(virtual_arena_init(&small, mem_page_size(), mem_page_size()), 0);
    allocator_t a = virtual_arena_allocator(&small);
    /* Exhaust the reserved range. */
    mem_alloc(a, mem_page_size(), 1);
    void *p = mem_alloc(a, 1, 1);
    EXPECT_EQ(p, nullptr);
    virtual_arena_destroy(&small);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

TEST_F(virtual_arena, reset_rewinds_offset)
{
    allocator_t a = virtual_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 64, 1);
    virtual_arena_reset(&arena);
    void *p2 = mem_alloc(a, 64, 1);
    EXPECT_EQ(p1, p2);
}

TEST_F(virtual_arena, reset_decommits_pages)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, COMMIT_CHUNK * 2, 1);
    EXPECT_GT(arena.committed, 0u);
    virtual_arena_reset(&arena);
    EXPECT_EQ(arena.committed, 0u);
    EXPECT_EQ(arena.offset, 0u);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

TEST_F(virtual_arena, realloc_inplace_last_alloc)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p, 0x77, 16);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 16, 32, 1);
    EXPECT_EQ(p, p2);
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(p2[i], (uint8_t)0x77);
}

TEST_F(virtual_arena, realloc_general_preserves_content)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p1, 0x33, 16);
    mem_alloc(a, 8, 1);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p1, 16, 16, 1);
    ASSERT_NE(p2, nullptr);
    EXPECT_NE(p1, p2);
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(p2[i], (uint8_t)0x33);
}

/* ── scratch ────────────────────────────────────────────────────────────────
 */

TEST_F(virtual_arena, scratch_rewinds_offset)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, 64, 1);
    size_t offset_before = arena.offset;

    scratch_t s;
    virtual_arena_scratch_begin(&s, &arena);
    mem_alloc(scratch_allocator(&s), COMMIT_CHUNK * 2, 1);
    scratch_end(&s);

    EXPECT_EQ(arena.offset, offset_before);
}

TEST_F(virtual_arena, scratch_decommits_on_end)
{
    scratch_t s;
    virtual_arena_scratch_begin(&s, &arena);
    /* Force at least two commit chunks. */
    mem_alloc(scratch_allocator(&s), COMMIT_CHUNK * 3, 1);
    size_t committed_during = arena.committed;
    EXPECT_GT(committed_during, 0u);

    scratch_end(&s);

    EXPECT_LT(arena.committed, committed_during);
}

/* ── nullptr-ptr contract ──────────────────────────────────────────────────────
 */

TEST_F(virtual_arena, realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = virtual_arena_allocator(&arena);
    void *p = mem_realloc(a, nullptr, 0, 64, 1);
    ASSERT_NE(p, nullptr);
}

TEST_F(virtual_arena, free_null_is_noop)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_free(a, nullptr, 0); /* must not crash */
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

TEST_F(virtual_arena, stats_reports_reserved_and_used)
{
    allocator_t a = virtual_arena_allocator(&arena);
    arena_stats_t s0 = virtual_arena_stats(&arena);
    EXPECT_EQ(s0.used, 0u);
    EXPECT_EQ(s0.committed, 0u); /* nothing committed yet */
    EXPECT_EQ(s0.reserved, (size_t)RESERVED);

    mem_alloc(a, 64, 1);
    arena_stats_t s1 = virtual_arena_stats(&arena);
    EXPECT_GE(s1.used, 64u);
    EXPECT_GE(s1.committed, (size_t)COMMIT_CHUNK);
    EXPECT_EQ(s1.reserved, (size_t)RESERVED);
}
