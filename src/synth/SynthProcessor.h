// The processor the engine drives: turns queued commands into sound.
//
// Commands arrive once per block, at the very start, already drained by the
// engine. Everything below that point runs on the audio thread and is
// allocation-free after prepare().
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <span>
#include <vector>

#include "core/AudioTypes.h"
#include "core/IAudioProcessor.h"
#include "core/SmoothedValue.h"
#include "synth/MasterLimiter.h"
#include "synth/ParameterIds.h"
#include "synth/VoicePool.h"

namespace drumas::synth {

class SynthProcessor final : public core::IAudioProcessor {
public:
    /// Default master gain. Sixteen sine voices summing at unity would clip long
    /// before the pool is full, so the master stage starts with headroom.
    static constexpr float kDefaultMasterGain = 0.25F;

    /// Control thread. Allocates the mono mix buffer (maxBlockSize samples).
    void prepare(const core::AudioSpec& spec) override {
        prepare(spec.sampleRate, spec.maxBlockSize, spec.channels);
    }

    /// Control thread. Same thing without the channel count, which the synth
    /// does not need (it renders mono and broadcasts to every channel).
    void prepare(double sampleRate, int maxBlockSize, int /*channels*/ = core::kDefaultChannels) {
        sampleRate_ = sampleRate;
        maxBlockSize_ = std::max(maxBlockSize, 1);
        mix_.assign(static_cast<std::size_t>(maxBlockSize_), 0.0F);

        masterGain_.prepare(sampleRate);
        masterGain_.reset(kDefaultMasterGain);

        pool_.prepare(sampleRate);
        pool_.setEnvelopeParameters(parameters_);
        parametersDirty_ = false;
    }

    /// Control thread.
    void reset() override {
        pool_.silence();
        std::fill(mix_.begin(), mix_.end(), 0.0F);
        masterGain_.reset(kDefaultMasterGain);
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

        if (numFrames > maxBlockSize_ || mix_.empty()) {
            // Cannot render more than was prepared without allocating. Silence
            // is the only real-time-safe answer; the engine sizes its buffers so
            // this does not happen.
            for (int channel = 0; channel < numChannels; ++channel) {
                std::fill_n(output[channel], numFrames, 0.0F);
            }
            return;
        }

        std::fill_n(mix_.data(), static_cast<std::size_t>(numFrames), 0.0F);
        pool_.renderAdd(mix_.data(), numFrames);

        for (int frame = 0; frame < numFrames; ++frame) {
            const float gain = masterGain_.getNextValue();
            // Soft clip instead of the stage-3 hard clip: transparent below the
            // knee, guaranteed ceiling above it. See
            // docs/adr/0005-master-soft-clip.md for the trade-off.
            const float sample =
                MasterLimiter::processSample(mix_[static_cast<std::size_t>(frame)] * gain);
            for (int channel = 0; channel < numChannels; ++channel) {
                output[channel][frame] = sample;
            }
        }

        // audio -> control readout: a single relaxed word, no tearing to worry
        // about, no lock, no allocation.
        activeVoices_.store(static_cast<std::uint32_t>(pool_.activeVoiceCount()),
                            std::memory_order_relaxed);
    }

    /// Audio thread. Applies a parameter change received outside the block.
    void applyCommand(const core::Command& command) noexcept {
        switch (command.type) {
            case core::CommandType::NoteOn:
                pool_.noteOn(command.note, command.value);
                break;
            case core::CommandType::NoteOff:
                pool_.noteOff(command.note);
                break;
            case core::CommandType::AllNotesOff:
                pool_.allNotesOff();
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

private:
    void applyParameter(core::ParameterId param, float value) noexcept {
        switch (param) {
            case kMasterGain:
                masterGain_.setTargetValue(std::clamp(value, 0.0F, 1.0F));
                break;
            case kAmpAttackMs:
                parameters_.attackMs = std::max(value, 0.01F);
                parametersDirty_ = true;
                break;
            case kAmpDecayMs:
                parameters_.decayMs = std::max(value, 0.01F);
                parametersDirty_ = true;
                break;
            case kAmpSustain:
                parameters_.sustain = std::clamp(value, 0.0F, 1.0F);
                parametersDirty_ = true;
                break;
            case kAmpReleaseMs:
                parameters_.releaseMs = std::max(value, 0.01F);
                parametersDirty_ = true;
                break;
            default:
                // Not a synth parameter (yet): ignore, the command still counts
                // as applied. Unknown ids are also rejected by the registry on
                // the control side.
                break;
        }
        if (parametersDirty_) {
            // Recomputing rates is arithmetic only: safe on the audio thread.
            pool_.setEnvelopeParameters(parameters_);
            parametersDirty_ = false;
        }
    }

    VoicePool pool_;
    core::SmoothedValue masterGain_;
    std::vector<float> mix_;
    AdsrParameters parameters_{};
    double sampleRate_ = core::kDefaultSampleRate;
    int maxBlockSize_ = 0;
    bool parametersDirty_ = false;
    std::atomic<std::uint32_t> activeVoices_{0};
};

}  // namespace drumas::synth
