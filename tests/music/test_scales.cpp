#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>

#include "music/Scale.h"

namespace {

using drumas::music::ChordQuality;
using drumas::music::PitchClass;
using drumas::music::Scale;
using drumas::music::ScaleType;

void requireScale(const Scale& scale, const std::array<int, 7>& expectedSemitones) {
    const auto actual = drumas::music::scaleNotes(scale);
    REQUIRE(actual.has_value());
    for (std::size_t degree = 0; degree < expectedSemitones.size(); ++degree) {
        REQUIRE(drumas::music::semitones(actual->notes[degree]) == expectedSemitones[degree]);
    }
}

}  // namespace

TEST_CASE("major and natural minor scales honor sharp and flat tonics",
          "[music][scale]") {
    requireScale(Scale{PitchClass::C, ScaleType::Major}, {0, 2, 4, 5, 7, 9, 11});
    requireScale(Scale{PitchClass::FSharp, ScaleType::Major}, {6, 8, 10, 11, 1, 3, 5});
    requireScale(Scale{PitchClass::DFlat, ScaleType::Major}, {1, 3, 5, 6, 8, 10, 0});
    requireScale(Scale{PitchClass::BFlat, ScaleType::Major}, {10, 0, 2, 3, 5, 7, 9});
    requireScale(Scale{PitchClass::A, ScaleType::NaturalMinor}, {9, 11, 0, 2, 4, 5, 7});
    requireScale(Scale{PitchClass::CSharp, ScaleType::NaturalMinor}, {1, 3, 4, 6, 8, 9, 11});

    const auto spelled = drumas::music::scaleNotes(Scale{PitchClass::FSharp, ScaleType::Major});
    REQUIRE(spelled.has_value());
    REQUIRE(drumas::music::name(spelled->notes[6]) == "E#");
    const auto flatSpelling =
        drumas::music::scaleNotes(Scale{PitchClass::BFlat, ScaleType::Major});
    REQUIRE(flatSpelling.has_value());
    REQUIRE(drumas::music::name(flatSpelling->notes[0]) == "Bb");
    REQUIRE(drumas::music::name(flatSpelling->notes[3]) == "Eb");
}

TEST_CASE("major diatonic triads and sevenths are named by degree", "[music][scale][chord]") {
    constexpr std::array<ChordQuality, 7> triads{
        ChordQuality::Major, ChordQuality::Minor, ChordQuality::Minor,
        ChordQuality::Major, ChordQuality::Major, ChordQuality::Minor,
        ChordQuality::Diminished};
    constexpr std::array<ChordQuality, 7> sevenths{
        ChordQuality::Major7, ChordQuality::Minor7, ChordQuality::Minor7,
        ChordQuality::Major7, ChordQuality::Dominant7, ChordQuality::Minor7,
        ChordQuality::HalfDiminished7};
    constexpr std::array<std::string_view, 7> numerals{
        "I", "ii", "iii", "IV", "V", "vi", "vii°"};

    const auto triadSet =
        drumas::music::diatonicChords(Scale{PitchClass::C, ScaleType::Major});
    const auto seventhSet = drumas::music::diatonicChords(
        Scale{PitchClass::C, ScaleType::Major}, drumas::music::DiatonicChordType::Seventh);
    REQUIRE(triadSet.has_value());
    REQUIRE(seventhSet.has_value());
    for (std::size_t degree = 0; degree < triads.size(); ++degree) {
        REQUIRE((*triadSet)[degree].degree == degree + 1);
        REQUIRE((*triadSet)[degree].numeral == numerals[degree]);
        REQUIRE((*triadSet)[degree].chord.quality == triads[degree]);
        REQUIRE((*seventhSet)[degree].chord.quality == sevenths[degree]);
    }
}

TEST_CASE("natural minor diatonic chords use their proper sevenths",
          "[music][scale][chord]") {
    constexpr std::array<ChordQuality, 7> triads{
        ChordQuality::Minor, ChordQuality::Diminished, ChordQuality::Major,
        ChordQuality::Minor, ChordQuality::Minor, ChordQuality::Major,
        ChordQuality::Major};
    constexpr std::array<ChordQuality, 7> sevenths{
        ChordQuality::Minor7, ChordQuality::HalfDiminished7, ChordQuality::Major7,
        ChordQuality::Minor7, ChordQuality::Minor7, ChordQuality::Major7,
        ChordQuality::Dominant7};
    constexpr std::array<std::string_view, 7> numerals{
        "i", "ii°", "III", "iv", "v", "VI", "VII"};

    const Scale aMinor{PitchClass::A, ScaleType::NaturalMinor};
    const auto triadSet = drumas::music::diatonicChords(aMinor);
    const auto seventhSet = drumas::music::diatonicChords(
        aMinor, drumas::music::DiatonicChordType::Seventh);
    REQUIRE(triadSet.has_value());
    REQUIRE(seventhSet.has_value());
    for (std::size_t degree = 0; degree < triads.size(); ++degree) {
        REQUIRE((*triadSet)[degree].numeral == numerals[degree]);
        REQUIRE((*triadSet)[degree].chord.quality == triads[degree]);
        REQUIRE((*seventhSet)[degree].chord.quality == sevenths[degree]);
    }
}

TEST_CASE("invalid scale values are rejected", "[music][scale]") {
    REQUIRE_FALSE(drumas::music::scaleNotes(
                      Scale{static_cast<PitchClass>(255), ScaleType::Major})
                      .has_value());
    REQUIRE_FALSE(drumas::music::scaleNotes(
                      Scale{PitchClass::C, static_cast<ScaleType>(255)})
                      .has_value());
}
