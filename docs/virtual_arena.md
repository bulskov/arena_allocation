# virtual_arena_t

Bump allocator over a large **reserved** virtual address range. Physical pages
are committed in chunks as allocations demand them. On `reset`, committed pages
are decommitted and returned to the OS while the virtual reservation is kept.

This is the preferred arena for large or unpredictably-sized working sets: the
VA reservation is cheap; physical pages are only paid for while in use.

## Creator API

```c
int   virtual_arena_init(virtual_arena_t *a,
                         size_t reserved_size, size_t commit_chunk);
void  virtual_arena_destroy(virtual_arena_t *a);
void  virtual_arena_reset(virtual_arena_t *a);
void  virtual_arena_scratch_begin(scratch_t *s, virtual_arena_t *a);
allocator_t   virtual_arena_allocator(virtual_arena_t *a);
arena_stats_t virtual_arena_stats(const virtual_arena_t *a);
```

| Function                | Description                                                                |
| ----------------------- | -------------------------------------------------------------------------- |
| `init(reserved, chunk)` | Reserve VA range (no physical pages). Returns 0 on success, -1 on failure. |
| `destroy()`             | Decommit + release the entire VA range.                                    |
| `reset()`               | Decommit all physical pages; keep VA reservation.                          |
| `scratch_begin(s)`      | Save offset; `scratch_end` decommits pages beyond the mark.                |
| `allocator()`           | Produce an `allocator_t` for user code.                                    |
| `stats()`               | `used=offset`, `committed=committed_pages`, `reserved=full_VA`.            |

## Allocator behaviour

| Operation                       | Behaviour                                                                                   |
| ------------------------------- | ------------------------------------------------------------------------------------------- |
| `alloc(size, align)`            | Bump pointer; commit more pages if needed; returns NULL on commit failure or VA exhaustion. |
| `realloc(ptr, old, new, align)` | In-place if `ptr` is the last allocation; otherwise alloc+copy. `ptr==NULL` → alloc.        |
| `free(ptr, size)`               | No-op. `ptr==NULL` is safe.                                                                 |

After `reset()` and `scratch_end()`, newly committed pages are OS-zeroed
(Linux/Windows both zero pages on commit). Existing in-use pages are not
zeroed. Use `mem_calloc` if you need explicit zero-init.

## Limitations

- VA reservation is fixed at init time; cannot grow beyond it.
- Individual frees are no-ops; reclaim pages via `reset()` or `scratch_end()`.
- Requires VM overcommit or a large enough address space.

## Typical use

```c
virtual_arena_t arena;
virtual_arena_init(&arena,
    2ULL * 1024 * 1024 * 1024, /* 2 GB VA reservation */
    2 * 1024 * 1024);           /* commit in 2 MB chunks */

allocator_t a = virtual_arena_allocator(&arena);
/* ... use a for the whole session ... */

virtual_arena_reset(&arena);   /* return physical pages to OS */
virtual_arena_destroy(&arena); /* release VA */
```
