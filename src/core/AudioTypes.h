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
    // A whole chord as one event: the pool allocates every note before any of
    // them starts, so a chord is always complete and sample-aligned. `group`
    // identifies the pressed pad so ChordOff releases exactly that chord.
    ChordOn,
    ChordOff,
    SetParam,
    SelectPatch,
    AllNotesOff,
    // Effect-chain commands (stage 5). The effect object pointer is an opaque
    // address built on the control thread; the audio thread only adopts it.
    SwapEffect,
    SetEffectBypass,
    SetEffectMix,
    SetEffectParam,
    // User patch from a JSON file. Like SwapEffect, the pointer is prepared on
    // the control thread; the audio thread copies the data and returns the
    // pointer through a retirement queue so the control thread can free it.
    SelectPatchData,
};

/// One event, small enough to travel through a lock-free queue by value.
/// Trivially copyable is a hard requirement of core::SpscQueue. `param` is a
/// stable 32-bit id from core::ParameterRegistry; 0 means "no parameter".
/// A chord never has more than four notes (triads and seventh chords, with or
/// without an inverted voicing).
inline constexpr std::uint8_t kMaxChordNotes = 4;
/// Group 0 means "not part of a chord" (piano keys, console notes).
inline constexpr std::uint8_t kMaxChordGroup = 31;

struct Command {
    CommandType type = CommandType::None;
    ParameterId param = kInvalidParameterId;
    std::uint8_t note = 0;
    std::uint8_t patch = 0;
    float value = 0.0F;
    /// Effect slot (0..5). Ignored by synth commands.
    std::uint8_t slot = 0;
    /// Opaque effect pointer for SwapEffect; zero for every other command.
    std::uintptr_t object = 0;
    /// Chord group (1..kMaxChordGroup); 0 for ungrouped notes.
    std::uint8_t group = 0;
    /// Notes in a ChordOn, one byte each, low byte first.
    std::uint32_t notes = 0;
    /// Number of valid notes in `notes` for ChordOn.
    std::uint8_t noteCount = 0;
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

/// Hands ownership of a heap-allocated Patch to the audio thread. The control
/// thread must keep it alive until the processor returns it through its patch
/// retirement queue; it must never delete it after a successful push.
[[nodiscard]] constexpr Command selectPatchData(std::uintptr_t patch) noexcept {
    return Command{CommandType::SelectPatchData, kInvalidParameterId, 0, 0, 0.0F, 0,
                   patch};
}

/// Packs up to kMaxChordNotes MIDI notes into a ChordOn payload. A longer list
/// is rejected by the caller, never cut into a half chord.
[[nodiscard]] constexpr std::uint32_t packChordPayload(const std::uint8_t* notes,
                                                       std::size_t count) noexcept {
    std::uint32_t packed = 0;
    const std::size_t limit = count > kMaxChordNotes ? kMaxChordNotes : count;
    for (std::size_t index = 0; index < limit; ++index) {
        packed |= static_cast<std::uint32_t>(notes[index]) << (8U * index);
    }
    return packed;
}

[[nodiscard]] constexpr std::uint8_t chordNoteAt(std::uint32_t packed,
                                                 std::uint8_t index) noexcept {
    return static_cast<std::uint8_t>((packed >> (8U * index)) & 0xFFU);
}

/// Starts a whole chord in one command. `group` must be 1..kMaxChordGroup; the
/// audio thread allocates all notes before starting any of them.
[[nodiscard]] constexpr Command chordOn(std::uint8_t group, const std::uint8_t* notes,
                                        std::size_t count,
                                        float velocity = 1.0F) noexcept {
    return Command{CommandType::ChordOn, kInvalidParameterId, 0, 0, velocity, 0, 0, group,
                   packChordPayload(notes, count),
                   static_cast<std::uint8_t>(count > kMaxChordNotes ? kMaxChordNotes
                                                                   : count)};
}

/// Releases every voice still owned by `group`. Notes shared with another held
/// chord keep sounding: only this group's claim is dropped.
[[nodiscard]] constexpr Command chordOff(std::uint8_t group) noexcept {
    return Command{CommandType::ChordOff, kInvalidParameterId, 0, 0, 0.0F, 0, 0, group, 0, 0};
}

[[nodiscard]] constexpr Command allNotesOff() noexcept {
    return Command{CommandType::AllNotesOff, kInvalidParameterId, 0, 0, 0.0F};
}

/// Hands ownership of a prepared effect to the audio thread. The control thread
/// must keep the object alive until the chain returns it through its retirement
/// queue; it must never delete it after a successful push.
[[nodiscard]] constexpr Command swapEffect(std::uint8_t slot, std::uintptr_t effect) noexcept {
    return Command{CommandType::SwapEffect, kInvalidParameterId, 0, 0, 0.0F, slot, effect};
}

[[nodiscard]] constexpr Command setEffectBypass(std::uint8_t slot, bool bypassed) noexcept {
    return Command{CommandType::SetEffectBypass, kInvalidParameterId, 0, 0,
                   bypassed ? 1.0F : 0.0F, slot, 0};
}

[[nodiscard]] constexpr Command setEffectMix(std::uint8_t slot, float wet) noexcept {
    return Command{CommandType::SetEffectMix, kInvalidParameterId, 0, 0, wet, slot, 0};
}

[[nodiscard]] constexpr Command setEffectParam(std::uint8_t slot, ParameterId id,
                                               float value) noexcept {
    return Command{CommandType::SetEffectParam, id, 0, 0, value, slot, 0};
}

/// Equal-tempered conversion from MIDI note number to frequency in Hz.
/// Not constexpr: std::pow is not constexpr until C++26. Cheap enough to be
/// called on note-on (control side of the audio thread), never per sample.
[[nodiscard]] inline float noteToFrequency(std::uint8_t note) noexcept {
    const int semitones = static_cast<int>(note) - static_cast<int>(kMidiNoteA4);
    return kFrequencyA4 * std::pow(2.0F, static_cast<float>(semitones) / 12.0F);
}

}  // namespace drumas::core
