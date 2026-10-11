#include "profiling.h"
#include <cstdlib>
#include <malloc.h>
#include <new>

// Track C++ heap ownership, including STL containers, on every game thread.
// Tracy uses its own malloc allocator, so these hooks do not recurse.
// Secure events tolerate allocations before profiler startup / during teardown.
namespace {
void* allocate(std::size_t size, std::size_t alignment = 0) {
    size = size ? size : 1;
    for (;;) {
        void* pointer = alignment ? _aligned_malloc(size, alignment) : std::malloc(size);
        if (pointer) { TracySecureAllocS(pointer, size, 8); return pointer; }
        if (auto handler = std::get_new_handler()) handler();
        else throw std::bad_alloc();
    }
}
void release(void* pointer, bool aligned = false) noexcept {
    if (!pointer) return;
    TracySecureFree(pointer);
    if (aligned) _aligned_free(pointer); else std::free(pointer);
}
}
void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* p) noexcept { release(p); }
void operator delete[](void* p) noexcept { release(p); }
void operator delete(void* p, std::size_t) noexcept { release(p); }
void operator delete[](void* p, std::size_t) noexcept { release(p); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { try { return allocate(n); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, const std::nothrow_t& tag) noexcept { return ::operator new(n, tag); }
void operator delete(void* p, const std::nothrow_t&) noexcept { release(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { release(p); }
void* operator new(std::size_t n, std::align_val_t a) { return allocate(n, std::size_t(a)); }
void* operator new[](std::size_t n, std::align_val_t a) { return allocate(n, std::size_t(a)); }
void operator delete(void* p, std::align_val_t) noexcept { release(p, true); }
void operator delete[](void* p, std::align_val_t) noexcept { release(p, true); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { release(p, true); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { release(p, true); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { try { return allocate(n, std::size_t(a)); } catch (...) { return nullptr; } }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t& tag) noexcept { return ::operator new(n, a, tag); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { release(p, true); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { release(p, true); }
