# fixed_arena_t

Bump allocator over an **externally-provided buffer**. The caller owns the
buffer and is responsible for its lifetime. The buffer may be a stack array,
a global, or a region obtained from `mem_map()`.

No mmap is performed by the arena itself — use `growing_arena_t` or
`virtual_arena_t` if you want self-managed backing.

## Creator API

```c
void  fixed_arena_init(fixed_arena_t *a, void *buf, size_t size);
void  fixed_arena_reset(fixed_arena_t *a);
void  fixed_arena_scratch_begin(scratch_t *s, fixed_arena_t *a);
allocator_t   fixed_arena_allocator(fixed_arena_t *a);
arena_stats_t fixed_arena_stats(const fixed_arena_t *a);
```

| Function           | Description                                                 |
| ------------------ | ----------------------------------------------------------- |
| `init(buf, size)`  | Attach to an existing buffer. No allocation.                |
| `reset()`          | Rewind bump pointer to zero. Buffer contents are untouched. |
| `scratch_begin(s)` | Save current offset; see [scratch_t](scratch.md).           |
| `allocator()`      | Produce an `allocator_t` for user code.                     |
| `stats()`          | Return current usage snapshot.                              |

## Allocator behaviour

| Operation                       | Behaviour                                                                            |
| ------------------------------- | ------------------------------------------------------------------------------------ |
| `alloc(size, align)`            | Bump pointer; returns NULL if capacity exceeded.                                     |
| `realloc(ptr, old, new, align)` | In-place if `ptr` is the last allocation; otherwise alloc+copy. `ptr==NULL` → alloc. |
| `free(ptr, size)`               | No-op. `ptr==NULL` is safe.                                                          |

Memory is **not zeroed** on alloc or reset. Use `mem_calloc` for zero-init.

## Limitations

- Fixed capacity; returns NULL when exhausted — no growth.
- Individual frees are no-ops; reclaim memory via `reset()` or `scratch_end()`.
- Does not own its buffer; caller must keep the buffer alive.

## Typical use

```c
uint8_t buf[4096];
fixed_arena_t arena;
fixed_arena_init(&arena, buf, sizeof(buf));

allocator_t a = fixed_arena_allocator(&arena);
MyStruct *s = mem_alloc(a, sizeof(MyStruct), _Alignof(MyStruct));

fixed_arena_reset(&arena); /* reuse the same buffer */
```
