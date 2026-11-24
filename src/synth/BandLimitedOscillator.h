// Band-limited oscillator with minBLEP correction.
//
// Why not DSPark's Oscillator: it is a correct PolyBLEP implementation, but its
// square is permanently fixed at 50 % duty and it has no pulse width, which is
// one of the waveforms the synth must offer. Its WavetableOscillator has a
// better alias floor but stores a full mipmapped table per instance (built with
// a DFT at prepare time); 8 unison oscillators per voice would mean tens of
// megabytes and a heavy setup, and a pulse width would need a rebuilt table.
//
// This oscillator keeps a few hundred bytes of state per instance and corrects
// every discontinuity with DSPark's shared minimum-phase band-limited step
// table (MinBlepTable): the kernel is causal and ~-90 dB of alias rejection
// (docs/adr/0004-band-limited-oscillators.md has the measured figures). A
// 2-point PolyBLEP kernel was tried first and measured at only about -30 dBc at
// 1 kHz, which is why it was replaced.
//
// Waveforms:
//   - Sine: degree-9 minimax approximation (dspark::fastSin, ~3e-6 max error).
//   - Saw: ramp whose wrap is corrected with minBLEP.
//   - Square/pulse: difference of two band-limited saws, one shifted by the
//     pulse width; zero DC at every width.
//   - Triangle: leaky integration of the band-limited square, normalized to
//     the steady-state peak for the current frequency.
//
// Real-time safety: prepare() builds the shared table (control thread, once);
// every other method is per-sample arithmetic. No allocation, no locks.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include <dspark/Core/DspMath.h>
#include <dspark/Core/MinBlepTable.h>

#include "synth/IOscillator.h"

namespace drumas::synth {

class BandLimitedOscillator final : public IOscillator {
public:
    /// Pulse width is kept away from 0 and 1: a vanishing pulse has no
    /// fundamental left and its band-limited difference of saws overshoots.
    static constexpr float kMinPulseWidth = 0.05F;
    static constexpr float kMaxPulseWidth = 0.95F;
    static constexpr float kTwoPi = 6.283185307179586F;

    using BlepTable = dspark::MinBlepTable<float>;
    static constexpr int kBlepTaps = BlepTable::kTaps;
    static constexpr int kBlepMask = kBlepTaps - 1;

    void prepare(double sampleRate) noexcept override {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        // Build the shared table here, on the setup thread: it does FFTs and
        // temporary heap allocations, which must never happen in the callback.
        blepTable_ = &BlepTable::instance();
        updatePhaseIncrement();
        updateTriangleNormalization();
        reset();
    }

    void reset() noexcept override {
        phase_ = 0.0F;
        triangleState_ = -triangleExpectedPeak_;
        corr_.fill(0.0F);
        corrHead_ = 0;
        dcInput_ = 0.0F;
        dcOutput_ = 0.0F;
    }

    void setFrequency(float hz) noexcept override {
        if (!std::isfinite(hz)) {
            return;
        }
        frequency_ = std::clamp(hz, 0.0F, static_cast<float>(sampleRate_) * 0.5F);
        updatePhaseIncrement();
        updateTriangleNormalization();
    }

    /// Audio thread. Applies a pitch ratio on top of `hz` without recomputing
    /// the triangle normalization: vibrato changes the increment every sample
    /// and the cached normalization stays correct to within the tiny amplitude
    /// error of a bend below a semitone. Used by the voice's LFO vibrato path.
    void setFrequencyModulated(float hz, float ratio) noexcept {
        if (!std::isfinite(hz) || !std::isfinite(ratio)) {
            return;
        }
        const float scaled = hz * ratio;
        const float clamped = std::clamp(scaled, 0.0F, static_cast<float>(sampleRate_) * 0.5F);
        phaseIncrement_ = std::clamp(clamped / static_cast<float>(sampleRate_), 0.0F, 0.5F);
    }

    void setWaveform(Waveform waveform) noexcept override { waveform_ = waveform; }

    void setPulseWidth(float width) noexcept override {
        if (!std::isfinite(width)) {
            return;
        }
        pulseWidth_ = std::clamp(width, kMinPulseWidth, kMaxPulseWidth);
    }

    void setPhase(float phase) noexcept override {
        if (!std::isfinite(phase)) {
            return;
        }
        phase_ = phase - std::floor(phase);
        if (phase_ >= 1.0F) {
            phase_ -= 1.0F;
        }
        // Seed the integrator at its steady-state value so a triangle starts at
        // the waveform point instead of easing in over a cycle.
        const float ideal = phase_ < 0.5F ? (4.0F * phase_ - 1.0F) : (3.0F - 4.0F * phase_);
        triangleState_ = static_cast<double>(ideal * triangleExpectedPeak_);
    }

    [[nodiscard]] float nextSample() noexcept override {
        const float phaseBefore = phase_;
        float raw = 0.0F;

        switch (waveform_) {
            case Waveform::Sine:
                raw = dspark::fastSin(phase_ * kTwoPi);
                break;

            case Waveform::Saw:
                raw = 2.0F * phase_ - 1.0F;
                break;

            case Waveform::Square: {
                // Pulse = saw(phase - width) - saw(phase). At 0.5 the result is
                // a square of +1/-1; other widths give a zero-DC pulse whose
                // peak grows as 2*(1-width) or 2*width.
                float shifted = phase_ - pulseWidth_;
                if (shifted < 0.0F) {
                    shifted += 1.0F;
                }
                raw = (2.0F * shifted - 1.0F) - (2.0F * phase_ - 1.0F);
                break;
            }

            case Waveform::Triangle:
                // Band-limited square; its integrator has unity DC gain, so the
                // square must be the DC-free 50 % one.
                raw = phase_ < 0.5F ? 1.0F : -1.0F;
                break;
        }

        // Emit the raw value plus the correction queued for this sample, then
        // advance the ring: the head now points at the next sample, which is
        // where events detected below will be scheduled.
        const float corrected = raw + corr_[static_cast<std::size_t>(corrHead_)];
        corr_[static_cast<std::size_t>(corrHead_)] = 0.0F;
        corrHead_ = (corrHead_ + 1) & kBlepMask;

        float output = corrected;
        if (waveform_ == Waveform::Triangle) {
            const double increment = static_cast<double>(phaseIncrement_);
            triangleState_ =
                increment * static_cast<double>(corrected) +
                (1.0 - increment) * triangleState_;
            output = static_cast<float>(triangleState_) * triangleNormalization_;
        }

        advancePhaseAndSchedule(phaseBefore);

        // The minBLEP residual has a small non-zero area, so periodizing the
        // correction train leaves a DC offset proportional to the event rate
        // (measured: +0.57 for a 7 kHz saw without this stage). Oscillators are
        // meant to be DC-free, so remove it with a one-pole high-pass well
        // below the audio band. It also removes the intentional DC of a
        // narrow pulse, which is what the patch mix expects.
        output = removeDc(output);
        return output;
    }

    [[nodiscard]] Waveform waveform() const noexcept { return waveform_; }
    [[nodiscard]] float frequency() const noexcept { return frequency_; }
    [[nodiscard]] float pulseWidth() const noexcept { return pulseWidth_; }
    [[nodiscard]] float phase() const noexcept { return phase_; }

private:
    /// Advances the phase and schedules a minBLEP correction for every
    /// discontinuity that fell inside this sample interval.
    void advancePhaseAndSchedule(float phaseBefore) noexcept {
        if (phaseIncrement_ <= 1.0e-10F) {
            return;
        }
        phase_ += phaseIncrement_;
        const float phaseAfterUnwrapped = phase_;
        if (phase_ >= 1.0F) {
            phase_ -= 1.0F;
        }

        switch (waveform_) {
            case Waveform::Sine:
                break;

            case Waveform::Saw:
                // Rising ramp wraps from +1 to -1: a jump of -2.
                if (phaseAfterUnwrapped >= 1.0F) {
                    scheduleBlep(-2.0F, (phaseAfterUnwrapped - 1.0F) / phaseIncrement_);
                }
                break;

            case Waveform::Square: {
                // pulse = saw(phase - width) - saw(phase): the first saw's wrap
                // adds +2 to the pulse, the shifted saw's wrap subtracts 2.
                if (phaseAfterUnwrapped >= 1.0F) {
                    scheduleBlep(2.0F, (phaseAfterUnwrapped - 1.0F) / phaseIncrement_);
                }
                if (phaseBefore < pulseWidth_) {
                    // The shifted saw starts its cycle at phase == width.
                    if (phaseAfterUnwrapped >= pulseWidth_) {
                        scheduleBlep(-2.0F,
                                     (phaseAfterUnwrapped - pulseWidth_) / phaseIncrement_);
                    }
                } else if (phaseAfterUnwrapped >= 1.0F + pulseWidth_) {
                    // At high increments the shifted saw can wrap past 1+width
                    // inside the same interval as the first saw's wrap.
                    scheduleBlep(-2.0F, (phaseAfterUnwrapped - 1.0F - pulseWidth_) /
                                             phaseIncrement_);
                }
                break;
            }

            case Waveform::Triangle: {
                // The square falls at 0.5 (-2) and rises at the wrap (+2).
                if (phaseBefore < 0.5F && phaseAfterUnwrapped >= 0.5F) {
                    scheduleBlep(-2.0F, (phaseAfterUnwrapped - 0.5F) / phaseIncrement_);
                }
                if (phaseAfterUnwrapped >= 1.0F) {
                    scheduleBlep(2.0F, (phaseAfterUnwrapped - 1.0F) / phaseIncrement_);
                }
                break;
            }
        }
    }

    /// Queues `jump * residual(j + frac)` for j = 0..kBlepTaps-1, starting at
    /// the next output sample. `frac` is how many samples before that sample
    /// the discontinuity happened, in [0, 1).
    void scheduleBlep(float jump, float frac) noexcept {
        const BlepTable& table = *blepTable_;
        for (int tap = 0; tap < kBlepTaps; ++tap) {
            const int index = (corrHead_ + tap) & kBlepMask;
            corr_[static_cast<std::size_t>(index)] +=
                jump * table.residual(static_cast<float>(tap) + frac);
        }
    }

    /// One-pole DC blocker (~4 Hz at 48 kHz). Tiny cost per sample, no state
    /// beyond two floats, and it does not touch the audio-band response.
    [[nodiscard]] float removeDc(float input) noexcept {
        const float output = input - dcInput_ + 0.9995F * dcOutput_;
        dcInput_ = input;
        dcOutput_ = output;
        return output;
    }

    void updatePhaseIncrement() noexcept {
        phaseIncrement_ = std::clamp(frequency_ / static_cast<float>(sampleRate_), 0.0F, 0.5F);
    }

    /// Steady-state peak of the leaky integrator driven by a +-1 square, so the
    /// triangle keeps unit amplitude at every frequency. Cached by increment:
    /// a vibrato LFO calls setFrequency() often and the pow is not cheap.
    void updateTriangleNormalization() noexcept {
        if (phaseIncrement_ == triangleNormalizationIncrement_) {
            return;
        }
        triangleNormalizationIncrement_ = phaseIncrement_;
        if (phaseIncrement_ > 1.0e-7F && phaseIncrement_ < 1.0F) {
            const float leak = 1.0F - phaseIncrement_;
            const float halfPeriodSamples = 0.5F / phaseIncrement_;
            const float q = std::pow(leak, halfPeriodSamples);
            triangleExpectedPeak_ = (1.0F - q) / (1.0F + q);
            triangleNormalization_ =
                triangleExpectedPeak_ > 0.001F ? 1.0F / triangleExpectedPeak_ : 4.0F;
        } else {
            triangleExpectedPeak_ = 0.25F;
            triangleNormalization_ = 4.0F;
        }
    }

    double sampleRate_ = 48000.0;
    float frequency_ = 440.0F;
    float phase_ = 0.0F;
    float phaseIncrement_ = 0.0F;
    float pulseWidth_ = 0.5F;
    Waveform waveform_ = Waveform::Sine;

    double triangleState_ = 0.0;
    float triangleNormalization_ = 4.0F;
    float triangleExpectedPeak_ = 0.25F;
    float triangleNormalizationIncrement_ = -1.0F;

    const BlepTable* blepTable_ = nullptr;
    std::array<float, kBlepTaps> corr_{};
    int corrHead_ = 0;
    float dcInput_ = 0.0F;
    float dcOutput_ = 0.0F;
};

}  // namespace drumas::synth
