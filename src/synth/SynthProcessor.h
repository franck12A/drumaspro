// The processor the engine drives: turns queued commands into sound.
//
// Commands arrive once per block, at the very start, already drained by the
// engine. Everything below that point runs on the audio thread and is
// allocation-free after prepare().
//
// Architecture (stage 4):
//   - one global LFO, computed once per sample and shared by every voice;
//   - 32 subtractive voices in a fixed pool;
//   - one patch bank; a voice captures the patch on note-on;
//   - live overrides (cutoff, resonance, envelope times) that a SetParam can
//     move on sounding voices with a ramp, as decided in
//     docs/adr/0007-patch-switching-and-live-parameters.md;
//   - master gain and a soft clip that guarantees the output ceiling.
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

#include "core/AudioTypes.h"
#include "core/IAudioProcessor.h"
#include "core/ParameterRegistry.h"
#include "core/SmoothedValue.h"
#include "synth/Lfo.h"
#include "synth/MasterLimiter.h"
#include "synth/ParameterIds.h"
#include "synth/PatchBank.h"
#include "synth/VoicePool.h"

namespace drumas::synth {

class SynthProcessor final : public core::IAudioProcessor {
public:
    /// Control thread. Allocates the stereo mix buffers and the LFO block; every
    /// allocation for the audio path happens here.
    void prepare(const core::AudioSpec& spec) override {
        prepare(spec.sampleRate, spec.maxBlockSize, spec.channels);
    }

    /// Control thread. Same thing without the channel count, which the synth
    /// only uses to decide how to broadcast its stereo mix.
    void prepare(double sampleRate, int maxBlockSize, int /*channels*/ = core::kDefaultChannels) {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : core::kDefaultSampleRate;
        maxBlockSize_ = std::max(maxBlockSize, 1);
        const auto frames = static_cast<std::size_t>(maxBlockSize_);
        mixLeft_.assign(frames, 0.0F);
        mixRight_.assign(frames, 0.0F);
        lfoBuffer_.assign(frames, 0.0F);

        registry_.clear();
        (void)registerSynthParameters(registry_);

        masterGain_.prepare(sampleRate_);
        lfo_.prepare(sampleRate_);
        pool_.prepare(sampleRate_);

        currentPatchIndex_ = std::clamp(kDefaultPatchIndex, 0, kFactoryPatchCount - 1);
        currentPatch_ = kFactoryPatches[static_cast<std::size_t>(currentPatchIndex_)];
        live_ = liveParametersFrom(currentPatch_);
        lfoRateHz_ = currentPatch_.lfoRateHz;
        lfoShape_ = currentPatch_.lfoShape;
        lfo_.setRate(lfoRateHz_);
        lfo_.setShape(lfoShape_);
        pool_.setLiveParameters(live_);

        masterGain_.reset(registry_.defaultValue(kMasterGain));
        activeVoices_.store(0, std::memory_order_relaxed);
    }

    /// Control thread.
    void reset() override {
        pool_.silence();
        lfo_.reset();
        std::fill(mixLeft_.begin(), mixLeft_.end(), 0.0F);
        std::fill(mixRight_.begin(), mixRight_.end(), 0.0F);
        masterGain_.reset(registry_.defaultValue(kMasterGain));
        live_ = liveParametersFrom(currentPatch_);
        pool_.setLiveParameters(live_);
        activeVoices_.store(0, std::memory_order_relaxed);
    }

    /// Audio thread. RT-SAFE.
    void process(std::span<const core::Command> commands, float* const* output, int numChannels,
                 int numFrames) noexcept override {
        for (const core::Command& command : commands) {
            applyCommand(command);
        }

        if (output == nullptr || numChannels <= 0 || numFrames <= 0) {
            return;
        }

        if (numFrames > maxBlockSize_ || mixLeft_.empty()) {
            // Cannot render more than was prepared without allocating. Silence
            // is the only real-time-safe answer; the engine sizes its buffers so
            // this does not happen.
            for (int channel = 0; channel < numChannels; ++channel) {
                std::fill_n(output[channel], numFrames, 0.0F);
            }
            return;
        }

        std::fill_n(mixLeft_.data(), static_cast<std::size_t>(numFrames), 0.0F);
        std::fill_n(mixRight_.data(), static_cast<std::size_t>(numFrames), 0.0F);

        for (int frame = 0; frame < numFrames; ++frame) {
            lfoBuffer_[static_cast<std::size_t>(frame)] = lfo_.nextValue();
        }
        pool_.renderAdd(mixLeft_.data(), mixRight_.data(), numFrames, lfoBuffer_.data());

        for (int frame = 0; frame < numFrames; ++frame) {
            const float gain = masterGain_.getNextValue();
            const float left =
                MasterLimiter::processSample(mixLeft_[static_cast<std::size_t>(frame)] * gain);
            const float right =
                MasterLimiter::processSample(mixRight_[static_cast<std::size_t>(frame)] * gain);

            if (numChannels == 1) {
                output[0][frame] = 0.5F * (left + right);
            } else {
                output[0][frame] = left;
                output[1][frame] = right;
                for (int channel = 2; channel < numChannels; ++channel) {
                    output[channel][frame] = (channel % 2 == 0) ? left : right;
                }
            }
        }

        activeVoices_.store(static_cast<std::uint32_t>(pool_.activeVoiceCount()),
                            std::memory_order_relaxed);
    }

    /// Audio thread. Applies a command received outside the block (tests and
    /// the console both go through the queue, but this keeps the command
    /// semantics in one place).
    void applyCommand(const core::Command& command) noexcept {
        switch (command.type) {
            case core::CommandType::NoteOn:
                pool_.noteOn(command.note, command.value, currentPatch_, live_);
                break;
            case core::CommandType::NoteOff:
                pool_.noteOff(command.note);
                break;
            case core::CommandType::AllNotesOff:
                pool_.allNotesOff();
                break;
            case core::CommandType::SelectPatch:
                selectPatch(command.patch);
                break;
            case core::CommandType::SetParam:
                applyParameter(command.param, command.value);
                break;
            case core::CommandType::None:
                break;
        }
    }

    /// Any thread. Published once per block; diagnostics only.
    [[nodiscard]] std::uint32_t activeVoices() const noexcept {
        return activeVoices_.load(std::memory_order_relaxed);
    }

    /// Audio thread only. Reading voice state from another thread would be a
    /// data race; use activeVoices() instead.
    [[nodiscard]] const VoicePool& voices() const noexcept { return pool_; }
    [[nodiscard]] double sampleRate() const noexcept { return sampleRate_; }
    [[nodiscard]] int maxBlockSize() const noexcept { return maxBlockSize_; }
    [[nodiscard]] float masterGainTarget() const noexcept { return masterGain_.target(); }

    /// Audio thread only. The patch new notes will use; sounding voices keep
    /// their captured copy.
    [[nodiscard]] int currentPatchIndex() const noexcept { return currentPatchIndex_; }
    [[nodiscard]] const Patch& currentPatch() const noexcept { return currentPatch_; }

    /// Any thread, after prepare(). The definitions are immutable from then on.
    [[nodiscard]] const core::ParameterRegistry& parameters() const noexcept {
        return registry_;
    }

private:
    void selectPatch(std::uint8_t patchIndex) noexcept {
        const int index = static_cast<int>(patchIndex);
        if (index < 0 || index >= kFactoryPatchCount) {
            return;
        }
        currentPatchIndex_ = index;
        currentPatch_ = kFactoryPatches[static_cast<std::size_t>(index)];
        live_ = liveParametersFrom(currentPatch_);
        lfoRateHz_ = currentPatch_.lfoRateHz;
        lfoShape_ = currentPatch_.lfoShape;
        lfo_.setRate(lfoRateHz_);
        lfo_.setShape(lfoShape_);
        // Deliberately does not touch sounding voices: they finish with the
        // patch they captured (ADR 0007). The next note-on uses the new patch.
    }

    void applyParameter(core::ParameterId param, float value) noexcept {
        // Clamp and reject unknown ids on the audio thread: the same registry
        // that will drive the UI is the single source of truth for ranges.
        if (!registry_.clampToRange(param, value)) {
            return;
        }

        switch (param) {
            case kMasterGain:
                masterGain_.setTargetValue(value);
                break;
            case kFilterCutoff:
                live_.cutoffHz = value;
                pool_.setLiveParameters(live_);
                break;
            case kFilterResonance:
                live_.resonance = value;
                pool_.setLiveParameters(live_);
                break;
            case kAmpAttackMs:
                live_.ampEnvelope.attackMs = value;
                pool_.setLiveParameters(live_);
                break;
            case kAmpDecayMs:
                live_.ampEnvelope.decayMs = value;
                pool_.setLiveParameters(live_);
                break;
            case kAmpSustain:
                live_.ampEnvelope.sustain = value;
                pool_.setLiveParameters(live_);
                break;
            case kAmpReleaseMs:
                live_.ampEnvelope.releaseMs = value;
                pool_.setLiveParameters(live_);
                break;
            case kFilterAttackMs:
                live_.filterEnvelope.attackMs = value;
                pool_.setLiveParameters(live_);
                break;
            case kFilterDecayMs:
                live_.filterEnvelope.decayMs = value;
                pool_.setLiveParameters(live_);
                break;
            case kFilterSustain:
                live_.filterEnvelope.sustain = value;
                pool_.setLiveParameters(live_);
                break;
            case kFilterReleaseMs:
                live_.filterEnvelope.releaseMs = value;
                pool_.setLiveParameters(live_);
                break;
            case kLfoRate:
                lfoRateHz_ = value;
                lfo_.setRate(value);
                break;
            case kLfoShape:
                lfoShape_ = static_cast<LfoShape>(static_cast<int>(value));
                lfo_.setShape(lfoShape_);
                break;
            default:
                // Patch-level parameters (oscillator shape, levels, unison,
                // LFO depths...) are not live-editable in this stage; they come
                // from the patch. The registry still validated the value.
                break;
        }
    }

    VoicePool pool_;
    Lfo lfo_;
    core::ParameterRegistry registry_;
    core::SmoothedValue masterGain_;
    std::vector<float> mixLeft_;
    std::vector<float> mixRight_;
    std::vector<float> lfoBuffer_;

    Patch currentPatch_{};
    LiveParameters live_{};
    int currentPatchIndex_ = kDefaultPatchIndex;
    float lfoRateHz_ = 5.0F;
    LfoShape lfoShape_ = LfoShape::Sine;

    double sampleRate_ = core::kDefaultSampleRate;
    int maxBlockSize_ = 0;
    std::atomic<std::uint32_t> activeVoices_{0};
};

}  // namespace drumas::synth
