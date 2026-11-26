// One subtractive voice: two band-limited oscillators with optional unison,
// a sub oscillator, white noise, a resonant low-pass, two ADSR envelopes and
// velocity response. The LFO is global and its value arrives per sample.
//
// The voice is the only architecture in the synth: pad, pluck and keys are
// patches (values), not subclasses. Adding an instrument means adding data to
// PatchBank.h.
//
// Patch capture vs live parameters (docs/adr/0007-...): noteOn() copies the
// patch, so a patch switch never changes a sounding voice. cutoff, resonance
// and the envelope times have live overrides owned by the processor: they are
// applied to every sounding voice with a ramp, and a new voice starts at the
// live value.
//
// Real-time safety: prepare() is setup; noteOn()/renderAdd() do only arithmetic
// (no allocation, no locks, no I/O). The per-note cos/sin/exp2 calls happen once
// per note-on, never per sample.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "core/AudioTypes.h"
#include "core/SmoothedValue.h"
#include "synth/AdsrEnvelope.h"
#include "synth/BandLimitedOscillator.h"
#include "synth/Patch.h"
#include "synth/ResonantLowPassFilter.h"

namespace drumas::synth {

class SynthVoice {
public:
    static constexpr int kMaxUnison = 4;

    /// Per-voice headroom: four voices at full tilt must stay below the master
    /// soft clip's knee (0.7). The patch levels are calibrated against this.
    static constexpr float kOutputGain = 0.20F;
    static constexpr float kPi = 3.14159265358979323846F;

    /// Setup thread.
    void prepare(double sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        for (BandLimitedOscillator& oscillator : osc1_) {
            oscillator.prepare(sampleRate_);
        }
        for (BandLimitedOscillator& oscillator : osc2_) {
            oscillator.prepare(sampleRate_);
        }
        sub_.prepare(sampleRate_);
        ampEnvelope_.prepare(sampleRate_);
        filterEnvelope_.prepare(sampleRate_);
        filter_.prepare(sampleRate_);
        baseCutoff_.prepare(sampleRate_);
        resonance_.prepare(sampleRate_);
        velocityGain_.prepare(sampleRate_);
        osc1Level_.prepare(sampleRate_);
        osc2Level_.prepare(sampleRate_);
        reset();
    }

    /// Setup thread (or audio thread after the stream stops).
    void reset() noexcept {
        for (BandLimitedOscillator& oscillator : osc1_) {
            oscillator.reset();
        }
        for (BandLimitedOscillator& oscillator : osc2_) {
            oscillator.reset();
        }
        sub_.reset();
        ampEnvelope_.reset();
        filterEnvelope_.reset();
        filter_.reset();
        baseCutoff_.reset(0.0F);
        resonance_.reset(0.0F);
        velocityGain_.reset(0.0F);
        osc1Level_.reset(0.0F);
        osc2Level_.reset(0.0F);
        note_ = 0;
        velocity_ = 0.0F;
        age_ = 0;
        held_ = false;
        unisonCount_ = 1;
        noiseState_ = 0x9E3779B9U;
    }

    /// Audio thread. Starts (or retriggers) the voice with the given patch and
    /// the processor's current live values. The oscillator phase is reset only
    /// when the voice was silent: a stolen voice keeps its phase and glides to
    /// the new pitch instead of clicking.
    void noteOn(std::uint8_t note, float velocity, std::uint64_t age, const Patch& patch,
                const LiveParameters& live) noexcept {
        const bool wasIdle = !ampEnvelope_.isActive();

        patch_ = patch;
        note_ = note;
        age_ = age;
        velocity_ = std::clamp(velocity, 0.0F, 1.0F);
        held_ = true;
        unisonCount_ = std::clamp(patch.unisonVoices, 1, kMaxUnison);

        const float baseFrequency = core::noteToFrequency(note);
        const int layers = unisonCount_;
        for (int layer = 0; layer < layers; ++layer) {
            const float spread = layerSpread(layer, layers);
            const float frequency1 =
                baseFrequency * centsToRatio(patch.osc1DetuneCents +
                                             spread * patch.unisonDetuneCents);
            const float frequency2 =
                baseFrequency * centsToRatio(patch.osc2DetuneCents +
                                             spread * patch.unisonDetuneCents);
            layerFrequency1_[layer] = frequency1;
            layerFrequency2_[layer] = frequency2;

            osc1_[layer].setWaveform(patch.osc1Waveform);
            osc1_[layer].setPulseWidth(patch.osc1PulseWidth);
            osc1_[layer].setFrequency(frequency1);
            osc2_[layer].setWaveform(patch.osc2Waveform);
            osc2_[layer].setPulseWidth(patch.osc2PulseWidth);
            osc2_[layer].setFrequency(frequency2);

            if (wasIdle) {
                osc1_[layer].setPhase(std::fmod(0.11F + 0.19F * static_cast<float>(layer),
                                                1.0F));
                osc2_[layer].setPhase(std::fmod(0.37F + 0.23F * static_cast<float>(layer),
                                                1.0F));
            }

            // Constant-power pan per layer. Stereo width only spreads the
            // unison layers; a single layer stays centred.
            const float pan = std::clamp(spread * patch.unisonStereo, -1.0F, 1.0F);
            const float angle = (pan + 1.0F) * 0.25F * kPi;
            const float leftGain = std::cos(angle);
            const float rightGain = std::sin(angle);
            const float layerGain = 1.0F / static_cast<float>(layers);
            layerGainLeft1_[layer] = leftGain * layerGain;
            layerGainRight1_[layer] = rightGain * layerGain;
            layerGainLeft2_[layer] = leftGain * layerGain;
            layerGainRight2_[layer] = rightGain * layerGain;
        }
        for (int layer = layers; layer < kMaxUnison; ++layer) {
            layerGainLeft1_[layer] = 0.0F;
            layerGainRight1_[layer] = 0.0F;
            layerGainLeft2_[layer] = 0.0F;
            layerGainRight2_[layer] = 0.0F;
        }

        subFrequency_ = baseFrequency * 0.5F;
        sub_.setWaveform(Waveform::Sine);
        sub_.setFrequency(subFrequency_);
        if (wasIdle) {
            sub_.setPhase(0.0F);
        }
        noiseState_ = 0x9E3779B9U ^ (static_cast<std::uint32_t>(age_) * 0x85EBCA6BU) ^
                      static_cast<std::uint32_t>(note_) * 0xC2B2AE35U;
        if (noiseState_ == 0U) {
            noiseState_ = 1U;
        }

        ampEnvelope_.setParameters(live.ampEnvelope);
        filterEnvelope_.setParameters(live.filterEnvelope);
        ampEnvelope_.noteOn();
        filterEnvelope_.noteOn();

        if (wasIdle) {
            filter_.reset();
        }
        filter_.setCutoff(live.cutoffHz);
        filter_.setResonance(live.resonance);

        baseCutoff_.reset(live.cutoffHz);
        resonance_.reset(live.resonance);

        const float velocityScale =
            (1.0F - patch.velocityVolume) + patch.velocityVolume * velocity_;
        if (wasIdle) {
            velocityGain_.reset(velocityScale);
            osc1Level_.reset(live.osc1Level);
            osc2Level_.reset(live.osc2Level);
        } else {
            velocityGain_.setTargetValue(velocityScale);
            osc1Level_.setTargetValue(live.osc1Level);
            osc2Level_.setTargetValue(live.osc2Level);
        }
    }

    /// Audio thread.
    void noteOff() noexcept {
        held_ = false;
        ampEnvelope_.noteOff();
        filterEnvelope_.noteOff();
    }

    /// Audio thread. Live overrides from the processor: the captured patch stays
    /// the base, these ramp on top of it. Applies to sounding voices and is
    /// also what a new voice starts from.
    void setLiveParameters(const LiveParameters& live) noexcept {
        baseCutoff_.setTargetValue(live.cutoffHz);
        resonance_.setTargetValue(live.resonance);
        osc1Level_.setTargetValue(live.osc1Level);
        osc2Level_.setTargetValue(live.osc2Level);
        ampEnvelope_.setParameters(live.ampEnvelope);
        filterEnvelope_.setParameters(live.filterEnvelope);
    }

    /// Audio thread. Adds this voice into the stereo mix. `lfo` points at the
    /// per-sample LFO values for the block (never null in the processor).
    void renderAdd(float* left, float* right, int numFrames, const float* lfo) noexcept {
        if (!isActive()) {
            return;
        }

        const int layers = unisonCount_;
        const bool hasSub = patch_.subLevel > 0.0F;
        const bool hasNoise = patch_.noiseLevel > 0.0F;
        const bool hasVibrato = patch_.lfoPitchDepthCents > 0.0F;
        const float velocityBrightness = patch_.velocityBrightness * velocity_ * 2.0F;

        for (int frame = 0; frame < numFrames; ++frame) {
            const float lfoValue = lfo != nullptr ? lfo[frame] : 0.0F;
            const float amp = ampEnvelope_.nextValue();
            const float filterEnv = filterEnvelope_.nextValue();
            const float baseCutoff = baseCutoff_.getNextValue();
            const float osc1Level = osc1Level_.getNextValue();
            const float osc2Level = osc2Level_.getNextValue();

            const float octaves = patch_.filterEnvOctaves * filterEnv + velocityBrightness +
                                  patch_.lfoCutoffDepth * lfoValue * 2.0F;
            const float cutoff =
                std::clamp(baseCutoff * std::exp2(octaves),
                           ResonantLowPassFilter::kMinCutoffHz,
                           ResonantLowPassFilter::kMaxCutoffHz);
            filter_.setCutoff(cutoff);
            filter_.setResonance(resonance_.getNextValue());

            float vibratoRatio = 1.0F;
            if (hasVibrato) {
                vibratoRatio =
                    std::exp2(patch_.lfoPitchDepthCents * lfoValue / 1200.0F);
            }

            float mixLeft = 0.0F;
            float mixRight = 0.0F;
            for (int layer = 0; layer < layers; ++layer) {
                if (hasVibrato) {
                    osc1_[layer].setFrequencyModulated(layerFrequency1_[layer], vibratoRatio);
                    osc2_[layer].setFrequencyModulated(layerFrequency2_[layer], vibratoRatio);
                }
                const float sample1 = osc1_[layer].nextSample();
                const float sample2 = osc2_[layer].nextSample();
                mixLeft += sample1 * layerGainLeft1_[layer] * osc1Level +
                           sample2 * layerGainLeft2_[layer] * osc2Level;
                mixRight += sample1 * layerGainRight1_[layer] * osc1Level +
                            sample2 * layerGainRight2_[layer] * osc2Level;
            }

            if (hasSub) {
                if (hasVibrato) {
                    sub_.setFrequencyModulated(subFrequency_, vibratoRatio);
                }
                const float sample = sub_.nextSample() * patch_.subLevel * 0.70710678F;
                mixLeft += sample;
                mixRight += sample;
            }
            if (hasNoise) {
                const float sample = nextNoise() * patch_.noiseLevel;
                mixLeft += sample;
                mixRight += sample;
            }

            filter_.processStereo(mixLeft, mixRight);

            const float gain = amp * velocityGain_.getNextValue() * kOutputGain;
            left[frame] += mixLeft * gain;
            right[frame] += mixRight * gain;
        }
    }

    [[nodiscard]] bool isActive() const noexcept { return ampEnvelope_.isActive() || held_; }
    [[nodiscard]] bool isFree() const noexcept { return !isActive(); }
    [[nodiscard]] std::uint8_t note() const noexcept { return note_; }
    [[nodiscard]] bool isHeld() const noexcept { return held_; }
    [[nodiscard]] float velocity() const noexcept { return velocity_; }
    [[nodiscard]] float envelopeLevel() const noexcept { return ampEnvelope_.level(); }
    [[nodiscard]] EnvelopeStage envelopeStage() const noexcept { return ampEnvelope_.stage(); }
    [[nodiscard]] std::uint64_t age() const noexcept { return age_; }
    [[nodiscard]] const Patch& patch() const noexcept { return patch_; }

private:
    [[nodiscard]] static float layerSpread(int layer, int layers) noexcept {
        if (layers <= 1) {
            return 0.0F;
        }
        return (2.0F * static_cast<float>(layer) / static_cast<float>(layers - 1)) - 1.0F;
    }

    [[nodiscard]] static float centsToRatio(float cents) noexcept {
        return std::exp2(cents / 1200.0F);
    }

    [[nodiscard]] float nextNoise() noexcept {
        noiseState_ ^= noiseState_ << 13U;
        noiseState_ ^= noiseState_ >> 17U;
        noiseState_ ^= noiseState_ << 5U;
        return static_cast<float>(static_cast<std::int32_t>(noiseState_)) *
               (1.0F / 2147483648.0F);
    }

    BandLimitedOscillator osc1_[kMaxUnison];
    BandLimitedOscillator osc2_[kMaxUnison];
    BandLimitedOscillator sub_;
    AdsrEnvelope ampEnvelope_;
    AdsrEnvelope filterEnvelope_;
    ResonantLowPassFilter filter_;
    core::SmoothedValue baseCutoff_;
    core::SmoothedValue resonance_;
    core::SmoothedValue velocityGain_;
    core::SmoothedValue osc1Level_;
    core::SmoothedValue osc2Level_;

    Patch patch_{};
    float layerGainLeft1_[kMaxUnison]{};
    float layerGainRight1_[kMaxUnison]{};
    float layerGainLeft2_[kMaxUnison]{};
    float layerGainRight2_[kMaxUnison]{};
    float layerFrequency1_[kMaxUnison]{};
    float layerFrequency2_[kMaxUnison]{};
    float subFrequency_ = 440.0F;
    int unisonCount_ = 1;

    std::uint8_t note_ = 0;
    float velocity_ = 0.0F;
    std::uint64_t age_ = 0;
    bool held_ = false;
    std::uint32_t noiseState_ = 0x9E3779B9U;
    double sampleRate_ = 48000.0;
};

}  // namespace drumas::synth
