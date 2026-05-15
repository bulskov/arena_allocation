# arena_allocation

A small C11 library of arena allocators. All arenas expose the same two-word
`allocator_t` interface to user code; only the creator holds the concrete type
and controls the lifetime.

## Design principles

- **Explicit ownership.** The arena creator decides when to reset or destroy.
  User code only receives an `allocator_t` and cannot end the lifetime.
- **No malloc.** Every arena uses `mmap` (or `VirtualAlloc` on Windows) for its
  backing memory.
- **Zero vtable overhead.** `allocator_t` is a fat pointer — vtable + context.
  Passed by value (two words). All calls dispatch through three function
  pointers: `alloc`, `realloc`, `free`.
- **NULL safety.** `realloc(NULL, size)` is equivalent to `alloc(size)`.
  `free(NULL)` is always a no-op.
- **Null allocator.** `ALLOCATOR_NULL` (zero-initialised `allocator_t`) is a
  safe sentinel: `alloc`/`realloc` return `NULL`, `free` is a no-op.

## Arena types

| Type                                           | Backing                       | `free`         | Scratch | Typical use                        |
| ---------------------------------------------- | ----------------------------- | -------------- | ------- | ---------------------------------- |
| [`fixed_arena_t`](docs/fixed_arena.md)         | external buffer               | no-op          | ✓       | stack buffer, embedded system      |
| [`growing_arena_t`](docs/growing_arena.md)     | chained mmap blocks           | no-op          | ✓       | general scoped allocation          |
| [`pool_t`](docs/pool.md)                       | single mmap block             | O(1) free-list | —       | many same-sized objects            |
| [`virtual_arena_t`](docs/virtual_arena.md)     | reserved VA, commit on demand | no-op          | ✓       | large / unpredictable working sets |
| [`stack_arena_t`](docs/stack_arena.md)         | single mmap block             | O(1) LIFO      | —       | LIFO / scope-stack patterns        |
| [`debug_allocator_t`](docs/debug_allocator.md) | wraps any `allocator_t`       | passthrough    | —       | testing, leak detection            |

Scratch sub-scopes work across the bump allocators; see
[`scratch_t`](docs/scratch.md).

`debug_allocator_t` is a diagnostic wrapper — it fills memory with sentinel
patterns (`0xCD` on alloc, `0xDD` on free) and tracks alloc counts and live
bytes.  Wrap any arena with it during testing; a non-zero `bytes_live` after
teardown indicates a leak.

## Quick start

```c
#include "arena/growing_arena.h"

growing_arena_t arena;
allocator_t a = growing_arena_allocator_new(&arena, 64 * 1024);

int *buf = mem_alloc(a, 256 * sizeof(int), _Alignof(int));
// ... use buf ...

growing_arena_reset(&arena);   // rewind; keep backing memory
growing_arena_destroy(&arena); // release backing memory
```

If you need to separate arena initialisation from allocator creation:

```c
#include "arena/growing_arena.h"

growing_arena_t arena;
growing_arena_init(&arena, 64 * 1024);

// ... some other code ...

allocator_t a = growing_arena_allocator(&arena);

int *buf = mem_alloc(a, 256 * sizeof(int), _Alignof(int));
// ... use buf ...

growing_arena_reset(&arena);   // rewind; keep backing memory
growing_arena_destroy(&arena); // release backing memory
```

## Allocator interface

See [`docs/allocator.md`](docs/allocator.md).

## Building

```sh
./build.sh          # default Debug build in ./build
BUILD_TYPE=Release ./build.sh
```

## Testing

Tests use [Google Test](https://github.com/google/googletest), fetched
automatically by CMake on first build — no manual installation needed.

```sh
./build.sh   # also builds tests
./test.sh
```

## Platform support

Linux (`mmap` / `mprotect` / `madvise`) and Windows (`VirtualAlloc` /
`VirtualFree`). Platform abstraction is in `src/platform.h` / `src/platform.c`
and is internal to the library.
