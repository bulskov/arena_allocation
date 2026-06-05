#include "arena/debug_allocator.h"
#include <stdint.h>
#include <string.h>

#define SENTINEL_ALLOC 0xCD
#define SENTINEL_FREE 0xDD

/* ---------- creator API -------------------------------------------------- */

void debug_allocator_init(debug_allocator_t *d, allocator_t inner)
{
    d->inner = inner;
    d->alloc_count = 0;
    d->free_count = 0;
    d->bytes_total = 0;
    d->bytes_live = 0;
    d->bytes_peak = 0;
}

void debug_allocator_reset(debug_allocator_t *d)
{
    d->alloc_count = 0;
    d->free_count = 0;
    d->bytes_total = 0;
    d->bytes_live = 0;
    d->bytes_peak = 0;
}

/* ---------- internal helpers --------------------------------------------- */

static void update_peak(debug_allocator_t *d)
{
    if (d->bytes_live > d->bytes_peak)
        d->bytes_peak = d->bytes_live;
}

/* ---------- vtable -------------------------------------------------------- */

static void *debug_alloc(void *ctx, size_t size, size_t align)
{
    debug_allocator_t *d = (debug_allocator_t *)ctx;
    void *p = mem_alloc(d->inner, size, align);
    if (!p)
        return NULL;
    memset(p, SENTINEL_ALLOC, size);
    d->alloc_count++;
    d->bytes_total += size;
    d->bytes_live += size;
    update_peak(d);
    return p;
}

static void *debug_realloc(
    void *ctx, void *ptr, size_t old_size, size_t new_size, size_t align)
{
    debug_allocator_t *d = (debug_allocator_t *)ctx;

    if (!ptr)
        return debug_alloc(ctx, new_size, align);

    void *result = mem_realloc(d->inner, ptr, old_size, new_size, align);
    if (!result)
        return NULL;

    if (result == ptr)
    {
        /* In-place: fill the changed region with the appropriate sentinel. */
        if (new_size > old_size)
            memset(
                (uint8_t *)ptr + old_size, SENTINEL_ALLOC, new_size - old_size);
        else if (new_size < old_size)
            memset(
                (uint8_t *)ptr + new_size, SENTINEL_FREE, old_size - new_size);
    }
    else
    {
        /*
         * Relocated: inner already copied min(old,new) bytes to result.
         * Fill any extension with alloc sentinel, and poison the old pointer
         * region so stale reads are detectable.
         */
        if (new_size > old_size)
            memset(
                (uint8_t *)result + old_size,
                SENTINEL_ALLOC,
                new_size - old_size);
        memset(ptr, SENTINEL_FREE, old_size);
    }

    d->bytes_live = d->bytes_live - old_size + new_size;
    update_peak(d);
    return result;
}

static void debug_free(void *ctx, void *ptr, size_t size)
{
    if (!ptr)
        return;
    debug_allocator_t *d = (debug_allocator_t *)ctx;
    memset(ptr, SENTINEL_FREE, size);
    mem_free(d->inner, ptr, size);
    d->free_count++;
    d->bytes_live -= size;
}

static const allocator_vtable_t debug_allocator_vtable = {
    .alloc = debug_alloc,
    .realloc = debug_realloc,
    .free = debug_free,
};

allocator_t debug_allocator_allocator(debug_allocator_t *d)
{
    return (allocator_t){.vt = &debug_allocator_vtable, .ctx = d};
}

debug_allocator_stats_t debug_allocator_stats(const debug_allocator_t *d)
{
    return (debug_allocator_stats_t){
        .alloc_count = d->alloc_count,
        .free_count = d->free_count,
        .bytes_total = d->bytes_total,
        .bytes_live = d->bytes_live,
        .bytes_peak = d->bytes_peak,
    };
}
