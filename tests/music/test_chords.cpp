#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <optional>

#include "music/Chord.h"

namespace {

using drumas::music::Chord;
using drumas::music::ChordQuality;
using drumas::music::PitchClass;

struct QualityCase {
    ChordQuality quality;
    std::array<std::uint8_t, 4> intervals;
    std::uint8_t count;
    std::string_view label;
};

constexpr std::array<QualityCase, 10> kQualities{{
    {ChordQuality::Major, {0, 4, 7, 0}, 3, "major"},
    {ChordQuality::Minor, {0, 3, 7, 0}, 3, "minor"},
    {ChordQuality::Diminished, {0, 3, 6, 0}, 3, "diminished"},
    {ChordQuality::Augmented, {0, 4, 8, 0}, 3, "augmented"},
    {ChordQuality::Dominant7, {0, 4, 7, 10}, 4, "dominant 7"},
    {ChordQuality::Major7, {0, 4, 7, 11}, 4, "major 7"},
    {ChordQuality::Minor7, {0, 3, 7, 10}, 4, "minor 7"},
    {ChordQuality::Sus2, {0, 2, 7, 0}, 3, "sus2"},
    {ChordQuality::Sus4, {0, 5, 7, 0}, 3, "sus4"},
    {ChordQuality::HalfDiminished7, {0, 3, 6, 10}, 4, "half-diminished 7"},
}};

constexpr std::array<PitchClass, 12> kPitchClasses{
    PitchClass::C, PitchClass::CSharp, PitchClass::D, PitchClass::DSharp,
    PitchClass::E, PitchClass::F, PitchClass::FSharp, PitchClass::G,
    PitchClass::GSharp, PitchClass::A, PitchClass::ASharp, PitchClass::B};

}  // namespace

TEST_CASE("every chord quality has its named interval set", "[music][chord]") {
    for (const QualityCase& expected : kQualities) {
        const auto actual = drumas::music::intervals(expected.quality);
        REQUIRE(actual.count == expected.count);
        REQUIRE(actual.label == expected.label);
        for (std::size_t index = 0; index < expected.count; ++index) {
            REQUIRE(actual.semitones[index] == expected.intervals[index]);
        }
    }
}

TEST_CASE("chord inversions rotate tones upward by octaves", "[music][chord]") {
    const auto major = drumas::music::chordToMidiNotes(
        Chord{PitchClass::C, ChordQuality::Major, 0, 4});
    REQUIRE(major.has_value());
    REQUIRE(major->count == 3);
    REQUIRE(major->notes == std::array<std::uint8_t, 4>{60, 64, 67, 0});

    const auto firstInversion = drumas::music::chordToMidiNotes(
        Chord{PitchClass::C, ChordQuality::Major, 1, 4});
    REQUIRE(firstInversion.has_value());
    REQUIRE(firstInversion->notes == std::array<std::uint8_t, 4>{64, 67, 72, 0});

    const auto secondInversion = drumas::music::chordToMidiNotes(
        Chord{PitchClass::C, ChordQuality::Major, 2, 4});
    REQUIRE(secondInversion.has_value());
    REQUIRE(secondInversion->notes == std::array<std::uint8_t, 4>{67, 72, 76, 0});

    const auto seventhInversion = drumas::music::chordToMidiNotes(
        Chord{PitchClass::C, ChordQuality::Major7, 3, 4});
    REQUIRE(seventhInversion.has_value());
    REQUIRE(seventhInversion->notes == std::array<std::uint8_t, 4>{71, 72, 76, 79});
}

TEST_CASE("chord MIDI conversion rejects invalid voicings without clipping",
          "[music][chord][midi]") {
    const auto lowest = drumas::music::chordToMidiNotes(
        Chord{PitchClass::C, ChordQuality::Major, 0, -1});
    REQUIRE(lowest.has_value());
    REQUIRE(lowest->notes[0] == 0);

    const auto highest = drumas::music::chordToMidiNotes(
        Chord{PitchClass::C, ChordQuality::Major, 0, 9});
    REQUIRE(highest.has_value());
    REQUIRE(highest->notes[2] == 127);

    REQUIRE_FALSE(drumas::music::chordToMidiNotes(
                      Chord{PitchClass::C, ChordQuality::Dominant7, 0, 9})
                      .has_value());
    REQUIRE_FALSE(drumas::music::chordToMidiNotes(
                      Chord{PitchClass::C, ChordQuality::Major, 3, 4})
                      .has_value());
    REQUIRE_FALSE(drumas::music::chordToMidiNotes(
                      Chord{static_cast<PitchClass>(255), ChordQuality::Major, 0, 4})
                      .has_value());
    REQUIRE_FALSE(drumas::music::chordToMidiNotes(
                      Chord{PitchClass::C, static_cast<ChordQuality>(255), 0, 4})
                      .has_value());
}

TEST_CASE("all qualities, inversions, roots and octaves obey MIDI bounds",
          "[music][chord][midi][exhaustive]") {
    for (const QualityCase& quality : kQualities) {
        for (const PitchClass root : kPitchClasses) {
            for (int octave = -2; octave <= 10; ++octave) {
                for (std::uint8_t inversion = 0; inversion < quality.count; ++inversion) {
                    const Chord chord{root, quality.quality, inversion, octave};
                    const auto actual = drumas::music::chordToMidiNotes(chord);
                    std::array<int, 4> expected{};
                    bool inRange = true;
                    const int rootMidi = (octave + 1) * 12 +
                                         drumas::music::semitones(root);
                    for (std::size_t index = 0; index < quality.count; ++index) {
                        const std::size_t source =
                            (index + inversion) % quality.count;
                        const bool raised = index + inversion >= quality.count;
                        expected[index] = rootMidi + quality.intervals[source] +
                                          (raised ? 12 : 0);
                        inRange = inRange && expected[index] >= 0 && expected[index] <= 127;
                    }

                    REQUIRE(actual.has_value() == inRange);
                    if (actual.has_value()) {
                        REQUIRE(actual->count == quality.count);
                        for (std::size_t index = 0; index < quality.count; ++index) {
                            REQUIRE(actual->notes[index] == expected[index]);
                        }
                    }
                }
            }
        }
    }
}

TEST_CASE("chord names retain enharmonic root spelling", "[music][chord]") {
    const auto flat = drumas::music::chordName(
        Chord{PitchClass::DFlat, ChordQuality::Major, 0, 4});
    const auto sharp = drumas::music::chordName(
        Chord{PitchClass::CSharp, ChordQuality::Major, 0, 4});
    REQUIRE(flat.has_value());
    REQUIRE(sharp.has_value());
    REQUIRE(flat->root == "Db");
    REQUIRE(sharp->root == "C#");
    REQUIRE(flat->quality == "major");
}
