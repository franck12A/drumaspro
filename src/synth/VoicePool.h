// Fixed-size polyphonic voice pool with simple voice stealing.
//
// Nothing is ever allocated: the pool is an array of voices sized at compile
// time. Politeness of the steal is deliberately simple for this stage -- take
// the quietest voice (lowest envelope level), oldest first on ties -- which is
// what a keyboard prototype needs and what tests can assert deterministically.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "synth/SineVoice.h"

namespace drumas::synth {

class VoicePool {
public:
    static constexpr std::size_t kMaxVoices = 16;

    /// Setup thread.
    void prepare(double sampleRate) noexcept {
        for (SineVoice& voice : voices_) {
            voice.prepare(sampleRate);
            voice.setEnvelopeParameters(parameters_);
        }
        nextAge_ = 0;
        stealCount_ = 0;
    }

    /// Setup thread.
    void reset() noexcept {
        for (SineVoice& voice : voices_) {
            voice.reset();
        }
        nextAge_ = 0;
        stealCount_ = 0;
    }

    /// Audio thread.
    void noteOn(std::uint8_t note, float velocity) noexcept {
        SineVoice* voice = findVoicePlaying(note);
        if (voice == nullptr) {
            voice = findFreeVoice();
        }
        if (voice == nullptr) {
            voice = findStealCandidate();
            ++stealCount_;
        }
        voice->noteOn(note, velocity, nextAge_++);
    }

    /// Audio thread. Releases every voice playing that note.
    void noteOff(std::uint8_t note) noexcept {
        for (SineVoice& voice : voices_) {
            if (voice.isHeld() && voice.note() == note) {
                voice.noteOff();
            }
        }
    }

    /// Audio thread.
    void allNotesOff() noexcept {
        for (SineVoice& voice : voices_) {
            if (voice.isHeld()) {
                voice.noteOff();
            }
        }
    }

    /// Audio thread. Hard reset of every voice (also kills release tails).
    void silence() noexcept {
        for (SineVoice& voice : voices_) {
            voice.reset();
            voice.setEnvelopeParameters(parameters_);
        }
    }

    /// Audio thread. Applies to every voice; recomputes rates, no allocation.
    void setEnvelopeParameters(const AdsrParameters& parameters) noexcept {
        parameters_ = parameters;
        for (SineVoice& voice : voices_) {
            voice.setEnvelopeParameters(parameters);
        }
    }

    /// Audio thread. Renders all active voices additively into `mono`.
    void renderAdd(float* mono, int numFrames) noexcept {
        for (SineVoice& voice : voices_) {
            voice.renderAdd(mono, numFrames);
        }
    }

    /// Diagnostics. Audio thread only, or any thread while the stream is stopped.
    [[nodiscard]] std::size_t activeVoiceCount() const noexcept {
        std::size_t count = 0;
        for (const SineVoice& voice : voices_) {
            if (voice.isActive()) {
                ++count;
            }
        }
        return count;
    }

    /// Number of steals since prepare(). Monotonic, for diagnostics.
    [[nodiscard]] std::size_t stealCount() const noexcept { return stealCount_; }

    [[nodiscard]] const AdsrParameters& envelopeParameters() const noexcept { return parameters_; }

    [[nodiscard]] SineVoice& voice(std::size_t index) noexcept { return voices_[index]; }
    [[nodiscard]] const SineVoice& voice(std::size_t index) const noexcept {
        return voices_[index];
    }

private:
    [[nodiscard]] SineVoice* findVoicePlaying(std::uint8_t note) noexcept {
        for (SineVoice& voice : voices_) {
            if (voice.isActive() && voice.note() == note) {
                return &voice;
            }
        }
        return nullptr;
    }

    [[nodiscard]] SineVoice* findFreeVoice() noexcept {
        for (SineVoice& voice : voices_) {
            if (voice.isFree()) {
                return &voice;
            }
        }
        return nullptr;
    }

    /// Quietest voice wins; ties go to the oldest (smallest age).
    [[nodiscard]] SineVoice* findStealCandidate() noexcept {
        SineVoice* candidate = &voices_[0];
        for (SineVoice& voice : voices_) {
            const float level = voice.envelopeLevel();
            const float candidateLevel = candidate->envelopeLevel();
            if (level < candidateLevel ||
                (level == candidateLevel && voice.age() < candidate->age())) {
                candidate = &voice;
            }
        }
        return candidate;
    }

    std::array<SineVoice, kMaxVoices> voices_{};
    AdsrParameters parameters_{};
    std::uint64_t nextAge_ = 0;
    std::size_t stealCount_ = 0;
};

}  // namespace drumas::synth
