// Lock-free single-producer / single-consumer queue: the only channel from the
// control thread (UI, keyboard, tests) into the audio thread.
//
// This is a thin, zero-overhead wrapper on top of dspark::SpscQueue. The wrapper
// exists so that the rest of the codebase never names a DSPark type directly:
// replacing the queue means rewriting this file, nothing else. See
// docs/adr/0003-control-to-audio-command-queue.md for the reasoning and
// third_party/README.md for the upstream thread contract this relies on.
//
// Thread contract (enforced by usage, not by the type system):
//   - exactly ONE thread calls push()      (producer: control/UI thread)
//   - exactly ONE thread calls pop()       (consumer: audio thread)
//   - sizeApprox()/empty() are diagnostics of either thread, never control flow
//   - push()/pop() are wait-free, lock-free, allocation-free, and do not throw.
//
// Real-time safety: push() and pop() are RT-SAFE. Construction/destruction are
// setup-time only.
#pragma once

#include <cstddef>
#include <type_traits>

#include <dspark/Core/SpscQueue.h>

namespace drumas::core {

/// Fixed-capacity bounded queue. `Capacity` must be a power of two and one slot
/// is reserved internally, so the usable capacity is `Capacity - 1`.
template<typename T, std::size_t Capacity>
class SpscQueue {
    static_assert(Capacity >= 2, "SpscQueue: capacity must be at least 2");
    static_assert((Capacity & (Capacity - 1)) == 0, "SpscQueue: capacity must be a power of two");
    static_assert(std::is_trivially_copyable_v<T>,
                  "SpscQueue: elements must be trivially copyable; a non-trivial element would "
                  "make the hand-off a data race");

public:
    using value_type = T;

    SpscQueue() = default;
    SpscQueue(const SpscQueue&) = delete;
    SpscQueue& operator=(const SpscQueue&) = delete;
    SpscQueue(SpscQueue&&) = delete;
    SpscQueue& operator=(SpscQueue&&) = delete;

    /// Producer side. Returns false when the queue is full; the item is dropped
    /// and the caller decides what to do (for note events, dropping is silent).
    [[nodiscard]] bool push(const T& item) noexcept { return impl_.push(item); }

    /// Consumer side. Returns false when the queue is empty.
    [[nodiscard]] bool pop(T& item) noexcept { return impl_.pop(item); }

    /// Diagnostics only: the value is stale the moment it is read.
    [[nodiscard]] std::size_t sizeApprox() const noexcept { return impl_.sizeApprox(); }

    /// Diagnostics only.
    [[nodiscard]] bool empty() const noexcept { return impl_.empty(); }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept { return impl_capacity(); }

private:
    static constexpr std::size_t impl_capacity() noexcept { return Capacity - 1; }

    dspark::SpscQueue<T, Capacity> impl_;
};

}  // namespace drumas::core
