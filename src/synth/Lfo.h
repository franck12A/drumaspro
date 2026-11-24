// One global LFO for the whole synth.
//
// Decision (docs/adr/0007-patch-switching-and-live-parameters.md): a global LFO
// instead of one per voice. All voices share phase, which is musically coherent
// for vibrato and cutoff sweeps, and the oscillator runs once per sample for the
// entire pool instead of once per voice (32x cheaper in the worst case). The
// cost of that choice is no per-voice analog drift; if a patch needs it, a
// per-voice phase offset can be added without changing this shape.
//
// The LFO is not band-limited on purpose: it runs at a few hertz, where a naive
// square or saw has no audible aliasing. A square's cutoff jumps are absorbed
// by the filter's cutoff ramp.
//
// Thread contract: prepare()/setRate()/setShape() on the setup thread or the
// audio thread before process; nextValue() on the audio thread. RT-SAFE.
#pragma once

#include <algorithm>
#include <cmath>

#include <dspark/Core/DspMath.h>

#include "synth/Patch.h"

namespace drumas::synth {

class Lfo {
public:
    static constexpr float kMinRateHz = 0.05F;
    static constexpr float kMaxRateHz = 20.0F;
    static constexpr float kTwoPi = 6.283185307179586F;

    void prepare(double sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        setRate(rateHz_);
        reset();
    }

    void reset() noexcept { phase_ = 0.0F; }

    void setRate(float hz) noexcept {
        if (!std::isfinite(hz)) {
            return;
        }
        rateHz_ = std::clamp(hz, kMinRateHz, kMaxRateHz);
        phaseIncrement_ = rateHz_ / static_cast<float>(sampleRate_);
    }

    void setShape(LfoShape shape) noexcept { shape_ = shape; }

    [[nodiscard]] float rate() const noexcept { return rateHz_; }
    [[nodiscard]] LfoShape shape() const noexcept { return shape_; }
    [[nodiscard]] float phase() const noexcept { return phase_; }

    /// Audio thread. Value in [-1, 1], then advances one sample. RT-SAFE.
    [[nodiscard]] float nextValue() noexcept {
        float value = 0.0F;
        switch (shape_) {
            case LfoShape::Sine:
                value = dspark::fastSin(phase_ * kTwoPi);
                break;
            case LfoShape::Triangle:
                // Starts at -1, peaks at phase 0.5, back to -1.
                value = 1.0F - 4.0F * std::fabs(phase_ - 0.5F);
                break;
            case LfoShape::Saw:
                value = 2.0F * phase_ - 1.0F;
                break;
            case LfoShape::Square:
                value = phase_ < 0.5F ? 1.0F : -1.0F;
                break;
        }
        phase_ += phaseIncrement_;
        if (phase_ >= 1.0F) {
            phase_ -= 1.0F;
        }
        return value;
    }

private:
    double sampleRate_ = 48000.0;
    float rateHz_ = 5.0F;
    float phase_ = 0.0F;
    float phaseIncrement_ = 0.0F;
    LfoShape shape_ = LfoShape::Sine;
};

}  // namespace drumas::synth
