// One sine voice: oscillator + ADSR + smoothed gain, rendered additively into a
// mono mix bus.
//
// Real-time safety: prepare() allocates nothing for this class (DSPark's
// oscillator builds its minBLEP table during prepare(), which is why prepare()
// must run on the setup thread); renderAdd() only does per-sample arithmetic.
#pragma once

#include <algorithm>
#include <cstdint>

#include "core/AudioTypes.h"
#include "core/SmoothedValue.h"
#include "synth/AdsrEnvelope.h"
#include "synth/SineOscillator.h"

namespace drumas::synth {

class SineVoice {
public:
    /// Setup thread.
    void prepare(double sampleRate) noexcept {
        sampleRate_ = sampleRate;
        oscillator_.prepare(sampleRate);
        envelope_.prepare(sampleRate);
        gain_.prepare(sampleRate);
        reset();
    }

    /// Setup thread (or audio thread after stopping the stream).
    void reset() noexcept {
        oscillator_.reset();
        envelope_.reset();
        gain_.reset(0.0F);
        note_ = 0;
        age_ = 0;
        velocity_ = 0.0F;
        held_ = false;
    }

    /// Audio thread. Starts (or retriggers) the voice. The oscillator phase is
    /// deliberately not reset: a constant-phase sine has no value
    /// discontinuity, so a stolen voice glides to the new pitch instead of
    /// clicking.
    void noteOn(std::uint8_t note, float velocity, std::uint64_t age) noexcept {
        note_ = note;
        age_ = age;
        velocity_ = std::clamp(velocity, 0.0F, 1.0F);
        held_ = true;
        oscillator_.setFrequency(core::noteToFrequency(note));
        // Ramp instead of snap: the envelope continues from its current level on
        // a retrigger, so a hard gain step would be audible.
        gain_.setTargetValue(velocity_);
        envelope_.noteOn();
    }

    /// Audio thread.
    void noteOff() noexcept {
        held_ = false;
        envelope_.noteOff();
    }

    /// Audio thread. Retunes the voice (used by pitch modulation later).
    void setFrequency(float hz) noexcept { oscillator_.setFrequency(hz); }

    void setEnvelopeParameters(const AdsrParameters& parameters) noexcept {
        envelope_.setParameters(parameters);
    }

    /// Audio thread. Adds this voice into `mono` (never overwrites).
    void renderAdd(float* mono, int numFrames) noexcept {
        if (!isActive()) {
            return;
        }
        for (int i = 0; i < numFrames; ++i) {
            const float envelopeValue = envelope_.nextValue();
            const float gain = gain_.getNextValue();
            mono[i] += oscillator_.nextSample() * envelopeValue * gain;
        }
    }

    /// A voice is active while its envelope is not idle, including the tail
    /// after note-off.
    [[nodiscard]] bool isActive() const noexcept { return envelope_.isActive() || held_; }

    /// Free means "no note held and no tail playing": safe to reuse silently.
    [[nodiscard]] bool isFree() const noexcept { return !isActive(); }

    [[nodiscard]] std::uint8_t note() const noexcept { return note_; }
    [[nodiscard]] bool isHeld() const noexcept { return held_; }
    [[nodiscard]] float velocity() const noexcept { return velocity_; }
    [[nodiscard]] float envelopeLevel() const noexcept { return envelope_.level(); }
    [[nodiscard]] EnvelopeStage envelopeStage() const noexcept { return envelope_.stage(); }
    [[nodiscard]] std::uint64_t age() const noexcept { return age_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }

private:
    SineOscillator oscillator_;
    AdsrEnvelope envelope_;
    core::SmoothedValue gain_;
    std::uint8_t note_ = 0;
    float velocity_ = 0.0F;
    std::uint64_t age_ = 0;
    bool held_ = false;
    double sampleRate_ = core::kDefaultSampleRate;
};

}  // namespace drumas::synth
