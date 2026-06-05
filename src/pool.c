#include "arena/pool.h"
#include "platform.h"

static inline size_t align_up(size_t val, size_t align)
{
    return (val + align - 1) & ~(align - 1);
}

static void build_free_list(pool_t *p)
{
    p->free_list = NULL;
    p->count = 0;
    /* Build in reverse so the first slot ends up at the head. */
    for (size_t i = p->capacity; i > 0; --i)
    {
        pool_free_node_t *node =
            (pool_free_node_t *)(p->base + (i - 1) * p->slot_size);
        node->next = p->free_list;
        p->free_list = node;
    }
}

/* ---------- creator API -------------------------------------------------- */

int pool_init(pool_t *p, size_t object_size, size_t capacity)
{
    if (object_size == 0 || capacity == 0)
        return -1;

    /* Each slot must accommodate a free-list pointer and be pointer-aligned. */
    size_t slot = object_size < sizeof(pool_free_node_t *)
                      ? sizeof(pool_free_node_t *)
                      : object_size;
    if (slot > SIZE_MAX - (sizeof(void *) - 1)) /* align_up overflow */
        return -1;
    slot = align_up(slot, sizeof(void *));

    size_t page = mem_page_size();
    if (slot > SIZE_MAX / capacity)         /* slot * capacity overflow */
        return -1;
    size_t bytes = slot * capacity;
    if (bytes > SIZE_MAX - (page - 1))      /* align_up overflow */
        return -1;
    size_t total = align_up(bytes, page);

    uint8_t *base = (uint8_t *)mem_map(total);
    if (!base)
        return -1;

    p->base = base;
    p->slot_size = slot;
    p->capacity = capacity;
    build_free_list(p);
    return 0;
}

void pool_destroy(pool_t *p)
{
    size_t total = align_up(p->slot_size * p->capacity, mem_page_size());
    mem_unmap(p->base, total);
    p->base = NULL;
    p->free_list = NULL;
    p->count = 0;
}

void pool_reset(pool_t *p)
{
    build_free_list(p);
}

/* ---------- vtable -------------------------------------------------------- */

static void *pool_alloc(void *ctx, size_t size, size_t align)
{
    pool_t *p = (pool_t *)ctx;
    (void)align;
    if (size > p->slot_size || !p->free_list)
        return NULL;
    pool_free_node_t *node = p->free_list;
    p->free_list = node->next;
    ++p->count;
    return (void *)node;
}

static void *pool_realloc(
    void *ctx, void *ptr, size_t old_size, size_t new_size, size_t align)
{
    if (!ptr)
        return pool_alloc(ctx, new_size, align);
    pool_t *p = (pool_t *)ctx;
    (void)old_size;
    (void)align;
    /* In-place as long as the new size still fits within the slot. */
    if (new_size <= p->slot_size)
        return ptr;
    return NULL; /* pools do not cross size boundaries */
}

static void pool_free(void *ctx, void *ptr, size_t size)
{
    if (!ptr)
        return;
    pool_t *p = (pool_t *)ctx;
    (void)size;
    pool_free_node_t *node = (pool_free_node_t *)ptr;
    node->next = p->free_list;
    p->free_list = node;
    --p->count;
}

static const allocator_vtable_t pool_vtable = {
    .alloc = pool_alloc,
    .realloc = pool_realloc,
    .free = pool_free,
};

allocator_t pool_allocator(pool_t *p)
{
    return (allocator_t){.vt = &pool_vtable, .ctx = p};
}

arena_stats_t pool_stats(const pool_t *p)
{
    return (arena_stats_t){
        .used = p->count * p->slot_size,
        .capacity = p->capacity * p->slot_size,
        .committed = p->capacity * p->slot_size,
        .reserved = p->capacity * p->slot_size,
    };
}

allocator_t pool_allocator_new(pool_t *p, size_t object_size, size_t capacity)
{
    if (pool_init(p, object_size, capacity) != 0)
        return ALLOCATOR_NULL;
    return pool_allocator(p);
}
