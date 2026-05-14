#include <criterion/criterion.h>
#include <stdint.h>
#include <string.h>

#include "arena/pool.h"

#define OBJECT_SIZE 32
#define CAPACITY    16

static pool_t pool;

static void setup(void)    { cr_assert_eq(pool_init(&pool, OBJECT_SIZE, CAPACITY), 0); }
static void teardown(void) { pool_destroy(&pool); }

TestSuite(pool, .init = setup, .fini = teardown);

/* ── basic alloc / free ───────────────────────────────────────────────────── */

Test(pool, alloc_returns_nonnull)
{
    void* p = mem_alloc(pool_allocator(&pool), OBJECT_SIZE, 1);
    cr_assert_not_null(p);
}

Test(pool, alloc_writes_are_readable)
{
    allocator_t a = pool_allocator(&pool);
    uint8_t*    p = mem_alloc(a, OBJECT_SIZE, 1);
    cr_assert_not_null(p);
    memset(p, 0xCC, OBJECT_SIZE);
    for (int i = 0; i < OBJECT_SIZE; ++i)
        cr_assert_eq(p[i], (uint8_t)0xCC);
}

Test(pool, free_returns_slot_to_pool)
{
    allocator_t a  = pool_allocator(&pool);
    void*       p1 = mem_alloc(a, OBJECT_SIZE, 1);
    cr_assert_not_null(p1);
    mem_free(a, p1, OBJECT_SIZE);
    void*       p2 = mem_alloc(a, OBJECT_SIZE, 1);
    cr_assert_not_null(p2);
    cr_assert_eq(p1, p2);       /* same slot reused */
}

/* ── capacity ─────────────────────────────────────────────────────────────── */

Test(pool, fills_to_capacity)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i) {
        void* p = mem_alloc(a, OBJECT_SIZE, 1);
        cr_assert_not_null(p, "alloc %zu returned NULL", i);
    }
    cr_assert_eq(pool.count, (size_t)CAPACITY);
}

Test(pool, oom_beyond_capacity)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
        mem_alloc(a, OBJECT_SIZE, 1);
    void* p = mem_alloc(a, OBJECT_SIZE, 1);
    cr_assert_null(p);
}

Test(pool, count_tracks_live_objects)
{
    allocator_t a = pool_allocator(&pool);
    cr_assert_eq(pool.count, 0u);

    void* p = mem_alloc(a, OBJECT_SIZE, 1);
    cr_assert_eq(pool.count, 1u);

    mem_free(a, p, OBJECT_SIZE);
    cr_assert_eq(pool.count, 0u);
}

/* ── realloc ──────────────────────────────────────────────────────────────── */

Test(pool, realloc_within_slot_is_inplace)
{
    allocator_t a  = pool_allocator(&pool);
    void*       p  = mem_alloc(a, OBJECT_SIZE, 1);
    void*       p2 = mem_realloc(a, p, OBJECT_SIZE, OBJECT_SIZE / 2, 1);
    cr_assert_eq(p, p2);
}

Test(pool, realloc_beyond_slot_returns_null)
{
    allocator_t a  = pool_allocator(&pool);
    void*       p  = mem_alloc(a, OBJECT_SIZE, 1);
    void*       p2 = mem_realloc(a, p, OBJECT_SIZE, OBJECT_SIZE + 1, 1);
    cr_assert_null(p2);
}

/* ── reset ────────────────────────────────────────────────────────────────── */

Test(pool, reset_restores_all_slots)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
        mem_alloc(a, OBJECT_SIZE, 1);
    cr_assert_eq(pool.count, (size_t)CAPACITY);

    pool_reset(&pool);

    cr_assert_eq(pool.count, 0u);
    for (size_t i = 0; i < CAPACITY; ++i) {
        void* p = mem_alloc(a, OBJECT_SIZE, 1);
        cr_assert_not_null(p, "post-reset alloc %zu returned NULL", i);
    }
}

/* ── slot size enforcement ────────────────────────────────────────────────── */

Test(pool, alloc_rejects_oversized_request)
{
    allocator_t a = pool_allocator(&pool);
    void* p = mem_alloc(a, OBJECT_SIZE + 1, 1);
    cr_assert_null(p);
}

/* ── pointer alignment ────────────────────────────────────────────────────── */

Test(pool, slots_are_pointer_aligned)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i) {
        void* p = mem_alloc(a, OBJECT_SIZE, 1);
        cr_assert_eq((uintptr_t)p % sizeof(void*), 0u,
                     "slot %zu not pointer-aligned", i);
    }
}
