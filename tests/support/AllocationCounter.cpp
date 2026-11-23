// Counting replacements for the global allocation functions.
//
// Every allocation in the test binary goes through these, so a test can snapshot
// the counter, run one audio block, and assert that nothing was allocated. The
// counter itself is a relaxed atomic and never allocates.
#include "support/AllocationCounter.h"

#include <atomic>
#include <cstdlib>
#include <malloc.h>
#include <new>

namespace {
std::atomic<std::size_t> g_allocations{0};
std::atomic<std::size_t> g_deallocations{0};
}  // namespace

namespace drumas::test {

std::size_t allocationCount() noexcept {
    return g_allocations.load(std::memory_order_relaxed);
}

std::size_t deallocationCount() noexcept {
    return g_deallocations.load(std::memory_order_relaxed);
}

void resetAllocationCount() noexcept {
    g_allocations.store(0, std::memory_order_relaxed);
    g_deallocations.store(0, std::memory_order_relaxed);
}

}  // namespace drumas::test

void* operator new(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size == 0 ? 1 : size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    return std::malloc(size == 0 ? 1 : size);
}

void* operator new(std::size_t size, std::align_val_t alignment) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = _aligned_malloc(size == 0 ? 1 : size, static_cast<std::size_t>(alignment))) {
        return memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t size, std::align_val_t alignment) {
    return ::operator new(size, alignment);
}

void* operator new(std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    return _aligned_malloc(size == 0 ? 1 : size, static_cast<std::size_t>(alignment));
}

void* operator new[](std::size_t size, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    return ::operator new(size, alignment, std::nothrow);
}

void operator delete(void* memory) noexcept {
    if (memory != nullptr) {
        g_deallocations.fetch_add(1, std::memory_order_relaxed);
        std::free(memory);
    }
}

void operator delete[](void* memory) noexcept {
    ::operator delete(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    ::operator delete(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    ::operator delete(memory);
}

void operator delete(void* memory, const std::nothrow_t&) noexcept {
    ::operator delete(memory);
}

void operator delete[](void* memory, const std::nothrow_t&) noexcept {
    ::operator delete(memory);
}

void operator delete(void* memory, std::align_val_t alignment) noexcept {
    if (memory != nullptr) {
        g_deallocations.fetch_add(1, std::memory_order_relaxed);
        _aligned_free(memory);
    }
    (void)alignment;
}

void operator delete[](void* memory, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}

void operator delete(void* memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}

void operator delete[](void* memory, std::size_t, std::align_val_t alignment) noexcept {
    ::operator delete(memory, alignment);
}

void operator delete(void* memory, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    ::operator delete(memory, alignment);
}

void operator delete[](void* memory, std::align_val_t alignment, const std::nothrow_t&) noexcept {
    ::operator delete(memory, alignment);
}
