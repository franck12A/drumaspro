// Resonant low-pass filter for one voice, one instance covering both channels.
//
// Decision: DSPark's StateVariableFilter rather than LadderFilter (see
// docs/adr/0006-filter.md). The SVF is a TPT/Zavalishin topology designed for
// per-sample cutoff modulation, stays numerically stable at any cutoff and is
// explicitly documented as "setters from the audio thread"; the ladder's
// setters are atomic and aimed at a control-thread writer, its passband gain
// falls with resonance and resonance 1 is marginal without drive. The voice
// modulates cutoff every sample from the filter envelope and the LFO, so the
// SVF is the right tool.
//
// Both stereo channels share the coefficients and keep independent integrator
// state, which is why one instance serves L and R instead of two filters
// recomputing the same coefficients twice.
//
// Thread contract: prepare()/reset() on the setup thread (or audio thread after
// stopping the stream); setCutoff()/setResonance()/processStereo() on the audio
// thread only. RT-SAFE after prepare(): no allocation, no locks.
#pragma once

#include <algorithm>
#include <cmath>

#include <dspark/Core/AudioSpec.h>
#include <dspark/Core/StateVariableFilter.h>

namespace drumas::synth {

class ResonantLowPassFilter {
public:
    static constexpr int kChannels = 2;
    static constexpr float kMinCutoffHz = 20.0F;
    static constexpr float kMaxCutoffHz = 20000.0F;
    /// Length of the glide applied to a large cutoff jump (5 ms at 48 kHz).
    static constexpr int kRampSamples = 240;
    /// A setCutoff() call within this many processStereo() calls counts as
    /// continuous modulation (the voice calls it every sample).
    static constexpr int kModulationWindow = 32;

    /// Setup thread. Allocates nothing; the SVF is a fixed-size value type.
    void prepare(double sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        dspark::AudioSpec spec;
        spec.sampleRate = sampleRate_;
        spec.maxBlockSize = 512;
        spec.numChannels = kChannels;
        impl_.prepare(spec);
        impl_.setMode(dspark::StateVariableFilter<float>::Mode::LowPass);
        impl_.setCutoff(targetCutoffHz_);
        impl_.setResonance(resonance_);
        reset();
    }

    /// Audio thread (or setup thread before the stream starts).
    void reset() noexcept {
        impl_.reset();
        currentCutoffHz_ = targetCutoffHz_;
        rampRemaining_ = 0;
        samplesSinceCutoff_ = kModulationWindow + 1;
        impl_.setCutoff(currentCutoffHz_);
    }

    /// Audio thread. Non-finite values are ignored; stored in real Hz and
    /// clamped to the filter's valid range.
    ///
    /// A small request (continuous envelope/LFO modulation, or a UI nudge) is
    /// applied immediately, so the modulation path has no extra lag. A large
    /// jump (patch recall, a snapped control) is spread over kRampSamples, so
    /// the coefficients never change in one sample and the output does not
    /// step.
    void setCutoff(float hz) noexcept {
        if (!std::isfinite(hz)) {
            return;
        }
        targetCutoffHz_ = std::clamp(hz, kMinCutoffHz, maxCutoffHz());

        // A caller that updates the cutoff every sample (the voice's envelope
        // and LFO) is modulation, not a jump: apply it immediately so the
        // modulation keeps its shape. A caller that was silent for a while and
        // then asks for a very different cutoff is recalling a parameter: glide
        // it, because changing the coefficients in one sample would step the
        // output.
        if (samplesSinceCutoff_ <= kModulationWindow) {
            rampRemaining_ = 0;
            currentCutoffHz_ = targetCutoffHz_;
            impl_.setCutoff(currentCutoffHz_);
        } else {
            const float threshold = std::max(5.0F, currentCutoffHz_ * 0.05F);
            if (std::fabs(targetCutoffHz_ - currentCutoffHz_) > threshold) {
                rampRemaining_ = kRampSamples;
                rampIncrement_ =
                    (targetCutoffHz_ - currentCutoffHz_) / static_cast<float>(kRampSamples);
            } else {
                rampRemaining_ = 0;
                currentCutoffHz_ = targetCutoffHz_;
                impl_.setCutoff(currentCutoffHz_);
            }
        }
        samplesSinceCutoff_ = 0;
    }

    /// Audio thread. 0 = critically damped, 1 = Q of 50 (strong resonance).
    /// Non-finite values are ignored; the filter clamps to [0, 1].
    void setResonance(float resonance) noexcept {
        if (!std::isfinite(resonance)) {
            return;
        }
        resonance_ = resonance;
        impl_.setResonance(resonance);
    }

    /// Audio thread. One sample through both channels. RT-SAFE.
    void processStereo(float& left, float& right) noexcept {
        if (samplesSinceCutoff_ <= kModulationWindow) {
            ++samplesSinceCutoff_;
        }
        if (rampRemaining_ > 0) {
            currentCutoffHz_ += rampIncrement_;
            if (--rampRemaining_ == 0) {
                currentCutoffHz_ = targetCutoffHz_;
            }
            impl_.setCutoff(currentCutoffHz_);
        }
        left = impl_.processSample(left, 0);
        right = impl_.processSample(right, 1);
    }

    /// Target cutoff in Hz (the value a caller last requested).
    [[nodiscard]] float cutoff() const noexcept { return targetCutoffHz_; }
    [[nodiscard]] float resonance() const noexcept { return impl_.getResonance(); }

private:
    [[nodiscard]] float maxCutoffHz() const noexcept {
        return static_cast<float>(sampleRate_ * 0.499);
    }

    dspark::StateVariableFilter<float> impl_;
    double sampleRate_ = 48000.0;
    float targetCutoffHz_ = 1000.0F;
    float currentCutoffHz_ = 1000.0F;
    float rampIncrement_ = 0.0F;
    int rampRemaining_ = 0;
    int samplesSinceCutoff_ = kModulationWindow + 1;
    float resonance_ = 0.0F;
};

}  // namespace drumas::synth
