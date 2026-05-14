# debug_allocator_t

A diagnostic wrapper around any `allocator_t`. It passes all operations
through to an inner allocator while filling memory with sentinel byte patterns
and tracking five allocation counters.

## Sentinels

| Pattern        | Value  | Meaning                                                         |
| -------------- | ------ | --------------------------------------------------------------- |
| Alloc sentinel | `0xCD` | Memory is freshly allocated but uninitialised                   |
| Free sentinel  | `0xDD` | Memory has been freed; reads through stale pointers are visible |

On every successful `alloc` or `realloc(NULL)` the returned region is filled
with `0xCD`. On `free` the region is filled with `0xDD` before it is handed
back to the inner allocator. For in-place `realloc`:

- **Growing** — the extension `[old_size, new_size)` is filled with `0xCD`.
- **Shrinking** — the discarded tail `[new_size, old_size)` is filled with
  `0xDD`.

For **relocating** `realloc` (the inner allocator returned a different
pointer): the extension is filled with `0xCD` and the entire old pointer
region is filled with `0xDD`, making stale reads of the old pointer
immediately visible.

## Creator API

```c
#include "arena/debug_allocator.h"
```

| Function                         | Description                                                                       |
| -------------------------------- | --------------------------------------------------------------------------------- |
| `debug_allocator_init(d, inner)` | Attach to `inner`; zero all counters. `inner` is not owned — it must outlive `d`. |
| `debug_allocator_reset(d)`       | Zero all counters. Does not affect the inner arena.                               |
| `debug_allocator_allocator(d)`   | Return an `allocator_t` for user code.                                            |
| `debug_allocator_stats(d)`       | Return a `debug_allocator_stats_t` snapshot.                                      |

There is no `destroy` function — the wrapper owns nothing.

## Stats

```c
typedef struct {
    size_t alloc_count;  /* successful alloc / realloc(NULL) calls */
    size_t free_count;   /* free calls (free(NULL) not counted) */
    size_t bytes_total;  /* cumulative bytes from alloc / realloc(NULL) */
    size_t bytes_live;   /* bytes currently in active use */
    size_t bytes_peak;   /* high-watermark of bytes_live */
} debug_allocator_stats_t;
```

`bytes_live` after a teardown sequence should be zero; a non-zero value
indicates a leak. `bytes_peak` is never lowered by frees.

## Typical use

Wrap the allocator you intend to test, run your code, then inspect the stats:

```c
growing_arena_t arena;
growing_arena_init(&arena, 64 * 1024);

debug_allocator_t dbg;
debug_allocator_init(&dbg, growing_arena_allocator(&arena));

allocator_t a = debug_allocator_allocator(&dbg);

/* --- code under test --- */
my_thing_t *t = mem_alloc(a, sizeof *t, _Alignof(*t));
my_thing_destroy(t, a);
/* ----------------------- */

debug_allocator_stats_t s = debug_allocator_stats(&dbg);
assert(s.bytes_live == 0);   /* no leaks */
assert(s.alloc_count == s.free_count);

growing_arena_destroy(&arena);
```

## Limitations

- Does not detect **out-of-bounds writes** — only the exact bytes returned by
  the allocator are filled; a write one byte past the end lands in padding or
  the next allocation and is not caught.
- Does not track **individual allocations** — there are no per-pointer
  records, so `bytes_live` going negative (double-free) will wrap around.
  Use an assert on `bytes_live` before decrementing if you want to catch this.
- The wrapper adds a `memset` on every alloc and free. Sentinel filling has a
  real cost; do not enable it in production builds.
