# pool_t

O(1) fixed-size object allocator backed by a single `mmap`'d block. A
free-list threads through unused slots, so both `alloc` and `free` are O(1).

All slots have the same size (rounded up to pointer alignment at init time).

## Creator API

```c
int   pool_init(pool_t *p, size_t object_size, size_t capacity);
void  pool_destroy(pool_t *p);
void  pool_reset(pool_t *p);
allocator_t   pool_allocator(pool_t *p);
arena_stats_t pool_stats(const pool_t *p);
```

| Function                      | Description                                                                  |
| ----------------------------- | ---------------------------------------------------------------------------- |
| `init(object_size, capacity)` | `mmap` backing memory; build free-list. Returns 0 on success, -1 on failure. |
| `destroy()`                   | `munmap` backing memory.                                                     |
| `reset()`                     | Rebuild free-list; all live objects become invalid.                          |
| `allocator()`                 | Produce an `allocator_t` for user code.                                      |
| `stats()`                     | Return usage snapshot (`used = count × slot_size`).                          |

## Allocator behaviour

| Operation                       | Behaviour                                                                                                                     |
| ------------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| `alloc(size, align)`            | Pop from free-list; returns NULL if `size > slot_size` or pool is full.                                                       |
| `realloc(ptr, old, new, align)` | In-place (returns `ptr`) if `new_size ≤ slot_size`; otherwise NULL — pools do not cross size boundaries. `ptr==NULL` → alloc. |
| `free(ptr, size)`               | Push slot back onto free-list. `ptr==NULL` is a no-op.                                                                        |

Memory is **not zeroed** on alloc or reset. Use `mem_calloc` for zero-init.

## Limitations

- Fixed capacity; `alloc` returns NULL when the pool is exhausted.
- All objects must fit in `slot_size`; `realloc` to a larger size returns NULL.
- `reset()` invalidates all live objects.
- Scratch sub-scopes are not supported (there is no linear bump state to rewind).

## Typical use

```c
pool_t node_pool;
pool_init(&node_pool, sizeof(TreeNode), 1024);

allocator_t a = pool_allocator(&node_pool);

TreeNode *n = mem_alloc(a, sizeof(TreeNode), _Alignof(TreeNode));
/* ... use n ... */
mem_free(a, n, sizeof(TreeNode)); /* O(1), slot returned to pool */

pool_destroy(&node_pool);
```
