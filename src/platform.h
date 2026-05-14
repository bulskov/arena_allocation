#ifndef ARENA_PLATFORM_H
#define ARENA_PLATFORM_H

#include <stddef.h>

/*
 * Minimal OS virtual-memory abstraction.
 * Internal to the library — not part of the public API.
 *
 * mem_map / mem_unmap
 *   Allocate / release committed, read-write anonymous pages.
 *   Returned pointer is page-aligned. Returns NULL on failure.
 *
 * mem_reserve / mem_release
 *   Reserve / release a virtual address range with no physical backing.
 *   Reserved memory faults on access until committed.
 *
 * mem_commit
 *   Back [ptr, ptr+size) with physical pages (R/W).
 *   ptr must be within a reserved range and page-aligned.
 *   Returns 0 on success, -1 on failure.
 *
 * mem_decommit
 *   Release physical pages for [ptr, ptr+size) but keep the VA reservation.
 *   Accessing decommitted memory is undefined behaviour.
 *
 * mem_page_size
 *   System page size in bytes. Always a power of two.
 */

void *mem_map(size_t size);
void mem_unmap(void *ptr, size_t size);

void *mem_reserve(size_t size);
void mem_release(void *ptr, size_t size);

int mem_commit(void *ptr, size_t size);
void mem_decommit(void *ptr, size_t size);

size_t mem_page_size(void);

#endif /* ARENA_PLATFORM_H */
