#include "ctt.h"

#include <stdint.h>
#include <string.h>

#include "arena/debug_allocator.h"
#include "arena/fixed_arena.h"

#define BUF_SIZE 1024

static uint8_t buf[BUF_SIZE];
static fixed_arena_t inner_arena;
static debug_allocator_t dbg;

void ctt_before_each(void)
{
    fixed_arena_init(&inner_arena, buf, BUF_SIZE);
    debug_allocator_init(&dbg, fixed_arena_allocator(&inner_arena));
}

/* ── sentinel: alloc fills 0xCD ────────────────────────────────────────────
 */

TEST(alloc_fills_with_0xcd)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_alloc(a, 32, 1);
    ASSERT_NOT_NULL(p);
    for (int i = 0; i < 32; ++i)
        ASSERT_EQ((uint8_t)0xCD, p[i]);
}

/* ── sentinel: free fills 0xDD ─────────────────────────────────────────────
 */

TEST(free_fills_with_0xdd)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_alloc(a, 32, 1);
    memset(p, 0xAB, 32);
    mem_free(a, p, 32);
    for (int i = 0; i < 32; ++i)
        ASSERT_EQ((uint8_t)0xDD, p[i]);
}

/* ── sentinel: realloc in-place expansion fills new bytes with 0xCD ─────── */

TEST(realloc_inplace_expansion_fills_0xcd)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p, 0x11, 16);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 16, 32, 1);
    ASSERT_PTR_EQ(p2, p); /* in-place: p is the last alloc in the fixed arena */
    for (int i = 0; i < 16; ++i)
        ASSERT_EQ((uint8_t)0x11, p2[i]); /* original content preserved */
    for (int i = 16; i < 32; ++i)
        ASSERT_EQ((uint8_t)0xCD, p2[i]); /* extension filled */
}

/* ── sentinel: realloc in-place shrink fills tail with 0xDD ────────────── */

TEST(realloc_inplace_shrink_fills_0xdd)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_alloc(a, 32, 1);
    memset(p, 0x22, 32);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 32, 16, 1);
    ASSERT_PTR_EQ(p2, p);
    for (int i = 16; i < 32; ++i)
        ASSERT_EQ((uint8_t)0xDD, p[i]); /* discarded tail poisoned */
}

/* ── sentinel: relocation poisons old pointer ──────────────────────────────
 */

TEST(realloc_relocation_poisons_old_ptr)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    mem_alloc(a, 8, 1); /* bump so p1 is no longer the last allocation */
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p1, 16, 16, 1);
    ASSERT_PTR_NE(p2, p1); /* must have relocated */
    for (int i = 0; i < 16; ++i)
        ASSERT_EQ((uint8_t)0xDD, p1[i]); /* old region poisoned */
}

/* ── NULL-ptr contract ──────────────────────────────────────────────────────
 */

TEST(realloc_null_acts_as_alloc)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    uint8_t *p = (uint8_t *)mem_realloc(a, NULL, 0, 32, 1);
    ASSERT_NOT_NULL(p);
    for (int i = 0; i < 32; ++i)
        ASSERT_EQ((uint8_t)0xCD, p[i]);
}

TEST(free_null_is_noop)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    mem_free(a, NULL, 0); /* must not crash */
}

/* ── stats: alloc_count and free_count ─────────────────────────────────────
 */

TEST(stats_counts_allocs_and_frees)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    mem_alloc(a, 16, 1);
    void *p = mem_alloc(a, 32, 1);
    ASSERT_EQ(2u, debug_allocator_stats(&dbg).alloc_count);
    ASSERT_EQ(0u, debug_allocator_stats(&dbg).free_count);
    mem_free(a, p, 32);
    ASSERT_EQ(1u, debug_allocator_stats(&dbg).free_count);
}

TEST(realloc_null_increments_alloc_count)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    mem_realloc(a, NULL, 0, 32, 1);
    ASSERT_EQ(1u, debug_allocator_stats(&dbg).alloc_count);
}

TEST(realloc_non_null_does_not_increment_alloc_count)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    void *p = mem_alloc(a, 16, 1);
    mem_realloc(a, p, 16, 32, 1);
    ASSERT_EQ(1u, debug_allocator_stats(&dbg).alloc_count);
}

/* ── stats: bytes_live tracks live bytes ───────────────────────────────────
 */

TEST(stats_bytes_live_tracks_allocations)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    void *p1 = mem_alloc(a, 32, 1);
    ASSERT_EQ(32u, debug_allocator_stats(&dbg).bytes_live);
    mem_alloc(a, 16, 1);
    ASSERT_EQ(48u, debug_allocator_stats(&dbg).bytes_live);
    mem_free(a, p1, 32);
    ASSERT_EQ(16u, debug_allocator_stats(&dbg).bytes_live);
}

/* ── stats: bytes_peak is high-watermark ───────────────────────────────────
 */

TEST(stats_bytes_peak_is_high_watermark)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    void *p1 = mem_alloc(a, 64, 1);
    void *p2 = mem_alloc(a, 32, 1);
    ASSERT_EQ(96u, debug_allocator_stats(&dbg).bytes_peak);
    mem_free(a, p1, 64);
    mem_free(a, p2, 32);
    ASSERT_EQ(0u, debug_allocator_stats(&dbg).bytes_live);
    ASSERT_EQ(96u, debug_allocator_stats(&dbg).bytes_peak); /* not lowered */
}

/* ── stats: bytes_total is cumulative ──────────────────────────────────────
 */

TEST(stats_bytes_total_is_cumulative)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    void *p = mem_alloc(a, 32, 1);
    mem_alloc(a, 16, 1);
    ASSERT_EQ(48u, debug_allocator_stats(&dbg).bytes_total);
    mem_free(a, p, 32); /* free does not reduce bytes_total */
    ASSERT_EQ(48u, debug_allocator_stats(&dbg).bytes_total);
}

/* ── reset zeroes all counters ──────────────────────────────────────────────
 */

TEST(reset_zeroes_all_counters)
{
    allocator_t a = debug_allocator_allocator(&dbg);
    mem_alloc(a, 64, 1);
    debug_allocator_reset(&dbg);
    debug_allocator_stats_t s = debug_allocator_stats(&dbg);
    ASSERT_EQ(0u, s.alloc_count);
    ASSERT_EQ(0u, s.free_count);
    ASSERT_EQ(0u, s.bytes_total);
    ASSERT_EQ(0u, s.bytes_live);
    ASSERT_EQ(0u, s.bytes_peak);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "debug_allocator");
}
