// Project-owned wrapper around DSPark's per-thread flush-to-zero guard.
//
// Recursive tails (filters, delay feedback, reverb) decay into the denormal
// range and can cost orders of magnitude more CPU there. The guard is installed
// once per block around the DSP work, never allocated and never shared across
// threads.
#pragma once

#include <dspark/Core/DenormalGuard.h>

namespace drumas::core {

class [[nodiscard]] DenormalGuard {
public:
    DenormalGuard() noexcept = default;

    DenormalGuard(const DenormalGuard&) = delete;
    DenormalGuard& operator=(const DenormalGuard&) = delete;
    DenormalGuard(DenormalGuard&&) = delete;
    DenormalGuard& operator=(DenormalGuard&&) = delete;

    [[nodiscard]] static constexpr bool isActive() noexcept {
        return dspark::DenormalGuard::isActive();
    }

private:
    dspark::DenormalGuard guard_;
};

}  // namespace drumas::core
