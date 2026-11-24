// Master-stage soft clip: the last thing between the voice mix and the device.
//
// Stage 3 left a hard clip here (`std::clamp(x, -1, 1)`). A pad with unison can
// sum well above full scale, and hard clipping turns every peak into a corner:
// harsh odd harmonics and intermodulation right where a chord should stay
// clean. This stage replaces it with a stateless soft clip that is exactly
// transparent below the knee and asymptotic to full scale above it, so the
// output can never exceed [-1, 1] while the waveform keeps its curvature.
//
// It is deliberately not a look-ahead limiter: that needs a delay line, an
// envelope follower and a release curve, and belongs with the effects stage
// (IEffect, stage 5) where it can be measured against the rest of the chain.
// The trade-off is documented in docs/adr/0005-master-soft-clip.md.
//
// Real-time safety: one branch below the knee, one tanh above it, no state,
// no allocation, no locks. RT-SAFE.
#pragma once

#include <cmath>

namespace drumas::synth {

class MasterLimiter {
public:
    /// Below this absolute value the signal passes through untouched. Chosen
    /// to leave normal chords (crest factor ~4-6 dB) untouched while still
    /// catching the unison peaks that used to hit the hard clip.
    static constexpr float kKnee = 0.7F;

    /// Hard guarantee: the output magnitude is always below this value.
    static constexpr float kCeiling = 1.0F;

    /// Audio thread. RT-SAFE.
    [[nodiscard]] static float processSample(float input) noexcept {
        if (!std::isfinite(input)) {
            // A non-finite sample is a bug upstream; silence is the only safe
            // thing to put on the device and keeps the error contained.
            return 0.0F;
        }
        const float magnitude = std::fabs(input);
        if (magnitude <= kKnee) {
            return input;
        }
        // tanh tail: C1-continuous at the knee (tanh'(0) == 1), monotonic and
        // asymptotic to 1. The range kKnee..1 maps onto kKnee..1.
        const float over = (magnitude - kKnee) / (kCeiling - kKnee);
        const float shaped = kKnee + (kCeiling - kKnee) * std::tanh(over);
        return input < 0.0F ? -shaped : shaped;
    }

    /// Audio thread. Convenience for a block of mono samples, in place.
    static void processBlock(float* samples, int numSamples) noexcept {
        for (int index = 0; index < numSamples; ++index) {
            samples[index] = processSample(samples[index]);
        }
    }
};

}  // namespace drumas::synth
