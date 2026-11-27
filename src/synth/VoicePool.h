// Fixed-size polyphonic voice pool with simple voice stealing.
//
// Nothing is ever allocated: the pool is an array of voices sized at compile
// time. Notes arrive either individually (piano, console) or as a whole chord
// (ChordOn/ChordOff from a pad), which is the reason for the group masks below.
//
// Chords: a ChordOn allocates every voice before starting any note, so all the
// notes of a chord begin on the same sample and a chord is never half played.
// Each voice carries a bit per chord group. When a note is already held by
// another chord (C in C major and in A minor), the new chord joins that voice
// instead of retriggering it, and a ChordOff only drops its own bit: the note
// keeps sounding for the chord that still needs it.
//
// Stealing: when the pool is exhausted, a voice in release goes first, then the
// quietest, oldest on ties. A started voice fades in over ~2 ms so a reused
// voice never steps.
//
// Thread contract: prepare()/reset() on the setup thread; noteOn()/noteOff()/
// chordOn()/chordOff()/renderAdd()/setLiveParameters() on the audio thread.
// RT-SAFE after prepare().
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/AudioTypes.h"
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

    /// Audio thread. A single note, not owned by any chord.
    void noteOn(std::uint8_t note, float velocity, const Patch& patch,
                const LiveParameters& live) noexcept {
        SynthVoice* voice = findUngroupedVoicePlaying(note);
        if (voice == nullptr) {
            voice = findFreeVoice(nullptr, 0);
        }
        if (voice == nullptr) {
            voice = findStealCandidate(nullptr, 0);
            ++stealCount_;
        }
        voice->noteOn(note, velocity, nextAge_++, patch, live);
    }

    /// Audio thread. Releases every voice playing that note outside a chord.
    void noteOff(std::uint8_t note) noexcept {
        for (SynthVoice& voice : voices_) {
            if (voice.isHeld() && voice.note() == note && voice.groupMask() == 0) {
                voice.noteOff();
            }
        }
    }

    /// Audio thread. Starts a whole chord atomically. Returns false (and starts
    /// nothing) if the group or the note list is invalid.
    [[nodiscard]] bool chordOn(std::uint8_t group, const std::uint8_t* notes,
                               std::uint8_t noteCount, float velocity, const Patch& patch,
                               const LiveParameters& live) noexcept {
        if (group == 0 || group > core::kMaxChordGroup || notes == nullptr ||
            noteCount == 0 || noteCount > core::kMaxChordNotes) {
            return false;
        }

        const std::uint32_t bit = 1U << group;
        std::array<SynthVoice*, core::kMaxChordNotes> targets{};
        std::array<std::uint8_t, core::kMaxChordNotes> targetNotes{};

        // Reserve pass: choose every voice first so a chord can never start
        // partially, and so two notes never land on the same voice.
        for (std::uint8_t index = 0; index < noteCount; ++index) {
            const std::uint8_t note = notes[index];
            SynthVoice* voice = nullptr;

            // The same note twice in one chord (possible in a manual voicing)
            // is only started once.
            bool duplicate = false;
            for (std::uint8_t previous = 0; previous < index; ++previous) {
                if (targetNotes[previous] == note) {
                    duplicate = true;
                    voice = targets[previous];
                    break;
                }
            }
            if (!duplicate) {
                voice = findVoiceHeldByOtherGroup(note, bit);
            }
            if (voice == nullptr) {
                voice = findVoiceInGroup(note, bit);
            }
            if (voice == nullptr) {
                voice = findFreeVoice(targets.data(), index);
            }
            if (voice == nullptr) {
                voice = findStealCandidate(targets.data(), index);
                ++stealCount_;
            }
            targets[index] = voice;
            targetNotes[index] = note;
        }

        // Start pass: shared notes just join the new group; new voices start
        // together on this block's first sample.
        for (std::uint8_t index = 0; index < noteCount; ++index) {
            SynthVoice& voice = *targets[index];
            const std::uint8_t note = targetNotes[index];
            if (voice.isHeld() && voice.note() == note &&
                (voice.groupMask() & bit) != 0) {
                continue;  // already ours
            }
            if (voice.isHeld() && voice.note() == note && voice.groupMask() != 0) {
                voice.addGroup(bit);
                continue;  // shared with another chord: never retriggered
            }
            voice.noteOn(note, velocity, nextAge_++, patch, live, bit);
        }
        return true;
    }

    /// Audio thread. Drops this group's claim on every voice it holds. Voices
    /// still claimed by another chord keep sounding; the rest release.
    void chordOff(std::uint8_t group) noexcept {
        if (group == 0 || group > core::kMaxChordGroup) {
            return;
        }
        const std::uint32_t bit = 1U << group;
        for (SynthVoice& voice : voices_) {
            if ((voice.groupMask() & bit) == 0) {
                continue;
            }
            voice.clearGroup(bit);
            if (voice.groupMask() == 0) {
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
    [[nodiscard]] static bool isTaken(const SynthVoice* const* taken,
                                      std::size_t takenCount,
                                      const SynthVoice* candidate) noexcept {
        for (std::size_t index = 0; index < takenCount; ++index) {
            if (taken[index] == candidate) {
                return true;
            }
        }
        return false;
    }

    /// A plain note retriggers only another plain note; a chord voice is left
    /// alone so the keyboard never steals a held pad.
    [[nodiscard]] SynthVoice* findUngroupedVoicePlaying(std::uint8_t note) noexcept {
        for (SynthVoice& voice : voices_) {
            if (voice.isActive() && voice.note() == note && voice.groupMask() == 0) {
                return &voice;
            }
        }
        return nullptr;
    }

    [[nodiscard]] SynthVoice* findVoiceHeldByOtherGroup(std::uint8_t note,
                                                       std::uint32_t bit) noexcept {
        for (SynthVoice& voice : voices_) {
            if (voice.isHeld() && voice.note() == note && voice.groupMask() != 0 &&
                (voice.groupMask() & bit) == 0) {
                return &voice;
            }
        }
        return nullptr;
    }

    [[nodiscard]] SynthVoice* findVoiceInGroup(std::uint8_t note,
                                               std::uint32_t bit) noexcept {
        for (SynthVoice& voice : voices_) {
            if (voice.isHeld() && voice.note() == note && (voice.groupMask() & bit) != 0) {
                return &voice;
            }
        }
        return nullptr;
    }

    [[nodiscard]] SynthVoice* findFreeVoice(const SynthVoice* const* taken,
                                            std::size_t takenCount) noexcept {
        for (SynthVoice& voice : voices_) {
            if (voice.isFree() && !isTaken(taken, takenCount, &voice)) {
                return &voice;
            }
        }
        return nullptr;
    }

    /// Voices already in release go first (their contribution is already
    /// decaying); otherwise the quietest wins, oldest on ties.
    [[nodiscard]] SynthVoice* findStealCandidate(const SynthVoice* const* taken,
                                                 std::size_t takenCount) noexcept {
        SynthVoice* releasing = nullptr;
        SynthVoice* quietest = nullptr;
        for (SynthVoice& voice : voices_) {
            if (isTaken(taken, takenCount, &voice)) {
                continue;
            }
            if (voice.isReleasing() &&
                (releasing == nullptr || voice.age() < releasing->age())) {
                releasing = &voice;
            }
            if (quietest == nullptr ||
                voice.envelopeLevel() < quietest->envelopeLevel() ||
                (voice.envelopeLevel() == quietest->envelopeLevel() &&
                 voice.age() < quietest->age())) {
                quietest = &voice;
            }
        }
        return releasing != nullptr ? releasing : (quietest != nullptr ? quietest : &voices_[0]);
    }

    std::array<SynthVoice, kMaxVoices> voices_{};
    std::uint64_t nextAge_ = 0;
    std::size_t stealCount_ = 0;
};

}  // namespace drumas::synth
