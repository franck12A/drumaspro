// Expands a music chord into one all-or-nothing batch of MIDI note commands.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <span>

#include "bridge/CommandSink.h"
#include "music/Chord.h"

namespace drumas::bridge {

class ChordExpander {
public:
    [[nodiscard]] bool noteOn(CommandSink& sink, const music::Chord& chord,
                              float velocity = 1.0F) noexcept {
        return enqueue(sink, chord, velocity, true);
    }

    [[nodiscard]] bool noteOff(CommandSink& sink, const music::Chord& chord) noexcept {
        return enqueue(sink, chord, 0.0F, false);
    }

    [[nodiscard]] std::uint64_t droppedChords() const noexcept {
        return droppedChords_.load(std::memory_order_relaxed);
    }

private:
    [[nodiscard]] bool enqueue(CommandSink& sink, const music::Chord& chord, float velocity,
                               bool noteOn) noexcept {
        const auto midiNotes = music::chordToMidiNotes(chord);
        if (!midiNotes.has_value()) {
            droppedChords_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        std::array<core::Command, 4> commands{};
        for (std::size_t index = 0; index < midiNotes->count; ++index) {
            commands[index] = noteOn ? core::noteOn(midiNotes->notes[index], velocity)
                                     : core::noteOff(midiNotes->notes[index]);
        }

        if (!sink.pushBatch(std::span<const core::Command>(commands.data(), midiNotes->count))) {
            droppedChords_.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        return true;
    }

    std::atomic<std::uint64_t> droppedChords_{0};
};

}  // namespace drumas::bridge
