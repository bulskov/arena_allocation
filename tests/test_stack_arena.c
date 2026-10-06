#include "ctt.h"

#include <stdint.h>
#include <string.h>

#include "arena/stack_arena.h"

#define CAPACITY 4096
#define MIN_ALIGN 8

static stack_arena_t arena;

void ctt_before_each(void)
{
    ASSERT_EQ(stack_arena_init(&arena, CAPACITY, MIN_ALIGN), 0);
}

void ctt_after_each(void)
{
    stack_arena_destroy(&arena);
}

/* ── basic allocation ───────────────────────────────────────────────────────
 */

TEST(alloc_returns_nonnull)
{
    void *p = mem_alloc(stack_arena_allocator(&arena), 16, 1);
    ASSERT_NOT_NULL(p);
}

TEST(alloc_writes_are_readable)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    ASSERT_NOT_NULL(p);
    memset(p, 0xAB, 16);
    for (int i = 0; i < 16; ++i)
        ASSERT_EQ((uint8_t)0xAB, p[i]);
}

TEST(alloc_oom_returns_null)
{
    void *p = mem_alloc(stack_arena_allocator(&arena), CAPACITY + 1, 1);
    ASSERT_NULL(p);
}

/* ── slot rounding ──────────────────────────────────────────────────────────
 */

TEST(slots_are_multiples_of_min_align)
{
    allocator_t a = stack_arena_allocator(&arena);
    mem_alloc(a, 1, 1);
    /* With min_align=8, each slot is 8 bytes; offset must be a multiple. */
    ASSERT_EQ(0u, arena.offset % MIN_ALIGN);
    mem_alloc(a, 3, 1);
    ASSERT_EQ(0u, arena.offset % MIN_ALIGN);
    mem_alloc(a, 7, 1);
    ASSERT_EQ(0u, arena.offset % MIN_ALIGN);
}

TEST(consecutive_slots_are_contiguous)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 1, 1); /* slot = 8 */
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 1, 1); /* slot = 8 */
    ASSERT_EQ((uintptr_t)MIN_ALIGN, (uintptr_t)p2 - (uintptr_t)p1);
}

/* ── LIFO free: single pop ──────────────────────────────────────────────────
 */

TEST(free_top_rewinds_offset)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p = mem_alloc(a, 16, 1);
    size_t after = arena.offset;
    (void)after;
    mem_free(a, p, 16);
    ASSERT_EQ(0u, arena.offset);
}

TEST(freed_slot_is_reused)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 16, 1);
    mem_free(a, p1, 16);
    void *p2 = mem_alloc(a, 16, 1);
    ASSERT_PTR_EQ(p2, p1);
}

/* ── LIFO free: multiple pops ───────────────────────────────────────────────
 */

TEST(multi_level_lifo_free)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 3, 1); /* slot=8, offset=8  */
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 5, 1); /* slot=8, offset=16 */
    uint8_t *p3 = (uint8_t *)mem_alloc(a, 7, 1); /* slot=8, offset=24 */

    memset(p1, 0x11, 3);
    memset(p2, 0x22, 5);
    memset(p3, 0x33, 7);

    mem_free(a, p3, 7);
    ASSERT_EQ(16u, arena.offset);

    mem_free(a, p2, 5);
    ASSERT_EQ(8u, arena.offset);

    mem_free(a, p1, 3);
    ASSERT_EQ(0u, arena.offset);
}

TEST(freed_slots_restore_content)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 8, 1);
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 8, 1);
    memset(p1, 0xAA, 8);
    memset(p2, 0xBB, 8);

    mem_free(a, p2, 8);
    /* p1 must still be intact after popping p2 */
    for (int i = 0; i < 8; ++i)
        ASSERT_EQ((uint8_t)0xAA, p1[i]);
}

/* ── LIFO violation: non-top free is a no-op ─────────────────────────────── */

TEST(free_non_top_is_noop)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 8, 1);
    void *p2 = mem_alloc(a, 8, 1);
    size_t before = arena.offset;

    mem_free(a, p1, 8); /* p1 is NOT the top — should be a no-op */
    ASSERT_EQ(before, arena.offset);

    (void)p2;
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

TEST(reset_rewinds_to_zero)
{
    allocator_t a = stack_arena_allocator(&arena);
    for (int i = 0; i < 10; ++i)
        mem_alloc(a, 8, 1);
    stack_arena_reset(&arena);
    ASSERT_EQ(0u, arena.offset);
}

TEST(reset_allows_full_reuse)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 32, 1);
    stack_arena_reset(&arena);
    void *p2 = mem_alloc(a, 32, 1);
    ASSERT_PTR_EQ(p2, p1);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

TEST(realloc_inplace_top)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 8, 1);
    memset(p, 0xCC, 8);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 8, 16, 1);
    ASSERT_PTR_EQ(p2, p);
    for (int i = 0; i < 8; ++i)
        ASSERT_EQ((uint8_t)0xCC, p2[i]);
}

TEST(realloc_general_preserves_content)
{
    allocator_t a = stack_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 8, 1);
    memset(p1, 0xDD, 8);
    mem_alloc(a, 8, 1); /* p1 no longer the top */
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p1, 8, 8, 1);
    ASSERT_NOT_NULL(p2);
    ASSERT_PTR_NE(p2, p1);
    for (int i = 0; i < 8; ++i)
        ASSERT_EQ((uint8_t)0xDD, p2[i]);
}

/* ── capacity ───────────────────────────────────────────────────────────────
 */

TEST(fills_to_capacity)
{
    allocator_t a = stack_arena_allocator(&arena);
    /* alloc in MIN_ALIGN-sized steps until full */
    size_t steps = arena.capacity / MIN_ALIGN;
    for (size_t i = 0; i < steps; ++i)
    {
        void *p = mem_alloc(a, 1, 1);
        ASSERT_NOT_NULL(p);
    }
    void *p = mem_alloc(a, 1, 1);
    ASSERT_NULL(p);
}

/* ── NULL-ptr contract ──────────────────────────────────────────────────────
 */

TEST(realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = stack_arena_allocator(&arena);
    void *p = mem_realloc(a, NULL, 0, 16, 1);
    ASSERT_NOT_NULL(p);
}

TEST(free_null_is_noop)
{
    allocator_t a = stack_arena_allocator(&arena);
    mem_free(a, NULL, 0); /* stack_free reads ptr+size — guard is critical */
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

TEST(stats_reports_used_and_capacity)
{
    allocator_t a = stack_arena_allocator(&arena);
    arena_stats_t s0 = stack_arena_stats(&arena);
    ASSERT_EQ(0u, s0.used);
    ASSERT_EQ((size_t)CAPACITY, s0.capacity);

    mem_alloc(a, 16, 1);
    arena_stats_t s1 = stack_arena_stats(&arena);
    ASSERT_GE(s1.used, 16u);
    ASSERT_EQ((size_t)CAPACITY, s1.capacity);

    stack_arena_reset(&arena);
    arena_stats_t s2 = stack_arena_stats(&arena);
    ASSERT_EQ(0u, s2.used);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "stack_arena");
}
