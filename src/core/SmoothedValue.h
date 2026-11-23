// Ramp-based smoothing for continuous parameters (gain, cutoff, pitch...).
//
// A parameter that jumps between two values produces a click. Every continuous
// parameter that reaches a DSP stage in this project therefore has to travel
// through a ramp: cheap, predictable, and it lands exactly on the target.
//
// The implementation is a thin wrapper on dspark::SmoothedValue configured for
// linear ramps. Wrapping it keeps DSPark out of our public API and lets us swap
// in our own smoother without touching call sites.
//
// Thread contract: construct and prepare() on the setup thread; setTargetValue()
// and getNextValue() from the audio thread only (no atomics inside).
#pragma once

#include <dspark/Core/SmoothedValue.h>

#include "core/AudioTypes.h"

namespace drumas::core {

class SmoothedValue {
public:
    /// Ramp length used when the caller does not care. ~10 ms is short enough to
    /// feel immediate and long enough to remove clicks at 48 kHz.
    static constexpr double kDefaultRampMs = 10.0;

    SmoothedValue() { impl_.prepare(static_cast<double>(kDefaultSampleRate), kDefaultRampMs); }

    SmoothedValue(const SmoothedValue&) = delete;
    SmoothedValue& operator=(const SmoothedValue&) = delete;
    SmoothedValue(SmoothedValue&&) = delete;
    SmoothedValue& operator=(SmoothedValue&&) = delete;

    /// Setup thread. Recomputes the per-sample step. RT-SAFE (no allocation),
    /// but must not run concurrently with the audio thread.
    void prepare(double sampleRate, double rampTimeMs = kDefaultRampMs) noexcept {
        impl_.prepare(sampleRate, rampTimeMs);
        impl_.setSmoothingType(dspark::SmoothedValue<float>::SmoothingType::Linear);
        impl_.reset(0.0F);
    }

    /// Audio thread. Starts a ramp towards `value`.
    void setTargetValue(float value) noexcept { impl_.setTargetValue(value); }

    /// Audio thread. Advances one sample and returns the new value. RT-SAFE.
    [[nodiscard]] float getNextValue() noexcept { return impl_.getNextValue(); }

    /// Audio thread readout, does not advance the ramp.
    [[nodiscard]] float current() const noexcept { return impl_.getCurrentValue(); }

    [[nodiscard]] float target() const noexcept { return impl_.getTargetValue(); }

    [[nodiscard]] bool isSmoothing() const noexcept { return impl_.isSmoothing(); }

    /// Jumps straight to the target. Setup thread, or audio thread when starting
    /// from a known silent state.
    void snapToTarget() noexcept { impl_.skip(); }

    /// Setup thread. Current and target become `value`.
    void reset(float value) noexcept { impl_.reset(value); }

private:
    dspark::SmoothedValue<float> impl_;
};

}  // namespace drumas::core
