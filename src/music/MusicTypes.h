// Value types shared by the pure music-theory helpers.
#pragma once

#include <cstdint>
#include <string_view>

namespace drumas::music {

/// Enharmonic spellings share a semitone but retain the requested note name.
enum class PitchClass : std::uint8_t {
    C,
    CSharp,
    DFlat,
    D,
    DSharp,
    EFlat,
    E,
    FFlat,
    ESharp,
    F,
    FSharp,
    GFlat,
    G,
    GSharp,
    AFlat,
    A,
    ASharp,
    BFlat,
    B,
    CFlat,
    BSharp,
};

/// Equal-tempered interval sizes, measured in semitones.
enum class Interval : std::uint8_t {
    Unison = 0,
    MinorSecond = 1,
    MajorSecond = 2,
    MinorThird = 3,
    MajorThird = 4,
    PerfectFourth = 5,
    Tritone = 6,
    PerfectFifth = 7,
    MinorSixth = 8,
    MajorSixth = 9,
    MinorSeventh = 10,
    MajorSeventh = 11,
    Octave = 12,
};

[[nodiscard]] constexpr int semitones(Interval interval) noexcept {
    return static_cast<int>(interval);
}

[[nodiscard]] constexpr int semitones(PitchClass pitchClass) noexcept {
    switch (pitchClass) {
        case PitchClass::C:
        case PitchClass::BSharp:
            return 0;
        case PitchClass::CSharp:
        case PitchClass::DFlat:
            return 1;
        case PitchClass::D:
            return 2;
        case PitchClass::DSharp:
        case PitchClass::EFlat:
            return 3;
        case PitchClass::E:
        case PitchClass::FFlat:
            return 4;
        case PitchClass::ESharp:
        case PitchClass::F:
            return 5;
        case PitchClass::FSharp:
        case PitchClass::GFlat:
            return 6;
        case PitchClass::G:
            return 7;
        case PitchClass::GSharp:
        case PitchClass::AFlat:
            return 8;
        case PitchClass::A:
            return 9;
        case PitchClass::ASharp:
        case PitchClass::BFlat:
            return 10;
        case PitchClass::B:
        case PitchClass::CFlat:
            return 11;
    }
    return -1;
}

[[nodiscard]] constexpr std::string_view name(PitchClass pitchClass) noexcept {
    switch (pitchClass) {
        case PitchClass::C: return "C";
        case PitchClass::CSharp: return "C#";
        case PitchClass::DFlat: return "Db";
        case PitchClass::D: return "D";
        case PitchClass::DSharp: return "D#";
        case PitchClass::EFlat: return "Eb";
        case PitchClass::E: return "E";
        case PitchClass::FFlat: return "Fb";
        case PitchClass::ESharp: return "E#";
        case PitchClass::F: return "F";
        case PitchClass::FSharp: return "F#";
        case PitchClass::GFlat: return "Gb";
        case PitchClass::G: return "G";
        case PitchClass::GSharp: return "G#";
        case PitchClass::AFlat: return "Ab";
        case PitchClass::A: return "A";
        case PitchClass::ASharp: return "A#";
        case PitchClass::BFlat: return "Bb";
        case PitchClass::B: return "B";
        case PitchClass::CFlat: return "Cb";
        case PitchClass::BSharp: return "B#";
    }
    return {};
}

}  // namespace drumas::music
