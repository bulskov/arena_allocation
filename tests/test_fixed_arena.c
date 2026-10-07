#include "ctt.h"

#include <stdint.h>
#include <string.h>

#include "arena/fixed_arena.h"

#define BUF_SIZE 1024

static uint8_t buf[BUF_SIZE];
static fixed_arena_t arena;
static fixed_arena_t owned; /* for tests that create an mmap-backed arena */

void ctt_before_each(void)
{
    fixed_arena_init(&arena, buf, BUF_SIZE);
}

void ctt_after_each(void)
{
    fixed_arena_destroy(
        &owned); /* idempotent — no-op if the test never created it */
}

/* ── basic allocation ───────────────────────────────────────────────────────
 */

TEST(alloc_returns_nonnull)
{
    void *p = mem_alloc(fixed_arena_allocator(&arena), 16, 1);
    ASSERT_NOT_NULL(p);
}

TEST(alloc_writes_are_readable)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 8, 1);
    ASSERT_NOT_NULL(p);
    memset(p, 0xAB, 8);
    for (int i = 0; i < 8; ++i)
    {
        ASSERT_EQ((uint8_t)0xAB, p[i]);
    }
}

TEST(alloc_advances_sequentially)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 16, 1);
    ASSERT_NOT_NULL(p1);
    ASSERT_NOT_NULL(p2);
    ASSERT_EQ(16u, (uintptr_t)p2 - (uintptr_t)p1);
}

/* ── alignment ──────────────────────────────────────────────────────────────
 */

TEST(alignment_1)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, 1, 1);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(0u, (uintptr_t)p % 1);
}

TEST(alignment_4)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_alloc(a, 1, 1); /* knock alignment off */
    void *p = mem_alloc(a, 4, 4);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(0u, (uintptr_t)p % 4);
}

TEST(alignment_16)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_alloc(a, 3, 1);
    void *p = mem_alloc(a, 32, 16);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(0u, (uintptr_t)p % 16);
}

/* ── OOM ────────────────────────────────────────────────────────────────────
 */

TEST(alloc_oom_returns_null)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, BUF_SIZE + 1, 1);
    ASSERT_NULL(p);
}

TEST(alloc_fills_to_exact_capacity)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, BUF_SIZE, 1);
    ASSERT_NOT_NULL(p);
    void *q = mem_alloc(a, 1, 1);
    ASSERT_NULL(q);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

TEST(reset_rewinds_offset)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 64, 1);
    fixed_arena_reset(&arena);
    void *p2 = mem_alloc(a, 64, 1);
    ASSERT_PTR_EQ(p2, p1);
}

TEST(reset_allows_full_reuse)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_alloc(a, BUF_SIZE, 1);
    fixed_arena_reset(&arena);
    void *p = mem_alloc(a, BUF_SIZE, 1);
    ASSERT_NOT_NULL(p);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

TEST(realloc_inplace_last_alloc)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p, 0xCD, 16);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 16, 32, 1);
    /* must be in-place */
    ASSERT_PTR_EQ(p2, p);
    /* original content preserved */
    for (int i = 0; i < 16; ++i)
    {
        ASSERT_EQ((uint8_t)0xCD, p2[i]);
    }
}

TEST(realloc_general_copies_content)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p1, 0x11, 16);
    mem_alloc(a, 8, 1); /* bump so p1 is no longer last */
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p1, 16, 16, 1);
    ASSERT_NOT_NULL(p2);
    ASSERT_PTR_NE(p2, p1);
    for (int i = 0; i < 16; ++i)
    {
        ASSERT_EQ((uint8_t)0x11, p2[i]);
    }
}

TEST(realloc_oom_returns_null)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, BUF_SIZE / 2, 1);
    mem_alloc(a, 1, 1); /* consume most of the rest */
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, BUF_SIZE / 2, BUF_SIZE, 1);
    ASSERT_NULL(p2);
}

/* ── free ───────────────────────────────────────────────────────────────────
 */

TEST(free_is_noop_no_crash)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, 32, 1);
    mem_free(a, p, 32); /* must not crash */
    void *q = mem_alloc(a, 32, 1);
    ASSERT_NOT_NULL(q); /* arena still usable */
}

/* ── scratch ────────────────────────────────────────────────────────────────
 */

TEST(scratch_rewinds_on_end)
{
    size_t before = arena.offset;
    scratch_t s;
    fixed_arena_scratch_begin(&s, &arena);
    mem_alloc(scratch_allocator(&s), 128, 1);
    scratch_end(&s);
    ASSERT_EQ(before, arena.offset);
}

TEST(scratch_alloc_is_visible_before_end)
{
    scratch_t s;
    fixed_arena_scratch_begin(&s, &arena);
    allocator_t sa = scratch_allocator(&s);
    uint8_t *p = (uint8_t *)mem_alloc(sa, 32, 1);
    ASSERT_NOT_NULL(p);
    memset(p, 0x55, 32);
    ASSERT_EQ((uint8_t)0x55, p[0]);
    scratch_end(&s);
}

/* ── NULL-ptr contract
 * ──────────────────────────────────────────────────────
 */

TEST(realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_realloc(a, NULL, 0, 32, 1);
    ASSERT_NOT_NULL(p);
}

TEST(free_null_is_noop)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_free(a, NULL, 0); /* must not crash */
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

TEST(stats_reports_used_and_capacity)
{
    allocator_t a = fixed_arena_allocator(&arena);
    arena_stats_t s0 = fixed_arena_stats(&arena);
    ASSERT_EQ(0u, s0.used);
    ASSERT_EQ((size_t)BUF_SIZE, s0.capacity);

    mem_alloc(a, 64, 1);
    arena_stats_t s1 = fixed_arena_stats(&arena);
    ASSERT_GE(s1.used, 64u);
    ASSERT_LE(s1.used, (size_t)BUF_SIZE);
    ASSERT_EQ((size_t)BUF_SIZE, s1.capacity);
}

/* ── mem_calloc ─────────────────────────────────────────────────────────────
 */

TEST(mem_calloc_returns_zeroed_memory)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_calloc(a, 64, 1);
    ASSERT_NOT_NULL(p);
    for (int i = 0; i < 64; ++i)
    {
        ASSERT_EQ((uint8_t)0, p[i]);
    }
}

/* ── ALLOCATOR_NULL ─────────────────────────────────────────────────────────
 */

TEST(allocator_null_alloc_returns_null)
{
    void *p = mem_alloc(ALLOCATOR_NULL, 32, 1);
    ASSERT_NULL(p);
}

TEST(allocator_null_realloc_returns_null)
{
    void *p = mem_realloc(ALLOCATOR_NULL, NULL, 0, 32, 1);
    ASSERT_NULL(p);
}

TEST(allocator_null_free_is_noop)
{
    uint8_t tmp[4];
    mem_free(ALLOCATOR_NULL, tmp, 4); /* vt==NULL — must not crash */
}

/* ── owned (mmap-backed) arena ──────────────────────────────────────────────
 */

TEST(owned_create_gives_usable_backing)
{
    ASSERT_EQ(0, fixed_arena_create(&owned, BUF_SIZE));
    ASSERT_NOT_NULL(owned.base);
    ASSERT_EQ((size_t)BUF_SIZE, owned.size);
    ASSERT_EQ((size_t)0, owned.offset);
    ASSERT_EQ(1, owned.owned);
}

TEST(owned_alloc_works)
{
    ASSERT_EQ(0, fixed_arena_create(&owned, BUF_SIZE));
    void *p = mem_alloc(fixed_arena_allocator(&owned), 64, 8);
    ASSERT_NOT_NULL(p);
}

TEST(owned_destroy_clears_struct)
{
    ASSERT_EQ(0, fixed_arena_create(&owned, BUF_SIZE));
    fixed_arena_destroy(&owned);
    ASSERT_NULL(owned.base);
    ASSERT_EQ(0, owned.owned);
}

TEST(allocator_new_returns_valid_allocator)
{
    allocator_t alloc = fixed_arena_allocator_new(&owned, BUF_SIZE);
    ASSERT_NOT_NULL(alloc.vt);
    void *p = mem_alloc(alloc, 64, 8);
    ASSERT_NOT_NULL(p);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "fixed_arena");
}
