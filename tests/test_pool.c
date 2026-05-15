#include <gtest/gtest.h>
#include <stdint.h>
#include <string.h>

extern "C" {
#include "arena/pool.h"
}

#define OBJECT_SIZE 32
#define CAPACITY 16

static pool_t pool;

class PoolTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        ASSERT_EQ(pool_init(&pool, OBJECT_SIZE, CAPACITY), 0);
    }
    void TearDown() override { pool_destroy(&pool); }
};

/* ── basic alloc / free ─────────────────────────────────────────────────────
 */

TEST_F(PoolTest, alloc_returns_nonnull)
{
    void *p = mem_alloc(pool_allocator(&pool), OBJECT_SIZE, 1);
    ASSERT_NE(p, nullptr);
}

TEST_F(PoolTest, alloc_writes_are_readable)
{
    allocator_t a = pool_allocator(&pool);
    uint8_t *p = (uint8_t *)mem_alloc(a, OBJECT_SIZE, 1);
    ASSERT_NE(p, nullptr);
    memset(p, 0xCC, OBJECT_SIZE);
    for (int i = 0; i < OBJECT_SIZE; ++i)
        EXPECT_EQ(p[i], (uint8_t)0xCC);
}

TEST_F(PoolTest, free_returns_slot_to_pool)
{
    allocator_t a = pool_allocator(&pool);
    void *p1 = mem_alloc(a, OBJECT_SIZE, 1);
    ASSERT_NE(p1, nullptr);
    mem_free(a, p1, OBJECT_SIZE);
    void *p2 = mem_alloc(a, OBJECT_SIZE, 1);
    ASSERT_NE(p2, nullptr);
    EXPECT_EQ(p1, p2); /* same slot reused */
}

/* ── capacity ───────────────────────────────────────────────────────────────
 */

TEST_F(PoolTest, fills_to_capacity)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
    {
        void *p = mem_alloc(a, OBJECT_SIZE, 1);
        ASSERT_NE(p, nullptr);
    }
    EXPECT_EQ(pool.count, (size_t)CAPACITY);
}

TEST_F(PoolTest, oom_beyond_capacity)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
        mem_alloc(a, OBJECT_SIZE, 1);
    void *p = mem_alloc(a, OBJECT_SIZE, 1);
    EXPECT_EQ(p, nullptr);
}

TEST_F(PoolTest, count_tracks_live_objects)
{
    allocator_t a = pool_allocator(&pool);
    EXPECT_EQ(pool.count, 0u);

    void *p = mem_alloc(a, OBJECT_SIZE, 1);
    EXPECT_EQ(pool.count, 1u);

    mem_free(a, p, OBJECT_SIZE);
    EXPECT_EQ(pool.count, 0u);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

TEST_F(PoolTest, realloc_within_slot_is_inplace)
{
    allocator_t a = pool_allocator(&pool);
    void *p = mem_alloc(a, OBJECT_SIZE, 1);
    void *p2 = mem_realloc(a, p, OBJECT_SIZE, OBJECT_SIZE / 2, 1);
    EXPECT_EQ(p, p2);
}

TEST_F(PoolTest, realloc_beyond_slot_returns_null)
{
    allocator_t a = pool_allocator(&pool);
    void *p = mem_alloc(a, OBJECT_SIZE, 1);
    void *p2 = mem_realloc(a, p, OBJECT_SIZE, OBJECT_SIZE + 1, 1);
    EXPECT_EQ(p2, nullptr);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

TEST_F(PoolTest, reset_restores_all_slots)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
        mem_alloc(a, OBJECT_SIZE, 1);
    EXPECT_EQ(pool.count, (size_t)CAPACITY);

    pool_reset(&pool);

    EXPECT_EQ(pool.count, 0u);
    for (size_t i = 0; i < CAPACITY; ++i)
    {
        void *p = mem_alloc(a, OBJECT_SIZE, 1);
        ASSERT_NE(p, nullptr);
    }
}

/* ── slot size enforcement ──────────────────────────────────────────────────
 */

TEST_F(PoolTest, alloc_rejects_oversized_request)
{
    allocator_t a = pool_allocator(&pool);
    void *p = mem_alloc(a, OBJECT_SIZE + 1, 1);
    EXPECT_EQ(p, nullptr);
}

/* ── pointer alignment ──────────────────────────────────────────────────────
 */

TEST_F(PoolTest, slots_are_pointer_aligned)
{
    allocator_t a = pool_allocator(&pool);
    for (size_t i = 0; i < CAPACITY; ++i)
    {
        void *p = mem_alloc(a, OBJECT_SIZE, 1);
        EXPECT_EQ((uintptr_t)p % sizeof(void *), 0u);
    }
}

/* ── nullptr-ptr contract ──────────────────────────────────────────────────────
 */

TEST_F(PoolTest, realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = pool_allocator(&pool);
    void *p = mem_realloc(a, nullptr, 0, OBJECT_SIZE, 1);
    ASSERT_NE(p, nullptr);
}

TEST_F(PoolTest, free_null_is_noop)
{
    allocator_t a = pool_allocator(&pool);
    mem_free(a, nullptr, 0); /* pool_free dereferences ptr — guard is critical */
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

TEST_F(PoolTest, stats_tracks_live_objects)
{
    allocator_t a = pool_allocator(&pool);
    arena_stats_t s0 = pool_stats(&pool);
    EXPECT_EQ(s0.used, 0u);
    EXPECT_EQ(s0.capacity, (size_t)(OBJECT_SIZE * CAPACITY));

    mem_alloc(a, OBJECT_SIZE, 1);
    arena_stats_t s1 = pool_stats(&pool);
    EXPECT_EQ(s1.used, (size_t)OBJECT_SIZE);

    mem_alloc(a, OBJECT_SIZE, 1);
    arena_stats_t s2 = pool_stats(&pool);
    EXPECT_EQ(s2.used, (size_t)(2 * OBJECT_SIZE));
}
