# fixed_arena_t

Bump allocator over a contiguous memory region. Supports two construction
modes: attach to an **externally-provided buffer** (caller owns it) or let the
arena **allocate its own backing** via `mmap`/`VirtualAlloc`.

## Creator API

```c
void  fixed_arena_init(fixed_arena_t *a, void *buf, size_t size);
int   fixed_arena_create(fixed_arena_t *a, size_t size);
void  fixed_arena_destroy(fixed_arena_t *a);
void  fixed_arena_reset(fixed_arena_t *a);
void  fixed_arena_scratch_begin(scratch_t *s, fixed_arena_t *a);
allocator_t   fixed_arena_allocator(fixed_arena_t *a);
allocator_t   fixed_arena_allocator_new(fixed_arena_t *a, size_t size);
arena_stats_t fixed_arena_stats(const fixed_arena_t *a);
```

| Function                | Description                                                                          |
| ----------------------- | ------------------------------------------------------------------------------------ |
| `init(buf, size)`       | Attach to an existing buffer. Arena does **not** own the buffer.                     |
| `create(a, size)`       | Allocate `size` bytes via mmap/VirtualAlloc. Arena owns the memory. Returns 0 on success, -1 on failure. |
| `destroy(a)`            | Release backing memory if owned; no-op if `init` was used. Safe to call twice.      |
| `reset()`               | Rewind bump pointer to zero. Buffer contents are untouched.                          |
| `scratch_begin(s)`      | Save current offset; see [scratch_t](scratch.md).                                   |
| `allocator()`           | Produce an `allocator_t` for user code.                                              |
| `allocator_new(a, size)`| `create` + `allocator` in one call. Returns `ALLOCATOR_NULL` on failure.            |
| `stats()`               | Return current usage snapshot.                                                       |

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

## Typical use

### External buffer (stack / global)

```c
uint8_t buf[4096];
fixed_arena_t arena;
fixed_arena_init(&arena, buf, sizeof(buf));

allocator_t a = fixed_arena_allocator(&arena);
MyStruct *s = mem_alloc(a, sizeof(MyStruct), _Alignof(MyStruct));

fixed_arena_reset(&arena); /* reuse the same buffer */
```

### Owned backing memory

```c
fixed_arena_t arena;
if (fixed_arena_create(&arena, 4096) != 0) { /* handle error */ }

allocator_t a = fixed_arena_allocator(&arena);
MyStruct *s = mem_alloc(a, sizeof(MyStruct), _Alignof(MyStruct));

fixed_arena_destroy(&arena); /* releases the mmap'd region */
```

### One-liner convenience

```c
fixed_arena_t arena;
allocator_t a = fixed_arena_allocator_new(&arena, 4096);
if (a.ctx == NULL) { /* handle error */ }

/* ... use a ... */

fixed_arena_destroy(&arena);
```
