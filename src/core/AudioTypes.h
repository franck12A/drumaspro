// Audio-level types shared by the engine, the synth and the effects.
//
// Everything in this header is a plain value type: no allocation, no virtual
// calls, no thread ownership. See docs/rt-safety.md for the rules these types
// exist to support.
#pragma once

#include <cmath>
#include <cstdint>
#include <type_traits>

#include "core/ParameterRegistry.h"

namespace drumas::core {

/// Sample format used end to end. The device is opened as 32-bit float, so no
/// conversion is needed between the callback buffer and the DSP code.
using Sample = float;

/// Musical and MIDI convention: 69 == A4 == 440 Hz.
inline constexpr std::uint8_t kMidiNoteA4 = 69;
inline constexpr float kFrequencyA4 = 440.0F;
inline constexpr std::uint8_t kMidiNoteMax = 127;

inline constexpr double kDefaultSampleRate = 48000.0;
inline constexpr int kDefaultChannels = 2;

/// Structural description of a processing stream. Fixed for the lifetime of a
/// prepared processor; changing it requires prepare() again (setup thread only).
struct AudioSpec {
    double sampleRate = kDefaultSampleRate;
    int maxBlockSize = 512;
    int channels = kDefaultChannels;
};

/// UI/control thread -> audio thread. Everything that describes "what the
/// engine should be doing" is one of these, never a shared mutable object.
enum class CommandType : std::uint8_t {
    None = 0,
    NoteOn,
    NoteOff,
    SetParam,
    SelectPatch,
    AllNotesOff,
};

/// One event, small enough to travel through a lock-free queue by value.
/// Trivially copyable is a hard requirement of core::SpscQueue. `param` is a
/// stable 32-bit id from core::ParameterRegistry; 0 means "no parameter".
struct Command {
    CommandType type = CommandType::None;
    ParameterId param = kInvalidParameterId;
    std::uint8_t note = 0;
    std::uint8_t patch = 0;
    float value = 0.0F;
};

static_assert(std::is_trivially_copyable_v<Command>,
              "Command must stay trivially copyable: it crosses a lock-free queue");

[[nodiscard]] constexpr Command noteOn(std::uint8_t note, float velocity = 1.0F) noexcept {
    return Command{CommandType::NoteOn, kInvalidParameterId, note, 0, velocity};
}

[[nodiscard]] constexpr Command noteOff(std::uint8_t note) noexcept {
    return Command{CommandType::NoteOff, kInvalidParameterId, note, 0, 0.0F};
}

[[nodiscard]] constexpr Command setParam(ParameterId id, float value) noexcept {
    return Command{CommandType::SetParam, id, 0, 0, value};
}

/// Selects a factory patch by index. Voices already sounding keep the patch
/// they captured; the new patch applies to the next note-on.
[[nodiscard]] constexpr Command selectPatch(std::uint8_t patchIndex) noexcept {
    return Command{CommandType::SelectPatch, kInvalidParameterId, 0, patchIndex, 0.0F};
}

[[nodiscard]] constexpr Command allNotesOff() noexcept {
    return Command{CommandType::AllNotesOff, kInvalidParameterId, 0, 0, 0.0F};
}

/// Equal-tempered conversion from MIDI note number to frequency in Hz.
/// Not constexpr: std::pow is not constexpr until C++26. Cheap enough to be
/// called on note-on (control side of the audio thread), never per sample.
[[nodiscard]] inline float noteToFrequency(std::uint8_t note) noexcept {
    const int semitones = static_cast<int>(note) - static_cast<int>(kMidiNoteA4);
    return kFrequencyA4 * std::pow(2.0F, static_cast<float>(semitones) / 12.0F);
}

}  // namespace drumas::core
