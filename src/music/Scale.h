// Diatonic major and natural-minor scales and their stacked-third chords.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

#include "music/Chord.h"

namespace drumas::music {

enum class ScaleType : std::uint8_t {
    Major,
    NaturalMinor,
};

struct Scale {
    PitchClass tonic = PitchClass::C;
    ScaleType type = ScaleType::Major;
};

struct ScaleNotes {
    std::array<PitchClass, 7> notes{};
};

enum class DiatonicChordType : std::uint8_t {
    Triad,
    Seventh,
};

struct DiatonicChord {
    Chord chord;
    std::uint8_t degree = 0;
    std::string_view numeral;
};

namespace detail {

inline constexpr std::array<int, 7> kNaturalPitchClasses{0, 2, 4, 5, 7, 9, 11};
inline constexpr std::array<int, 7> kMajorScale{0, 2, 4, 5, 7, 9, 11};
inline constexpr std::array<int, 7> kNaturalMinorScale{0, 2, 3, 5, 7, 8, 10};

[[nodiscard]] constexpr int letterIndex(PitchClass pitchClass) noexcept {
    switch (pitchClass) {
        case PitchClass::C:
        case PitchClass::CSharp:
        case PitchClass::BSharp: return 0;
        case PitchClass::DFlat:
        case PitchClass::D:
        case PitchClass::DSharp: return 1;
        case PitchClass::EFlat:
        case PitchClass::E:
        case PitchClass::FFlat: return 2;
        case PitchClass::ESharp:
        case PitchClass::F:
        case PitchClass::FSharp: return 3;
        case PitchClass::GFlat:
        case PitchClass::G:
        case PitchClass::GSharp: return 4;
        case PitchClass::AFlat:
        case PitchClass::A:
        case PitchClass::ASharp: return 5;
        case PitchClass::BFlat:
        case PitchClass::B:
        case PitchClass::CFlat: return 6;
    }
    return -1;
}

[[nodiscard]] constexpr PitchClass spelledPitchClass(int letter, int accidental) noexcept {
    switch (letter) {
        case 0:
            if (accidental == 0) return PitchClass::C;
            if (accidental == 1) return PitchClass::CSharp;
            if (accidental == -1) return PitchClass::CFlat;
            break;
        case 1:
            if (accidental == 0) return PitchClass::D;
            if (accidental == 1) return PitchClass::DSharp;
            if (accidental == -1) return PitchClass::DFlat;
            break;
        case 2:
            if (accidental == 0) return PitchClass::E;
            if (accidental == 1) return PitchClass::ESharp;
            if (accidental == -1) return PitchClass::EFlat;
            break;
        case 3:
            if (accidental == 0) return PitchClass::F;
            if (accidental == 1) return PitchClass::FSharp;
            if (accidental == -1) return PitchClass::FFlat;
            break;
        case 4:
            if (accidental == 0) return PitchClass::G;
            if (accidental == 1) return PitchClass::GSharp;
            if (accidental == -1) return PitchClass::GFlat;
            break;
        case 5:
            if (accidental == 0) return PitchClass::A;
            if (accidental == 1) return PitchClass::ASharp;
            if (accidental == -1) return PitchClass::AFlat;
            break;
        case 6:
            if (accidental == 0) return PitchClass::B;
            if (accidental == 1) return PitchClass::BSharp;
            if (accidental == -1) return PitchClass::BFlat;
            break;
    }
    return static_cast<PitchClass>(255);
}

[[nodiscard]] constexpr PitchClass canonicalPitchClass(int semitone) noexcept {
    switch ((semitone % 12 + 12) % 12) {
        case 0: return PitchClass::C;
        case 1: return PitchClass::CSharp;
        case 2: return PitchClass::D;
        case 3: return PitchClass::DSharp;
        case 4: return PitchClass::E;
        case 5: return PitchClass::F;
        case 6: return PitchClass::FSharp;
        case 7: return PitchClass::G;
        case 8: return PitchClass::GSharp;
        case 9: return PitchClass::A;
        case 10: return PitchClass::ASharp;
        case 11: return PitchClass::B;
    }
    return PitchClass::C;
}

[[nodiscard]] constexpr const std::array<int, 7>* scaleIntervals(ScaleType type) noexcept {
    switch (type) {
        case ScaleType::Major: return &kMajorScale;
        case ScaleType::NaturalMinor: return &kNaturalMinorScale;
    }
    return nullptr;
}

}  // namespace detail

[[nodiscard]] constexpr std::optional<ScaleNotes> scaleNotes(const Scale& scale) noexcept {
    const int tonicSemitone = semitones(scale.tonic);
    const int tonicLetter = detail::letterIndex(scale.tonic);
    const auto* intervals = detail::scaleIntervals(scale.type);
    if (tonicSemitone < 0 || tonicLetter < 0 || intervals == nullptr) {
        return std::nullopt;
    }

    ScaleNotes result;
    for (std::size_t degree = 0; degree < result.notes.size(); ++degree) {
        const int pitch = (tonicSemitone + (*intervals)[degree]) % 12;
        const int letter = (tonicLetter + static_cast<int>(degree)) % 7;
        const int natural = detail::kNaturalPitchClasses[static_cast<std::size_t>(letter)];
        int accidental = (pitch - natural + 12) % 12;
        if (accidental == 11) {
            accidental = -1;
        } else if (accidental > 1) {
            result.notes[degree] = detail::canonicalPitchClass(pitch);
            continue;
        }

        result.notes[degree] = detail::spelledPitchClass(letter, accidental);
        if (semitones(result.notes[degree]) < 0) {
            result.notes[degree] = detail::canonicalPitchClass(pitch);
        }
    }
    return result;
}

[[nodiscard]] constexpr std::optional<std::array<DiatonicChord, 7>> diatonicChords(
    const Scale& scale, DiatonicChordType chordType = DiatonicChordType::Triad) noexcept {
    const auto notes = scaleNotes(scale);
    if (!notes.has_value()) {
        return std::nullopt;
    }

    constexpr std::array<ChordQuality, 7> kMajorTriads{
        ChordQuality::Major, ChordQuality::Minor, ChordQuality::Minor, ChordQuality::Major,
        ChordQuality::Major, ChordQuality::Minor, ChordQuality::Diminished};
    constexpr std::array<ChordQuality, 7> kMinorTriads{
        ChordQuality::Minor, ChordQuality::Diminished, ChordQuality::Major,
        ChordQuality::Minor, ChordQuality::Minor, ChordQuality::Major, ChordQuality::Major};
    constexpr std::array<ChordQuality, 7> kMajorSevenths{
        ChordQuality::Major7, ChordQuality::Minor7, ChordQuality::Minor7, ChordQuality::Major7,
        ChordQuality::Dominant7, ChordQuality::Minor7, ChordQuality::HalfDiminished7};
    constexpr std::array<ChordQuality, 7> kMinorSevenths{
        ChordQuality::Minor7, ChordQuality::HalfDiminished7, ChordQuality::Major7,
        ChordQuality::Minor7, ChordQuality::Minor7, ChordQuality::Major7, ChordQuality::Dominant7};
    constexpr std::array<std::string_view, 7> kMajorNumerals{
        "I", "ii", "iii", "IV", "V", "vi", "vii°"};
    constexpr std::array<std::string_view, 7> kMinorNumerals{
        "i", "ii°", "III", "iv", "v", "VI", "VII"};

    const bool major = scale.type == ScaleType::Major;
    const auto* qualities = chordType == DiatonicChordType::Triad
                                ? (major ? &kMajorTriads : &kMinorTriads)
                                : (major ? &kMajorSevenths : &kMinorSevenths);
    const auto& numerals = major ? kMajorNumerals : kMinorNumerals;

    std::array<DiatonicChord, 7> result{};
    for (std::size_t degree = 0; degree < result.size(); ++degree) {
        result[degree] = DiatonicChord{
            Chord{notes->notes[degree], (*qualities)[degree], 0, 4},
            static_cast<std::uint8_t>(degree + 1), numerals[degree]};
    }
    return result;
}

}  // namespace drumas::music
