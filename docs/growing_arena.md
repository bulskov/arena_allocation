# growing_arena_t

Chained bump allocator. When the current block is exhausted a new block is
`mmap`'d and prepended to the chain. All blocks are released on `destroy`.

The first `mmap` is deferred to the first allocation, so `init` never fails.

## Creator API

```c
void  growing_arena_init(growing_arena_t *a, size_t block_size);
void  growing_arena_destroy(growing_arena_t *a);
void  growing_arena_reset(growing_arena_t *a);
void  growing_arena_reset_full(growing_arena_t *a);
void  growing_arena_scratch_begin(scratch_t *s, growing_arena_t *a);
allocator_t   growing_arena_allocator(growing_arena_t *a);
arena_stats_t growing_arena_stats(const growing_arena_t *a);
```

| Function           | Description                                                                                                                                        |
| ------------------ | -------------------------------------------------------------------------------------------------------------------------------------------------- |
| `init(block_size)` | Set minimum bytes per block. No mmap yet.                                                                                                          |
| `destroy()`        | `munmap` all blocks.                                                                                                                               |
| `reset()`          | Free all blocks except the head; rewind head's offset.                                                                                             |
| `reset_full()`     | Free **all** blocks including the head; arena is as if freshly initialised. Use when the head block may be oversized from a past large allocation. |
| `scratch_begin(s)` | Save block chain + offset; see [scratch_t](scratch.md).                                                                                            |
| `allocator()`      | Produce an `allocator_t` for user code.                                                                                                            |
| `stats()`          | Return current usage snapshot (sums across all blocks).                                                                                            |

### `reset()` vs `reset_full()`

`reset()` keeps the head block so the next cycle avoids an `mmap` call. If a
previous allocation forced a larger-than-normal block, that large block is
retained. Use `reset_full()` to release all memory when this is a concern.

## Allocator behaviour

| Operation                       | Behaviour                                                                                                 |
| ------------------------------- | --------------------------------------------------------------------------------------------------------- |
| `alloc(size, align)`            | Bump pointer in current block; `mmap` a new block if needed.                                              |
| `realloc(ptr, old, new, align)` | In-place if `ptr` is the last allocation in the current block; otherwise alloc+copy. `ptr==NULL` → alloc. |
| `free(ptr, size)`               | No-op. `ptr==NULL` is safe.                                                                               |

Memory is **not zeroed** on alloc or reset. Use `mem_calloc` for zero-init.

## Typical use

```c
growing_arena_t arena;
growing_arena_init(&arena, 64 * 1024); /* 64 KB per block */

allocator_t a = growing_arena_allocator(&arena);
/* ... hand 'a' to user code ... */

growing_arena_reset(&arena);   /* end of request / frame */
growing_arena_destroy(&arena); /* shutdown */
```
