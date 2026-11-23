// Helpers that exist only to make the audio callback correct.
//
// The rules the callback must follow are documented in docs/rt-safety.md. This
// header provides the one runtime ingredient that has to be applied per thread:
// denormal flushing.
#pragma once

#include <dspark/Core/DenormalGuard.h>

namespace drumas::core {

/// Flushes denormals to zero (FTZ/DAZ) on the calling thread while it is alive.
///
/// Construct it as the first statement of the audio callback and keep it alive
/// for the whole callback: a discarded temporary protects nothing (the type is
/// [[nodiscard]] so the compiler warns about the mistake).
///
/// RT-SAFE: writes one CPU control register, no allocation, no locks, no
/// syscall. Cheap enough to build per callback; it also restores the previous
/// floating-point mode on destruction.
class [[nodiscard]] ScopedDenormalGuard {
public:
    ScopedDenormalGuard() = default;
    ~ScopedDenormalGuard() = default;

    ScopedDenormalGuard(const ScopedDenormalGuard&) = delete;
    ScopedDenormalGuard& operator=(const ScopedDenormalGuard&) = delete;
    ScopedDenormalGuard(ScopedDenormalGuard&&) = delete;
    ScopedDenormalGuard& operator=(ScopedDenormalGuard&&) = delete;

private:
    dspark::DenormalGuard guard_;
};

}  // namespace drumas::core
