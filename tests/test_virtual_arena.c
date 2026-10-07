#include "ctt.h"

#include <stdint.h>
#include <string.h>

#include "arena/virtual_arena.h"
#include "platform.h" /* mem_page_size */

/* Reserve 16 MB, commit in 64 KB chunks. */
#define RESERVED (16u * 1024u * 1024u)
#define COMMIT_CHUNK (64u * 1024u)

static virtual_arena_t arena;

void ctt_before_each(void)
{
    ASSERT_EQ(virtual_arena_init(&arena, RESERVED, COMMIT_CHUNK), 0);
}

void ctt_after_each(void)
{
    virtual_arena_destroy(&arena);
}

/* ── basic allocation ───────────────────────────────────────────────────────
 */

TEST(alloc_returns_nonnull)
{
    void *p = mem_alloc(virtual_arena_allocator(&arena), 64, 1);
    ASSERT_NOT_NULL(p);
}

TEST(alloc_writes_are_readable)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    ASSERT_NOT_NULL(p);
    memset(p, 0xF0, 16);
    for (int i = 0; i < 16; ++i)
    {
        ASSERT_EQ((uint8_t)0xF0, p[i]);
    }
}

TEST(alloc_advances_sequentially)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 32, 1);
    uint8_t *p2 = (uint8_t *)mem_alloc(a, 32, 1);
    ASSERT_NOT_NULL(p1);
    ASSERT_NOT_NULL(p2);
    ASSERT_EQ(32u, (uintptr_t)p2 - (uintptr_t)p1);
}

/* ── alignment ──────────────────────────────────────────────────────────────
 */

TEST(alignment_8)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, 3, 1);
    void *p = mem_alloc(a, 8, 8);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(0u, (uintptr_t)p % 8);
}

TEST(alignment_64)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, 7, 1);
    void *p = mem_alloc(a, 64, 64);
    ASSERT_NOT_NULL(p);
    ASSERT_EQ(0u, (uintptr_t)p % 64);
}

/* ── commit on demand ───────────────────────────────────────────────────────
 */

TEST(nothing_committed_before_first_alloc)
{
    ASSERT_EQ(0u, arena.committed);
}

TEST(commits_on_first_alloc)
{
    mem_alloc(virtual_arena_allocator(&arena), 1, 1);
    ASSERT_GT(arena.committed, 0u);
}

TEST(committed_grows_in_chunks)
{
    allocator_t a = virtual_arena_allocator(&arena);
    /* First alloc triggers the first commit chunk. */
    mem_alloc(a, 1, 1);
    size_t committed_after_first = arena.committed;
    ASSERT_EQ(COMMIT_CHUNK, committed_after_first);

    /* Stay within the first chunk — committed must not change. */
    mem_alloc(a, COMMIT_CHUNK / 2, 1);
    ASSERT_EQ(committed_after_first, arena.committed);

    /* Cross into the next chunk. */
    mem_alloc(a, COMMIT_CHUNK, 1);
    ASSERT_GT(arena.committed, committed_after_first);
}

/* ── OOM ────────────────────────────────────────────────────────────────────
 */

TEST(alloc_beyond_reserved_returns_null)
{
    virtual_arena_t small;
    ASSERT_EQ(0, virtual_arena_init(&small, mem_page_size(), mem_page_size()));
    allocator_t a = virtual_arena_allocator(&small);
    /* Exhaust the reserved range. */
    mem_alloc(a, mem_page_size(), 1);
    void *p = mem_alloc(a, 1, 1);
    ASSERT_NULL(p);
    virtual_arena_destroy(&small);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

TEST(reset_rewinds_offset)
{
    allocator_t a = virtual_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 64, 1);
    virtual_arena_reset(&arena);
    void *p2 = mem_alloc(a, 64, 1);
    ASSERT_PTR_EQ(p2, p1);
}

TEST(reset_decommits_pages)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, COMMIT_CHUNK * 2, 1);
    ASSERT_GT(arena.committed, 0u);
    virtual_arena_reset(&arena);
    ASSERT_EQ(0u, arena.committed);
    ASSERT_EQ(0u, arena.offset);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

TEST(realloc_inplace_last_alloc)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p, 0x77, 16);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p, 16, 32, 1);
    ASSERT_PTR_EQ(p2, p);
    for (int i = 0; i < 16; ++i)
    {
        ASSERT_EQ((uint8_t)0x77, p2[i]);
    }
}

TEST(realloc_general_preserves_content)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p1 = (uint8_t *)mem_alloc(a, 16, 1);
    memset(p1, 0x33, 16);
    mem_alloc(a, 8, 1);
    uint8_t *p2 = (uint8_t *)mem_realloc(a, p1, 16, 16, 1);
    ASSERT_NOT_NULL(p2);
    ASSERT_PTR_NE(p2, p1);
    for (int i = 0; i < 16; ++i)
    {
        ASSERT_EQ((uint8_t)0x33, p2[i]);
    }
}

/* ── scratch ────────────────────────────────────────────────────────────────
 */

TEST(scratch_rewinds_offset)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, 64, 1);
    size_t offset_before = arena.offset;

    scratch_t s;
    virtual_arena_scratch_begin(&s, &arena);
    mem_alloc(scratch_allocator(&s), COMMIT_CHUNK * 2, 1);
    scratch_end(&s);

    ASSERT_EQ(offset_before, arena.offset);
}

TEST(scratch_decommits_on_end)
{
    scratch_t s;
    virtual_arena_scratch_begin(&s, &arena);
    /* Force at least two commit chunks. */
    mem_alloc(scratch_allocator(&s), COMMIT_CHUNK * 3, 1);
    size_t committed_during = arena.committed;
    ASSERT_GT(committed_during, 0u);

    scratch_end(&s);

    ASSERT_LT(arena.committed, committed_during);
}

/* ── NULL-ptr contract ──────────────────────────────────────────────────────
 */

TEST(realloc_null_ptr_acts_as_alloc)
{
    allocator_t a = virtual_arena_allocator(&arena);
    void *p = mem_realloc(a, NULL, 0, 64, 1);
    ASSERT_NOT_NULL(p);
}

TEST(free_null_is_noop)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_free(a, NULL, 0); /* must not crash */
}

/* ── stats ──────────────────────────────────────────────────────────────────
 */

TEST(stats_reports_reserved_and_used)
{
    allocator_t a = virtual_arena_allocator(&arena);
    arena_stats_t s0 = virtual_arena_stats(&arena);
    ASSERT_EQ(0u, s0.used);
    ASSERT_EQ(0u, s0.committed); /* nothing committed yet */
    ASSERT_EQ((size_t)RESERVED, s0.reserved);

    mem_alloc(a, 64, 1);
    arena_stats_t s1 = virtual_arena_stats(&arena);
    ASSERT_GE(s1.used, 64u);
    ASSERT_GE(s1.committed, (size_t)COMMIT_CHUNK);
    ASSERT_EQ((size_t)RESERVED, s1.reserved);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "virtual_arena");
}
