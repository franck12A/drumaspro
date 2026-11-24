// Fixed-size polyphonic voice pool with simple voice stealing.
//
// Nothing is ever allocated: the pool is an array of voices sized at compile
// time. Politeness of the steal is deliberately simple -- take the quietest
// voice (lowest amp envelope level), oldest first on ties -- which is what a
// keyboard prototype needs and what tests can assert deterministically.
//
// Thread contract: prepare()/reset() on the setup thread; noteOn()/noteOff()/
// renderAdd()/setLiveParameters() on the audio thread. RT-SAFE after prepare().
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "synth/Patch.h"
#include "synth/SynthVoice.h"

namespace drumas::synth {

class VoicePool {
public:
    /// 32 voices: the benchmark target of this stage and enough for a four-note
    /// chord with a long release tail.
    static constexpr std::size_t kMaxVoices = 32;

    /// Setup thread.
    void prepare(double sampleRate) noexcept {
        for (SynthVoice& voice : voices_) {
            voice.prepare(sampleRate);
        }
        nextAge_ = 0;
        stealCount_ = 0;
    }

    /// Setup thread.
    void reset() noexcept {
        for (SynthVoice& voice : voices_) {
            voice.reset();
        }
        nextAge_ = 0;
        stealCount_ = 0;
    }

    /// Audio thread. Hard reset of every voice (also kills release tails).
    void silence() noexcept {
        for (SynthVoice& voice : voices_) {
            voice.reset();
        }
    }

    /// Audio thread.
    void noteOn(std::uint8_t note, float velocity, const Patch& patch,
                const LiveParameters& live) noexcept {
        SynthVoice* voice = findVoicePlaying(note);
        if (voice == nullptr) {
            voice = findFreeVoice();
        }
        if (voice == nullptr) {
            voice = findStealCandidate();
            ++stealCount_;
        }
        voice->noteOn(note, velocity, nextAge_++, patch, live);
    }

    /// Audio thread. Releases every voice playing that note.
    void noteOff(std::uint8_t note) noexcept {
        for (SynthVoice& voice : voices_) {
            if (voice.isHeld() && voice.note() == note) {
                voice.noteOff();
            }
        }
    }

    /// Audio thread.
    void allNotesOff() noexcept {
        for (SynthVoice& voice : voices_) {
            if (voice.isHeld()) {
                voice.noteOff();
            }
        }
    }

    /// Audio thread. Applies live overrides to every voice (no allocation).
    void setLiveParameters(const LiveParameters& live) noexcept {
        for (SynthVoice& voice : voices_) {
            voice.setLiveParameters(live);
        }
    }

    /// Audio thread. Renders all active voices additively into the stereo mix.
    void renderAdd(float* left, float* right, int numFrames, const float* lfo) noexcept {
        for (SynthVoice& voice : voices_) {
            voice.renderAdd(left, right, numFrames, lfo);
        }
    }

    /// Diagnostics. Audio thread only, or any thread while the stream is stopped.
    [[nodiscard]] std::size_t activeVoiceCount() const noexcept {
        std::size_t count = 0;
        for (const SynthVoice& voice : voices_) {
            if (voice.isActive()) {
                ++count;
            }
        }
        return count;
    }

    /// Number of steals since prepare(). Monotonic, for diagnostics.
    [[nodiscard]] std::size_t stealCount() const noexcept { return stealCount_; }

    [[nodiscard]] SynthVoice& voice(std::size_t index) noexcept { return voices_[index]; }
    [[nodiscard]] const SynthVoice& voice(std::size_t index) const noexcept {
        return voices_[index];
    }

private:
    [[nodiscard]] SynthVoice* findVoicePlaying(std::uint8_t note) noexcept {
        for (SynthVoice& voice : voices_) {
            if (voice.isActive() && voice.note() == note) {
                return &voice;
            }
        }
        return nullptr;
    }

    [[nodiscard]] SynthVoice* findFreeVoice() noexcept {
        for (SynthVoice& voice : voices_) {
            if (voice.isFree()) {
                return &voice;
            }
        }
        return nullptr;
    }

    /// Quietest voice wins; ties go to the oldest (smallest age).
    [[nodiscard]] SynthVoice* findStealCandidate() noexcept {
        SynthVoice* candidate = &voices_[0];
        for (SynthVoice& voice : voices_) {
            const float level = voice.envelopeLevel();
            const float candidateLevel = candidate->envelopeLevel();
            if (level < candidateLevel ||
                (level == candidateLevel && voice.age() < candidate->age())) {
                candidate = &voice;
            }
        }
        return candidate;
    }

    std::array<SynthVoice, kMaxVoices> voices_{};
    std::uint64_t nextAge_ = 0;
    std::size_t stealCount_ = 0;
};

}  // namespace drumas::synth
