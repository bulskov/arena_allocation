# scratch_t

A lightweight sub-scope over any bump arena. `scratch_begin` saves the arena's
current state; `scratch_end` restores it, discarding everything allocated
inside the scratch.

Supported arenas: `fixed_arena_t`, `growing_arena_t`, `virtual_arena_t`.

## API

```c
void        scratch_end(scratch_t *s);
allocator_t scratch_allocator(scratch_t *s);
```

Begin functions live on the individual arena types:

```c
void fixed_arena_scratch_begin  (scratch_t *s, fixed_arena_t   *a);
void growing_arena_scratch_begin(scratch_t *s, growing_arena_t *a);
void virtual_arena_scratch_begin(scratch_t *s, virtual_arena_t *a);
```

## Usage

```c
scratch_t sc;
growing_arena_scratch_begin(&sc, &arena);

allocator_t tmp = scratch_allocator(&sc);
void *buf = mem_alloc(tmp, 1024, 1);
/* ... temporary work ... */

scratch_end(&sc); /* rewind arena to state before scratch_begin */
```

## Nesting

Scratches can be nested. Each `scratch_begin` saves the current state;
each `scratch_end` restores only its own mark. Nesting must be LIFO —
the innermost scratch must be ended first.

```c
scratch_t s1, s2;
growing_arena_scratch_begin(&s1, &arena);
  growing_arena_scratch_begin(&s2, &arena);
    /* allocations here are discarded by scratch_end(&s2) */
  scratch_end(&s2);
  /* allocations here survive until scratch_end(&s1) */
scratch_end(&s1);
```

## Per-arena behaviour on `scratch_end`

| Arena             | What `scratch_end` does                                             |
| ----------------- | ------------------------------------------------------------------- |
| `fixed_arena_t`   | Rewind bump offset to saved value.                                  |
| `growing_arena_t` | Free blocks prepended after the mark; restore saved block's offset. |
| `virtual_arena_t` | Decommit pages beyond saved offset; return physical memory to OS.   |

## Limitations

- `pool_t` and `stack_arena_t` do not support scratch. `pool_t` has no linear
  state to rewind; `stack_arena_t` is itself a scratch-like mechanism.
- Scratch does not transfer ownership — the parent arena must outlive the scratch.
