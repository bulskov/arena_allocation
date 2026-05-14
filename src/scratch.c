#include "arena/scratch.h"

void scratch_end(scratch_t *s) {
  s->pop(s->ctx, s->saved_block, s->saved_offset);
}

allocator_t scratch_allocator(scratch_t *s) { return s->parent_alloc; }
