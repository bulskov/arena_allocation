# stack_arena_t

Bump allocator with O(1) **LIFO free** and no per-allocation headers.

All allocations are aligned to `min_align` and their sizes are rounded up to
`min_align`. This eliminates padding gaps between consecutive allocations,
which makes the LIFO check exact:

```
free(ptr, size) rewinds offset  ⟺  ptr + align_up(size, min_align) == base + offset
```

If the freed allocation is not the current top of the stack (LIFO violation),
`free` is a **silent no-op**. The bump pointer stays where it is; subsequent
allocations push on top. LIFO frees then work again from that new position.

## Creator API

```c
int   stack_arena_init(stack_arena_t *a, size_t capacity, size_t min_align);
void  stack_arena_destroy(stack_arena_t *a);
void  stack_arena_reset(stack_arena_t *a);
allocator_t   stack_arena_allocator(stack_arena_t *a);
arena_stats_t stack_arena_stats(const stack_arena_t *a);
```

| Function                    | Description                                                                                         |
| --------------------------- | --------------------------------------------------------------------------------------------------- |
| `init(capacity, min_align)` | `mmap` backing buffer. `min_align` must be a power of two ≥ 1. Returns 0 on success, -1 on failure. |
| `destroy()`                 | `munmap` backing buffer.                                                                            |
| `reset()`                   | Rewind offset to zero.                                                                              |
| `allocator()`               | Produce an `allocator_t` for user code.                                                             |
| `stats()`                   | Return usage snapshot.                                                                              |

## Allocator behaviour

| Operation                       | Behaviour                                                                                                                                                                   |
| ------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `alloc(size, align)`            | Start = `align_up(offset, min_align)`; slot = `align_up(size, min_align)`. Returns NULL if capacity exceeded. The `align` parameter is accepted but clamped to `min_align`. |
| `realloc(ptr, old, new, align)` | In-place if `ptr` is the current top; otherwise alloc+copy (old slot is not freed). `ptr==NULL` → alloc.                                                                    |
| `free(ptr, size)`               | Rewinds offset if `ptr` is the current top; otherwise silent no-op. `ptr==NULL` is a no-op.                                                                                 |

Memory is **not zeroed** on alloc or reset. Use `mem_calloc` for zero-init.

## The `min_align` contract

`min_align` governs **both** the start address and the slot size. This means:

- Every slot occupies exactly `align_up(size, min_align)` bytes.
- Consecutive slots are contiguous — no padding gaps between them.
- The LIFO check is exact with no header overhead.

If you request `align > min_align` via the vtable, the returned pointer is
still only guaranteed to be `min_align`-aligned. For strict per-allocation
alignment, set `min_align` accordingly at creation time.

## Limitations

- Fixed capacity (single mmap block); no growth.
- No scratch support — the stack *is* the scratch pattern. Use `reset()` to
  discard a scope's worth of allocations.
- LIFO violation leaves a "high watermark": bytes below the current offset
  cannot be individually freed; they are reclaimed only by `reset()`.

## Typical use

```c
stack_arena_t stack;
stack_arena_init(&stack, 64 * 1024, sizeof(void *)); /* 8-byte min_align */

allocator_t a = stack_arena_allocator(&stack);

Header *h = mem_alloc(a, sizeof(Header), 1);
Body   *b = mem_alloc(a, sizeof(Body),   1);
/* strict LIFO: */
mem_free(a, b, sizeof(Body));
mem_free(a, h, sizeof(Header));

stack_arena_destroy(&stack);
```
