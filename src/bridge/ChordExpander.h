// Turns a music chord into one ChordOn/ChordOff command.
//
// A chord travels as a single event, so the engine allocates every voice before
// starting any note: the chord is complete and sample-aligned, and a queue
// without capacity drops the whole chord instead of half of it. The group id
// identifies the pressed pad; ChordOff releases exactly that chord and leaves
// notes shared with another held chord sounding.
#pragma once

#include <atomic>
#include <cstdint>

#include "bridge/CommandSink.h"
#include "core/AudioTypes.h"
#include "music/Chord.h"

namespace drumas::bridge {

class ChordExpander {
public:
    [[nodiscard]] bool noteOn(CommandSink& sink, const music::Chord& chord, float velocity,
                              std::uint8_t group) noexcept {
        const auto midiNotes = music::chordToMidiNotes(chord);
        if (!midiNotes.has_value() || midiNotes->count == 0 ||
            midiNotes->count > core::kMaxChordNotes || group == 0 ||
            group > core::kMaxChordGroup) {
            droppedChords_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        if (!sink.push(core::chordOn(group, midiNotes->notes.data(), midiNotes->count,
                                     velocity))) {
            droppedChords_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }

    [[nodiscard]] bool noteOff(CommandSink& sink, std::uint8_t group) noexcept {
        if (group == 0 || group > core::kMaxChordGroup) {
            return false;
        }
        return sink.push(core::chordOff(group));
    }

    [[nodiscard]] std::uint64_t droppedChords() const noexcept {
        return droppedChords_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<std::uint64_t> droppedChords_{0};
};

}  // namespace drumas::bridge
