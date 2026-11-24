// Last-resort static clip after the look-ahead limiter.
//
// It is intentionally almost transparent: the limiter's ceiling is below the
// knee, so in normal operation this stage never touches a sample. It exists to
// contain a NaN/Inf or a coding error before the device, exactly as the stage-4
// soft clip did.
#pragma once

#include <cmath>

namespace drumas::fx {

class SafetyClip {
public:
    static constexpr float kKnee = 0.95F;
    static constexpr float kCeiling = 1.0F;

    [[nodiscard]] static float processSample(float input) noexcept {
        if (!std::isfinite(input)) return 0.0F;
        const float magnitude = std::fabs(input);
        if (magnitude <= kKnee) return input;
        const float over = (magnitude - kKnee) / (kCeiling - kKnee);
        const float shaped = kKnee + (kCeiling - kKnee) * std::tanh(over);
        return input < 0.0F ? -shaped : shaped;
    }
};

}  // namespace drumas::fx
