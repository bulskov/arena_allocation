#include "platform.h"

/* ========================================================================= */
/* Windows                                                                    */
/* ========================================================================= */
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void *mem_map(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
}

void mem_unmap(void *ptr, size_t size)
{
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
}

void *mem_reserve(size_t size)
{
    return VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
}

void mem_release(void *ptr, size_t size)
{
    (void)size;
    VirtualFree(ptr, 0, MEM_RELEASE);
}

int mem_commit(void *ptr, size_t size)
{
    return VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE) ? 0 : -1;
}

void mem_decommit(void *ptr, size_t size)
{
    VirtualFree(ptr, size, MEM_DECOMMIT);
}

size_t mem_page_size(void)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return (size_t)si.dwPageSize;
}

/* ========================================================================= */
/* POSIX (Linux / macOS / BSDs)                                               */
/* ========================================================================= */
#else

#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

void *mem_map(size_t size)
{
    void *p = mmap(
        NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

void mem_unmap(void *ptr, size_t size)
{
    munmap(ptr, size);
}

void *mem_reserve(size_t size)
{
    void *p = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return (p == MAP_FAILED) ? NULL : p;
}

void mem_release(void *ptr, size_t size)
{
    munmap(ptr, size);
}

int mem_commit(void *ptr, size_t size)
{
    return mprotect(ptr, size, PROT_READ | PROT_WRITE);
}

void mem_decommit(void *ptr, size_t size)
{
    mprotect(ptr, size, PROT_NONE);
    madvise(ptr, size, MADV_DONTNEED);
}

size_t mem_page_size(void)
{
    return (size_t)sysconf(_SC_PAGESIZE);
}

#endif /* _WIN32 */
