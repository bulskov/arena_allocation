# allocator_t — the allocator interface

`allocator_t` is the only type user code ever sees. It is a fat pointer —
vtable + opaque context — passed by value (two words on the stack).

```c
typedef struct {
    void *(*alloc  )(void *ctx, size_t size, size_t align);
    void *(*realloc)(void *ctx, void *ptr,
                     size_t old_size, size_t new_size, size_t align);
    void  (*free   )(void *ctx, void *ptr, size_t size);
} allocator_vtable_t;

typedef struct {
    const allocator_vtable_t *vt;
    void                     *ctx;
} allocator_t;
```

## Convenience wrappers

All wrappers handle `ALLOCATOR_NULL` (vt == NULL) gracefully.

```c
void *mem_alloc  (allocator_t a, size_t size, size_t align);
void *mem_realloc(allocator_t a, void *ptr,
                  size_t old_size, size_t new_size, size_t align);
void  mem_free   (allocator_t a, void *ptr, size_t size);
void *mem_calloc (allocator_t a, size_t size, size_t align); /* alloc + zero */
```

## Contract

### `alloc(size, align)`
- Returns `size` bytes aligned to `align`, or `NULL` on failure.
- `align` must be a power of two ≥ 1.
- `size` must be > 0.

### `realloc(ptr, old_size, new_size, align)`
- Resizes `ptr`, preserving `min(old_size, new_size)` bytes of content.
- `ptr == NULL` behaves as `alloc(new_size, align)`.
- Returns `NULL` on failure; `ptr` remains valid in that case.
- The returned pointer may differ from `ptr`; callers must not assume stability.
- Implementations **may** extend in-place when `ptr` is the most recent
  allocation (bump arenas do this); callers must never rely on it.

### `free(ptr, size)`
- Releases `ptr` (`size` bytes).
- `ptr == NULL` is always a no-op.
- Bump allocators (`fixed_arena`, `growing_arena`, `virtual_arena`) treat free
  as a no-op. `pool_t` and `stack_arena_t` have O(1) free semantics.

## Null allocator

```c
allocator_t a = ALLOCATOR_NULL;   /* zero-initialised */
```

`alloc`/`realloc` return `NULL`; `free` is a no-op. Use as a safe uninitialised
sentinel, e.g. for an ambient allocator stack before the first push.

## stats

Individual arena types expose a `*_stats()` function that returns:

```c
typedef struct {
    size_t used;       /* bytes in active use                              */
    size_t capacity;   /* total backing capacity                           */
    size_t committed;  /* committed physical pages (virtual_arena differs) */
    size_t reserved;   /* reserved VA range      (virtual_arena differs)   */
} arena_stats_t;
```

## Ownership rule

The creator holds the **concrete** arena type and controls reset / destroy.
User code only receives `allocator_t` and cannot end the lifetime. The vtable
deliberately has no `destroy` operation.

## Thread safety

**None.** Every arena type is single-threaded by design.

- Do not share an arena between threads without external synchronisation.
- The intended pattern is **one arena per thread**: each thread owns its
  arenas for its entire lifetime and never hands them to another thread.
- `allocator_t` values (fat pointers) may be passed freely — they carry no
  state themselves — but the underlying arena they point to must only be
  driven from one thread at a time.
- There is no plan to add internal locking; a mutex would defeat the
  purpose of bump allocation.
