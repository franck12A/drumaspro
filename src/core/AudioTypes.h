// Audio-level types shared by the engine, the synth and the effects.
//
// Everything in this header is a plain value type: no allocation, no virtual
// calls, no thread ownership. See docs/rt-safety.md for the rules these types
// exist to support.
#pragma once

#include <cmath>
#include <cstdint>

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
    AllNotesOff,
};

/// Parameter ids understood by the synth. Effects will add their own ranges in
/// later stages; an id is just a tag carried in a Command.
enum class ParamId : std::uint8_t {
    None = 0,
    MasterGain,  // linear gain, 0..1
    AttackMs,
    DecayMs,
    Sustain,  // 0..1
    ReleaseMs,
};

/// One event, small enough to travel through a lock-free queue by value.
/// Trivially copyable is a hard requirement of core::SpscQueue.
struct Command {
    CommandType type = CommandType::None;
    ParamId param = ParamId::None;
    std::uint8_t note = 0;
    float value = 0.0F;
};

[[nodiscard]] constexpr Command noteOn(std::uint8_t note, float velocity = 1.0F) noexcept {
    return Command{CommandType::NoteOn, ParamId::None, note, velocity};
}

[[nodiscard]] constexpr Command noteOff(std::uint8_t note) noexcept {
    return Command{CommandType::NoteOff, ParamId::None, note, 0.0F};
}

[[nodiscard]] constexpr Command setParam(ParamId id, float value) noexcept {
    return Command{CommandType::SetParam, id, 0, value};
}

[[nodiscard]] constexpr Command allNotesOff() noexcept {
    return Command{CommandType::AllNotesOff, ParamId::None, 0, 0.0F};
}

/// Equal-tempered conversion from MIDI note number to frequency in Hz.
/// Not constexpr: std::pow is not constexpr until C++26. Cheap enough to be
/// called on note-on (control side of the audio thread), never per sample.
[[nodiscard]] inline float noteToFrequency(std::uint8_t note) noexcept {
    const int semitones = static_cast<int>(note) - static_cast<int>(kMidiNoteA4);
    return kFrequencyA4 * std::pow(2.0F, static_cast<float>(semitones) / 12.0F);
}

}  // namespace drumas::core
