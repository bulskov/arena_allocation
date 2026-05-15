#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

extern "C" {
#include "arena/debug_allocator.h"
#include "arena/fixed_arena.h"
}

#define BUF_SIZE 1024

static uint8_t buf[BUF_SIZE];
static fixed_arena_t inner_arena;
static debug_allocator_t dbg;

class debug_allocator : public ::testing::Test {
protected:
    void SetUp() override
    {
        fixed_arena_init(&inner_arena, buf, BUF_SIZE);
        debug_allocator_init(&dbg, fixed_arena_allocator(&inner_arena));
    }
};

/* ── sentinel: alloc fills 0xCD ────────────────────────────────────────────
 */

TEST_F(debug_allocator, alloc_fills_with_0xcd)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_alloc(a, 32, 1);
    ASSERT_NE(p, nullptr);
    for (int i = 0; i < 32; ++i)
        EXPECT_EQ(p[i], (uint8_t)0xCD);
}

/* ── sentinel: free fills 0xDD ─────────────────────────────────────────────
 */

TEST_F(debug_allocator, free_fills_with_0xdd)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_alloc(a, 32, 1);
    memset(p, 0xAB, 32);
    mem_free(a, p, 32);
    for (int i = 0; i < 32; ++i)
        EXPECT_EQ(p[i], (uint8_t)0xDD);
}

/* ── sentinel: realloc in-place expansion fills new bytes with 0xCD ─────── */

TEST_F(debug_allocator, realloc_inplace_expansion_fills_0xcd)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p, 0x11, 16);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 16, 32, 1);
    EXPECT_EQ(p, p2); /* in-place: p is the last alloc in the fixed arena */
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(p2[i], (uint8_t)0x11); /* original content preserved */
    for (int i = 16; i < 32; ++i)
        EXPECT_EQ(p2[i], (uint8_t)0xCD); /* extension filled */
}

/* ── sentinel: realloc in-place shrink fills tail with 0xDD ────────────── */

TEST_F(debug_allocator, realloc_inplace_shrink_fills_0xdd)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_alloc(a, 32, 1);
    memset(p, 0x22, 32);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 32, 16, 1);
    EXPECT_EQ(p, p2);
    for (int i = 16; i < 32; ++i)
        EXPECT_EQ(p[i], (uint8_t)0xDD); /* discarded tail poisoned */
}

/* ── sentinel: relocation poisons old pointer ──────────────────────────────
 */

TEST_F(debug_allocator, realloc_relocation_poisons_old_ptr)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    mem_alloc(a, 8, 1); /* bump so p1 is no longer the last allocation */
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p1, 16, 16, 1);
    EXPECT_NE(p1, p2); /* must have relocated */
    for (int i = 0; i < 16; ++i)
        EXPECT_EQ(p1[i], (uint8_t)0xDD); /* old region poisoned */
}

/* ── nullptr-ptr contract ──────────────────────────────────────────────────────
 */

TEST_F(debug_allocator, realloc_null_acts_as_alloc)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_realloc(a, nullptr, 0, 32, 1);
    ASSERT_NE(p, nullptr);
    for (int i = 0; i < 32; ++i)
        EXPECT_EQ(p[i], (uint8_t)0xCD);
}

TEST_F(debug_allocator, free_null_is_noop)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    mem_free(a, nullptr, 0); /* must not crash */
}

/* ── stats: alloc_count and free_count ─────────────────────────────────────
 */

TEST_F(debug_allocator, stats_counts_allocs_and_frees)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    mem_alloc(a, 16, 1);
    void *p = mem_alloc(a, 32, 1);
    EXPECT_EQ(debug_allocator_stats(&dbg).alloc_count, 2u);
    EXPECT_EQ(debug_allocator_stats(&dbg).free_count, 0u);
    mem_free(a, p, 32);
    EXPECT_EQ(debug_allocator_stats(&dbg).free_count, 1u);
}

TEST_F(debug_allocator, realloc_null_increments_alloc_count)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    mem_realloc(a, nullptr, 0, 32, 1);
    EXPECT_EQ(debug_allocator_stats(&dbg).alloc_count, 1u);
}

TEST_F(debug_allocator, realloc_non_null_does_not_increment_alloc_count)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    void *p = mem_alloc(a, 16, 1);
    mem_realloc(a, p, 16, 32, 1);
    EXPECT_EQ(debug_allocator_stats(&dbg).alloc_count, 1u);
}

/* ── stats: bytes_live tracks live bytes ───────────────────────────────────
 */

TEST_F(debug_allocator, stats_bytes_live_tracks_allocations)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    void *p1 = mem_alloc(a, 32, 1);
    EXPECT_EQ(debug_allocator_stats(&dbg).bytes_live, 32u);
    mem_alloc(a, 16, 1);
    EXPECT_EQ(debug_allocator_stats(&dbg).bytes_live, 48u);
    mem_free(a, p1, 32);
    EXPECT_EQ(debug_allocator_stats(&dbg).bytes_live, 16u);
}

/* ── stats: bytes_peak is high-watermark ───────────────────────────────────
 */

TEST_F(debug_allocator, stats_bytes_peak_is_high_watermark)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    void *p1 = mem_alloc(a, 64, 1);
    void *p2 = mem_alloc(a, 32, 1);
    EXPECT_EQ(debug_allocator_stats(&dbg).bytes_peak, 96u);
    mem_free(a, p1, 64);
    mem_free(a, p2, 32);
    EXPECT_EQ(debug_allocator_stats(&dbg).bytes_live, 0u);
    EXPECT_EQ(debug_allocator_stats(&dbg).bytes_peak, 96u); /* not lowered */
}

/* ── stats: bytes_total is cumulative ──────────────────────────────────────
 */

TEST_F(debug_allocator, stats_bytes_total_is_cumulative)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    void *p = mem_alloc(a, 32, 1);
    mem_alloc(a, 16, 1);
    EXPECT_EQ(debug_allocator_stats(&dbg).bytes_total, 48u);
    mem_free(a, p, 32); /* free does not reduce bytes_total */
    EXPECT_EQ(debug_allocator_stats(&dbg).bytes_total, 48u);
}

/* ── reset zeroes all counters ──────────────────────────────────────────────
 */

TEST_F(debug_allocator, reset_zeroes_all_counters)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    mem_alloc(a, 64, 1);
    debug_allocator_reset(&dbg);
    debug_allocator_stats_t s = debug_allocator_stats(&dbg);
    EXPECT_EQ(s.alloc_count, 0u);
    EXPECT_EQ(s.free_count, 0u);
    EXPECT_EQ(s.bytes_total, 0u);
    EXPECT_EQ(s.bytes_live, 0u);
    EXPECT_EQ(s.bytes_peak, 0u);
}
