// Chord definitions and MIDI note expansion. No audio or UI dependencies.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string_view>

#include "music/MusicTypes.h"

namespace drumas::music {

enum class ChordQuality : std::uint8_t {
    Major,
    Minor,
    Diminished,
    Augmented,
    Dominant7,
    Major7,
    Minor7,
    Sus2,
    Sus4,
    HalfDiminished7,
};

struct ChordIntervals {
    std::array<std::uint8_t, 4> semitones{};
    std::uint8_t count = 0;
    std::string_view label;
};

[[nodiscard]] constexpr ChordIntervals intervals(ChordQuality quality) noexcept {
    switch (quality) {
        case ChordQuality::Major: return {{0, 4, 7, 0}, 3, "major"};
        case ChordQuality::Minor: return {{0, 3, 7, 0}, 3, "minor"};
        case ChordQuality::Diminished: return {{0, 3, 6, 0}, 3, "diminished"};
        case ChordQuality::Augmented: return {{0, 4, 8, 0}, 3, "augmented"};
        case ChordQuality::Dominant7: return {{0, 4, 7, 10}, 4, "dominant 7"};
        case ChordQuality::Major7: return {{0, 4, 7, 11}, 4, "major 7"};
        case ChordQuality::Minor7: return {{0, 3, 7, 10}, 4, "minor 7"};
        case ChordQuality::Sus2: return {{0, 2, 7, 0}, 3, "sus2"};
        case ChordQuality::Sus4: return {{0, 5, 7, 0}, 3, "sus4"};
        case ChordQuality::HalfDiminished7:
            return {{0, 3, 6, 10}, 4, "half-diminished 7"};
    }
    return {};
}

struct Chord {
    PitchClass root = PitchClass::C;
    ChordQuality quality = ChordQuality::Major;
    /// Root position is 0; each inversion moves one more chord tone up an octave.
    std::uint8_t inversion = 0;
    /// MIDI octave convention: C4 is MIDI note 60; C-1 is MIDI note 0.
    int octave = 4;
};

struct MidiNotes {
    std::array<std::uint8_t, 4> notes{};
    std::uint8_t count = 0;
};

struct ChordName {
    std::string_view root;
    std::string_view quality;
};

[[nodiscard]] constexpr std::optional<ChordName> chordName(const Chord& chord) noexcept {
    const ChordIntervals chordIntervals = intervals(chord.quality);
    const std::string_view rootName = name(chord.root);
    if (chordIntervals.count == 0 || rootName.empty()) {
        return std::nullopt;
    }
    return ChordName{rootName, chordIntervals.label};
}

/// Expands a chord in ascending voicing order. Invalid roots, qualities,
/// inversions and any voicing crossing MIDI 0..127 are rejected as a whole;
/// notes are never clamped or wrapped.
[[nodiscard]] constexpr std::optional<MidiNotes> chordToMidiNotes(
    const Chord& chord) noexcept {
    const ChordIntervals chordIntervals = intervals(chord.quality);
    const int rootSemitones = semitones(chord.root);
    if (chordIntervals.count == 0 || rootSemitones < 0 ||
        chord.inversion >= chordIntervals.count) {
        return std::nullopt;
    }

    const std::int64_t rootMidi =
        (static_cast<std::int64_t>(chord.octave) + 1) * 12 + rootSemitones;
    MidiNotes result;
    result.count = chordIntervals.count;

    for (std::size_t index = 0; index < result.count; ++index) {
        const std::size_t source =
            (index + static_cast<std::size_t>(chord.inversion)) % result.count;
        const bool raised = index + static_cast<std::size_t>(chord.inversion) >= result.count;
        const std::int64_t note = rootMidi + chordIntervals.semitones[source] +
                                  (raised ? 12 : 0);
        if (note < 0 || note > std::numeric_limits<std::uint8_t>::max() ||
            note > 127) {
            return std::nullopt;
        }
        result.notes[index] = static_cast<std::uint8_t>(note);
    }
    return result;
}

}  // namespace drumas::music
