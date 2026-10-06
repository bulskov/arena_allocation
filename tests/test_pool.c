#include "ctt.h"

#include <stdint.h>
#include <string.h>

#include "arena/pool.h"

#define OBJECT_SIZE 32
#define CAPACITY 16

static pool_t pool;

void ctt_before_each(void)
{
    ASSERT_EQ(pool_init(&pool, OBJECT_SIZE, CAPACITY), 0);
}

void ctt_after_each(void)
{
    pool_destroy(&pool);
}

/* ── basic alloc / free ─────────────────────────────────────────────────────
 */

TEST(alloc_returns_nonnull)
{
    void *p = mem_alloc(pool_allocator(&pool), OBJECT_SIZE, 1);
    ASSERT_NOT_NULL(p);
}

TEST(alloc_writes_are_readable)
{
    allocator_t a = pool_allocator(&pool);
    uint8_t *p = (uint8_t *)mem_alloc(a, OBJECT_SIZE, 1);
    ASSERT_NOT_NULL(p);
    memset(p, 0xCC, OBJECT_SIZE);
    for (int i = 0; i < OBJECT_SIZE; ++i)
        ASSERT_EQ((uint8_t)0xCC, p[i]);
}

TEST(free_returns_slot_to_pool)
{
    allocator_t a = pool_allocator(&pool);
    void *p1 = mem_alloc(a, OBJECT_SIZE, 1);
    ASSERT_NOT_NULL(p1);
    mem_free(a, p1, OBJECT_SIZE);
    void *p2 = mem_alloc(a, OBJECT_SIZE, 1);
    ASSERT_NOT_NULL(p2);
    ASSERT_PTR_EQ(p2, p1); /* same slot reused */
}

/* ── capacity ───────────────────────────────────────────────────────────────
 */

TEST(fills_to_capacity)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
    {
        void *p = mem_alloc(a, OBJECT_SIZE, 1);
        ASSERT_NOT_NULL(p);
    }
    ASSERT_EQ((size_t)CAPACITY, pool.count);
}

TEST(oom_beyond_capacity)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
        mem_alloc(a, OBJECT_SIZE, 1);
    void *p = mem_alloc(a, OBJECT_SIZE, 1);
    ASSERT_NULL(p);
}

TEST(count_tracks_live_objects)
{
    allocator_t a = pool_allocator(&pool);
    ASSERT_EQ(0u, pool.count);

    void *p = mem_alloc(a, OBJECT_SIZE, 1);
    ASSERT_EQ(1u, pool.count);

    mem_free(a, p, OBJECT_SIZE);
    ASSERT_EQ(0u, pool.count);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

TEST(realloc_within_slot_is_inplace)
{
    allocator_t a = pool_allocator(&pool);
    void *p = mem_alloc(a, OBJECT_SIZE, 1);
    void *p2 = mem_realloc(a, p, OBJECT_SIZE, OBJECT_SIZE / 2, 1);
    ASSERT_PTR_EQ(p2, p);
}

TEST(realloc_beyond_slot_returns_null)
{
    allocator_t a = pool_allocator(&pool);
    void *p = mem_alloc(a, OBJECT_SIZE, 1);
    void *p2 = mem_realloc(a, p, OBJECT_SIZE, OBJECT_SIZE + 1, 1);
    ASSERT_NULL(p2);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

TEST(reset_restores_all_slots)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
        mem_alloc(a, OBJECT_SIZE, 1);
    ASSERT_EQ((size_t)CAPACITY, pool.count);

    pool_reset(&pool);

    ASSERT_EQ(0u, pool.count);
    for (size_t i = 0; i < CAPACITY; ++i)
    {
        void *p = mem_alloc(a, OBJECT_SIZE, 1);
        ASSERT_NOT_NULL(p);
    }
}

/* ── slot size enforcement ──────────────────────────────────────────────────
 */

TEST(alloc_rejects_oversized_request)
{
    allocator_t a = pool_allocator(&pool);
    void *p = mem_alloc(a, OBJECT_SIZE + 1, 1);
    ASSERT_NULL(p);
}

/* ── pointer alignment ──────────────────────────────────────────────────────
 */

TEST(slots_are_pointer_aligned)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
    {
        void *p = mem_alloc(a, OBJECT_SIZE, 1);
        ASSERT_EQ(0u, (uintptr_t)p % sizeof(void *));
    }
}

/* ── NULL-ptr contract ──────────────────────────────────────────────────────
 */

TEST(realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = pool_allocator(&pool);
    void *p = mem_realloc(a, NULL, 0, OBJECT_SIZE, 1);
    ASSERT_NOT_NULL(p);
}

TEST(free_null_is_noop)
{
    allocator_t a = pool_allocator(&pool);
    mem_free(a, NULL, 0); /* pool_free dereferences ptr — guard is critical */
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

TEST(stats_tracks_live_objects)
{
    allocator_t a = pool_allocator(&pool);
    arena_stats_t s0 = pool_stats(&pool);
    ASSERT_EQ(0u, s0.used);
    ASSERT_EQ((size_t)(OBJECT_SIZE * CAPACITY), s0.capacity);

    mem_alloc(a, OBJECT_SIZE, 1);
    arena_stats_t s1 = pool_stats(&pool);
    ASSERT_EQ((size_t)OBJECT_SIZE, s1.used);

    mem_alloc(a, OBJECT_SIZE, 1);
    arena_stats_t s2 = pool_stats(&pool);
    ASSERT_EQ((size_t)(2 * OBJECT_SIZE), s2.used);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "pool");
}
