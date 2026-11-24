// A patch is a set of values for the single voice architecture: two
// oscillators, sub, noise, unison, filter, two envelopes, velocity response and
// LFO routing.
//
// It is plain data on purpose: trivially copyable (so a voice can capture it on
// note-on without allocating), and every field maps to a stable parameter id
// from ParameterIds.h, which is what a JSON preset will serialise in stage 8.
// Adding an instrument means adding an entry to the patch bank, not a class.
#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

#include "core/AudioTypes.h"
#include "synth/IEnvelope.h"
#include "synth/IOscillator.h"

namespace drumas::synth {

/// LFO shapes. Values are part of the `lfo.shape` parameter contract.
enum class LfoShape : std::uint8_t {
    Sine = 0,
    Triangle = 1,
    Saw = 2,
    Square = 3,
};

struct Patch {
    std::string_view name{};

    // Oscillator 1.
    Waveform osc1Waveform = Waveform::Saw;
    float osc1Level = 0.5F;
    float osc1DetuneCents = 0.0F;
    float osc1PulseWidth = 0.5F;

    // Oscillator 2.
    Waveform osc2Waveform = Waveform::Saw;
    float osc2Level = 0.5F;
    float osc2DetuneCents = 0.0F;
    float osc2PulseWidth = 0.5F;

    float subLevel = 0.0F;    // sine one octave below, 0..1
    float noiseLevel = 0.0F;  // white noise, 0..1

    int unisonVoices = 1;          // 1..4 layers per oscillator
    float unisonDetuneCents = 0.0F;  // total spread, +/- this value
    float unisonStereo = 0.0F;       // 0 = mono, 1 = full stereo spread

    float cutoffHz = 5000.0F;
    float resonance = 0.1F;         // 0..1 (maps to Q 0.5..50)
    float filterEnvOctaves = 2.0F;  // filter envelope -> cutoff, in octaves

    AdsrParameters ampEnvelope{};
    AdsrParameters filterEnvelope{};

    float velocityVolume = 0.7F;      // 0 = ignore velocity, 1 = full scaling
    float velocityBrightness = 0.0F;  // 0..1 -> up to +2 octaves at velocity 1

    LfoShape lfoShape = LfoShape::Sine;
    float lfoRateHz = 5.0F;
    float lfoCutoffDepth = 0.0F;   // 0..1 -> up to +/-2 octaves
    float lfoPitchDepthCents = 0.0F;
};

static_assert(std::is_trivially_copyable_v<Patch>,
              "a voice captures a Patch on note-on; it must be trivially copyable");

/// Parameters that a SetParam command can change on every sounding voice, on
/// top of the patch each voice captured. A patch switch resets these to the new
/// patch's values for future voices but leaves sounding voices alone; see
/// docs/adr/0007-patch-switching-and-live-parameters.md.
struct LiveParameters {
    float cutoffHz = 5000.0F;
    float resonance = 0.1F;
    AdsrParameters ampEnvelope{};
    AdsrParameters filterEnvelope{};
};

[[nodiscard]] inline LiveParameters liveParametersFrom(const Patch& patch) noexcept {
    LiveParameters live;
    live.cutoffHz = patch.cutoffHz;
    live.resonance = patch.resonance;
    live.ampEnvelope = patch.ampEnvelope;
    live.filterEnvelope = patch.filterEnvelope;
    return live;
}

}  // namespace drumas::synth
