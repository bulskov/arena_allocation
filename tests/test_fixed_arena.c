#include <criterion/criterion.h>
#include <stdint.h>
#include <string.h>

#include "arena/fixed_arena.h"

#define BUF_SIZE 1024

static uint8_t buf[BUF_SIZE];
static fixed_arena_t arena;

static void setup(void)
{
    fixed_arena_init(&arena, buf, BUF_SIZE);
}
static void teardown(void)
{ /* nothing — externally owned buffer */
}

TestSuite(fixed_arena, .init = setup, .fini = teardown);

/* ── basic allocation ───────────────────────────────────────────────────────
 */

Test(fixed_arena, alloc_returns_nonnull)
{
    void *p = mem_alloc(fixed_arena_allocator(&arena), 16, 1);
    cr_assert_not_null(p);
}

Test(fixed_arena, alloc_writes_are_readable)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = mem_alloc(a, 8, 1);
    cr_assert_not_null(p);
    memset(p, 0xAB, 8);
    for (int i = 0; i < 8; ++i)
        cr_assert_eq(p[i], (uint8_t)0xAB);
}

Test(fixed_arena, alloc_advances_sequentially)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p1 = mem_alloc(a, 16, 1);
    uint8_t *p2 = mem_alloc(a, 16, 1);
    cr_assert_not_null(p1);
    cr_assert_not_null(p2);
    cr_assert_eq((uintptr_t)p2 - (uintptr_t)p1, 16u);
}

/* ── alignment ──────────────────────────────────────────────────────────────
 */

Test(fixed_arena, alignment_1)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, 1, 1);
    cr_assert_not_null(p);
    cr_assert_eq((uintptr_t)p % 1, 0u);
}

Test(fixed_arena, alignment_4)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_alloc(a, 1, 1); /* knock alignment off */
    void *p = mem_alloc(a, 4, 4);
    cr_assert_not_null(p);
    cr_assert_eq((uintptr_t)p % 4, 0u);
}

Test(fixed_arena, alignment_16)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_alloc(a, 3, 1);
    void *p = mem_alloc(a, 32, 16);
    cr_assert_not_null(p);
    cr_assert_eq((uintptr_t)p % 16, 0u);
}

/* ── OOM ────────────────────────────────────────────────────────────────────
 */

Test(fixed_arena, alloc_oom_returns_null)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, BUF_SIZE + 1, 1);
    cr_assert_null(p);
}

Test(fixed_arena, alloc_fills_to_exact_capacity)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, BUF_SIZE, 1);
    cr_assert_not_null(p);
    void *q = mem_alloc(a, 1, 1);
    cr_assert_null(q);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

Test(fixed_arena, reset_rewinds_offset)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 64, 1);
    fixed_arena_reset(&arena);
    void *p2 = mem_alloc(a, 64, 1);
    cr_assert_eq(p1, p2);
}

Test(fixed_arena, reset_allows_full_reuse)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_alloc(a, BUF_SIZE, 1);
    fixed_arena_reset(&arena);
    void *p = mem_alloc(a, BUF_SIZE, 1);
    cr_assert_not_null(p);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

Test(fixed_arena, realloc_inplace_last_alloc)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = mem_alloc(a, 16, 1);
    memset(p, 0xCD, 16);
    uint8_t *p2 = mem_realloc(a, p, 16, 32, 1);
    /* must be in-place */
    cr_assert_eq(p, p2);
    /* original content preserved */
    for (int i = 0; i < 16; ++i)
        cr_assert_eq(p2[i], (uint8_t)0xCD);
}

Test(fixed_arena, realloc_general_copies_content)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p1 = mem_alloc(a, 16, 1);
    memset(p1, 0x11, 16);
    mem_alloc(a, 8, 1); /* bump so p1 is no longer last */
    uint8_t *p2 = mem_realloc(a, p1, 16, 16, 1);
    cr_assert_not_null(p2);
    cr_assert_neq(p1, p2);
    for (int i = 0; i < 16; ++i)
        cr_assert_eq(p2[i], (uint8_t)0x11);
}

Test(fixed_arena, realloc_oom_returns_null)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = mem_alloc(a, BUF_SIZE / 2, 1);
    mem_alloc(a, 1, 1); /* consume most of the rest */
    uint8_t *p2 = mem_realloc(a, p, BUF_SIZE / 2, BUF_SIZE, 1);
    cr_assert_null(p2);
}

/* ── free ───────────────────────────────────────────────────────────────────
 */

Test(fixed_arena, free_is_noop_no_crash)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_alloc(a, 32, 1);
    mem_free(a, p, 32); /* must not crash */
    void *q = mem_alloc(a, 32, 1);
    cr_assert_not_null(q); /* arena still usable */
}

/* ── scratch ────────────────────────────────────────────────────────────────
 */

Test(fixed_arena, scratch_rewinds_on_end)
{
    allocator_t a = fixed_arena_allocator(&arena);
    size_t before = arena.offset;
    scratch_t s;
    fixed_arena_scratch_begin(&s, &arena);
    mem_alloc(scratch_allocator(&s), 128, 1);
    scratch_end(&s);
    cr_assert_eq(arena.offset, before);
}

Test(fixed_arena, scratch_alloc_is_visible_before_end)
{
    scratch_t s;
    fixed_arena_scratch_begin(&s, &arena);
    allocator_t sa = scratch_allocator(&s);
    uint8_t *p = mem_alloc(sa, 32, 1);
    cr_assert_not_null(p);
    memset(p, 0x55, 32);
    cr_assert_eq(p[0], (uint8_t)0x55);
    scratch_end(&s);
}

/* ── NULL-ptr contract ──────────────────────────────────────────────────────
 */

Test(fixed_arena, realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = fixed_arena_allocator(&arena);
    void *p = mem_realloc(a, NULL, 0, 32, 1);
    cr_assert_not_null(p);
}

Test(fixed_arena, free_null_is_noop)
{
    allocator_t a = fixed_arena_allocator(&arena);
    mem_free(a, NULL, 0); /* must not crash */
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

Test(fixed_arena, stats_reports_used_and_capacity)
{
    allocator_t a = fixed_arena_allocator(&arena);
    arena_stats_t s0 = fixed_arena_stats(&arena);
    cr_assert_eq(s0.used, 0u);
    cr_assert_eq(s0.capacity, (size_t)BUF_SIZE);

    mem_alloc(a, 64, 1);
    arena_stats_t s1 = fixed_arena_stats(&arena);
    cr_assert_geq(s1.used, 64u);
    cr_assert_leq(s1.used, (size_t)BUF_SIZE);
    cr_assert_eq(s1.capacity, (size_t)BUF_SIZE);
}

/* ── mem_calloc ─────────────────────────────────────────────────────────────
 */

Test(fixed_arena, mem_calloc_returns_zeroed_memory)
{
    allocator_t a = fixed_arena_allocator(&arena);
    uint8_t *p = mem_calloc(a, 64, 1);
    cr_assert_not_null(p);
    for (int i = 0; i < 64; ++i)
        cr_assert_eq(p[i], (uint8_t)0);
}

/* ── ALLOCATOR_NULL ─────────────────────────────────────────────────────────
 */

Test(fixed_arena, allocator_null_alloc_returns_null)
{
    void *p = mem_alloc(ALLOCATOR_NULL, 32, 1);
    cr_assert_null(p);
}

Test(fixed_arena, allocator_null_realloc_returns_null)
{
    void *p = mem_realloc(ALLOCATOR_NULL, NULL, 0, 32, 1);
    cr_assert_null(p);
}

Test(fixed_arena, allocator_null_free_is_noop)
{
    uint8_t tmp[4];
    mem_free(ALLOCATOR_NULL, tmp, 4); /* vt==NULL — must not crash */
}
