#include <criterion/criterion.h>
#include <stdint.h>
#include <string.h>

#include "arena/virtual_arena.h"
#include "platform.h" /* mem_page_size */

/* Reserve 16 MB, commit in 64 KB chunks. */
#define RESERVED (16u * 1024u * 1024u)
#define COMMIT_CHUNK (64u * 1024u)

static virtual_arena_t arena;

static void setup(void)
{
    cr_assert_eq(virtual_arena_init(&arena, RESERVED, COMMIT_CHUNK), 0);
}
static void teardown(void)
{
    virtual_arena_destroy(&arena);
}

TestSuite(virtual_arena, .init = setup, .fini = teardown);

/* ── basic allocation ───────────────────────────────────────────────────────
 */

Test(virtual_arena, alloc_returns_nonnull)
{
    void *p = mem_alloc(virtual_arena_allocator(&arena), 64, 1);
    cr_assert_not_null(p);
}

Test(virtual_arena, alloc_writes_are_readable)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p = mem_alloc(a, 16, 1);
    cr_assert_not_null(p);
    memset(p, 0xF0, 16);
    for (int i = 0; i < 16; ++i)
        cr_assert_eq(p[i], (uint8_t)0xF0);
}

Test(virtual_arena, alloc_advances_sequentially)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p1 = mem_alloc(a, 32, 1);
    uint8_t *p2 = mem_alloc(a, 32, 1);
    cr_assert_not_null(p1);
    cr_assert_not_null(p2);
    cr_assert_eq((uintptr_t)p2 - (uintptr_t)p1, 32u);
}

/* ── alignment ──────────────────────────────────────────────────────────────
 */

Test(virtual_arena, alignment_8)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, 3, 1);
    void *p = mem_alloc(a, 8, 8);
    cr_assert_not_null(p);
    cr_assert_eq((uintptr_t)p % 8, 0u);
}

Test(virtual_arena, alignment_64)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, 7, 1);
    void *p = mem_alloc(a, 64, 64);
    cr_assert_not_null(p);
    cr_assert_eq((uintptr_t)p % 64, 0u);
}

/* ── commit on demand ───────────────────────────────────────────────────────
 */

Test(virtual_arena, nothing_committed_before_first_alloc)
{
    cr_assert_eq(arena.committed, 0u);
}

Test(virtual_arena, commits_on_first_alloc)
{
    mem_alloc(virtual_arena_allocator(&arena), 1, 1);
    cr_assert_gt(arena.committed, 0u);
}

Test(virtual_arena, committed_grows_in_chunks)
{
    allocator_t a = virtual_arena_allocator(&arena);
    /* First alloc triggers the first commit chunk. */
    mem_alloc(a, 1, 1);
    size_t committed_after_first = arena.committed;
    cr_assert_eq(committed_after_first, COMMIT_CHUNK);

    /* Stay within the first chunk — committed must not change. */
    mem_alloc(a, COMMIT_CHUNK / 2, 1);
    cr_assert_eq(arena.committed, committed_after_first);

    /* Cross into the next chunk. */
    mem_alloc(a, COMMIT_CHUNK, 1);
    cr_assert_gt(arena.committed, committed_after_first);
}

/* ── OOM ────────────────────────────────────────────────────────────────────
 */

Test(virtual_arena, alloc_beyond_reserved_returns_null)
{
    virtual_arena_t small;
    cr_assert_eq(
        virtual_arena_init(&small, mem_page_size(), mem_page_size()), 0);
    allocator_t a = virtual_arena_allocator(&small);
    /* Exhaust the reserved range. */
    mem_alloc(a, mem_page_size(), 1);
    void *p = mem_alloc(a, 1, 1);
    cr_assert_null(p);
    virtual_arena_destroy(&small);
}

/* ── reset ──────────────────────────────────────────────────────────────────
 */

Test(virtual_arena, reset_rewinds_offset)
{
    allocator_t a = virtual_arena_allocator(&arena);
    void *p1 = mem_alloc(a, 64, 1);
    virtual_arena_reset(&arena);
    void *p2 = mem_alloc(a, 64, 1);
    cr_assert_eq(p1, p2);
}

Test(virtual_arena, reset_decommits_pages)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, COMMIT_CHUNK * 2, 1);
    cr_assert_gt(arena.committed, 0u);
    virtual_arena_reset(&arena);
    cr_assert_eq(arena.committed, 0u);
    cr_assert_eq(arena.offset, 0u);
}

/* ── realloc ────────────────────────────────────────────────────────────────
 */

Test(virtual_arena, realloc_inplace_last_alloc)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p = mem_alloc(a, 16, 1);
    memset(p, 0x77, 16);
    uint8_t *p2 = mem_realloc(a, p, 16, 32, 1);
    cr_assert_eq(p, p2);
    for (int i = 0; i < 16; ++i)
        cr_assert_eq(p2[i], (uint8_t)0x77);
}

Test(virtual_arena, realloc_general_preserves_content)
{
    allocator_t a = virtual_arena_allocator(&arena);
    uint8_t *p1 = mem_alloc(a, 16, 1);
    memset(p1, 0x33, 16);
    mem_alloc(a, 8, 1);
    uint8_t *p2 = mem_realloc(a, p1, 16, 16, 1);
    cr_assert_not_null(p2);
    cr_assert_neq(p1, p2);
    for (int i = 0; i < 16; ++i)
        cr_assert_eq(p2[i], (uint8_t)0x33);
}

/* ── scratch ────────────────────────────────────────────────────────────────
 */

Test(virtual_arena, scratch_rewinds_offset)
{
    allocator_t a = virtual_arena_allocator(&arena);
    mem_alloc(a, 64, 1);
    size_t offset_before = arena.offset;

    scratch_t s;
    virtual_arena_scratch_begin(&s, &arena);
    mem_alloc(scratch_allocator(&s), COMMIT_CHUNK * 2, 1);
    scratch_end(&s);

    cr_assert_eq(arena.offset, offset_before);
}

Test(virtual_arena, scratch_decommits_on_end)
{
    scratch_t s;
    virtual_arena_scratch_begin(&s, &arena);
    /* Force at least two commit chunks. */
    mem_alloc(scratch_allocator(&s), COMMIT_CHUNK * 3, 1);
    size_t committed_during = arena.committed;
    cr_assert_gt(committed_during, 0u);

    scratch_end(&s);

    cr_assert_lt(arena.committed, committed_during);
}
