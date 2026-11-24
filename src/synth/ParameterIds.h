// Stable ids and the registry definition for every synth parameter.
//
// Ids are hashes of the names below and never change once published: they
// travel in SetParam commands, will travel in presets (stage 8) and are what a
// future UI binds to. Adding a parameter means appending an entry here, never
// reordering or renaming an existing one.
#pragma once

#include "core/ParameterRegistry.h"

namespace drumas::synth {

using core::makeParameterId;
using core::ParameterDescriptor;
using core::ParameterId;
using core::ParameterRegistry;

// -- Oscillators -------------------------------------------------------------
inline constexpr ParameterId kOsc1Waveform = makeParameterId("osc1.waveform");
inline constexpr ParameterId kOsc1Level = makeParameterId("osc1.level");
inline constexpr ParameterId kOsc1DetuneCents = makeParameterId("osc1.detune");
inline constexpr ParameterId kOsc1PulseWidth = makeParameterId("osc1.pulseWidth");
inline constexpr ParameterId kOsc2Waveform = makeParameterId("osc2.waveform");
inline constexpr ParameterId kOsc2Level = makeParameterId("osc2.level");
inline constexpr ParameterId kOsc2DetuneCents = makeParameterId("osc2.detune");
inline constexpr ParameterId kOsc2PulseWidth = makeParameterId("osc2.pulseWidth");
inline constexpr ParameterId kSubLevel = makeParameterId("sub.level");
inline constexpr ParameterId kNoiseLevel = makeParameterId("noise.level");

// -- Unison ------------------------------------------------------------------
inline constexpr ParameterId kUnisonVoices = makeParameterId("unison.voices");
inline constexpr ParameterId kUnisonDetuneCents = makeParameterId("unison.detune");
inline constexpr ParameterId kUnisonStereo = makeParameterId("unison.stereo");

// -- Filter ------------------------------------------------------------------
inline constexpr ParameterId kFilterCutoff = makeParameterId("filter.cutoff");
inline constexpr ParameterId kFilterResonance = makeParameterId("filter.resonance");
inline constexpr ParameterId kFilterEnvOctaves = makeParameterId("filter.envOctaves");
inline constexpr ParameterId kFilterAttackMs = makeParameterId("filter.attack");
inline constexpr ParameterId kFilterDecayMs = makeParameterId("filter.decay");
inline constexpr ParameterId kFilterSustain = makeParameterId("filter.sustain");
inline constexpr ParameterId kFilterReleaseMs = makeParameterId("filter.release");

// -- Amplitude envelope ------------------------------------------------------
inline constexpr ParameterId kAmpAttackMs = makeParameterId("amp.attack");
inline constexpr ParameterId kAmpDecayMs = makeParameterId("amp.decay");
inline constexpr ParameterId kAmpSustain = makeParameterId("amp.sustain");
inline constexpr ParameterId kAmpReleaseMs = makeParameterId("amp.release");

// -- Velocity ----------------------------------------------------------------
inline constexpr ParameterId kVelocityVolume = makeParameterId("velocity.volume");
inline constexpr ParameterId kVelocityBrightness = makeParameterId("velocity.brightness");

// -- LFO ---------------------------------------------------------------------
inline constexpr ParameterId kLfoShape = makeParameterId("lfo.shape");
inline constexpr ParameterId kLfoRate = makeParameterId("lfo.rate");
inline constexpr ParameterId kLfoCutoffDepth = makeParameterId("lfo.cutoffDepth");
inline constexpr ParameterId kLfoPitchDepthCents = makeParameterId("lfo.pitchDepth");

// -- Master ------------------------------------------------------------------
inline constexpr ParameterId kMasterGain = makeParameterId("master.gain");

/// Control thread. Registers every synth parameter with its metadata. Returns
/// false if any definition is rejected (a coding error, reported by the test
/// that builds the registry).
[[nodiscard]] inline bool registerSynthParameters(ParameterRegistry& registry) noexcept {
    using core::ParameterCurve;
    using core::ParameterUnit;

    const ParameterDescriptor descriptors[] = {
        // id, name, min, max, default, unit, curve
        {kOsc1Waveform, "osc1.waveform", 0.0F, 3.0F, 1.0F, ParameterUnit::None,
         ParameterCurve::Linear},
        {kOsc1Level, "osc1.level", 0.0F, 1.0F, 0.5F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kOsc1DetuneCents, "osc1.detune", -50.0F, 50.0F, 0.0F, ParameterUnit::Cents,
         ParameterCurve::Linear},
        {kOsc1PulseWidth, "osc1.pulseWidth", 0.05F, 0.95F, 0.5F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kOsc2Waveform, "osc2.waveform", 0.0F, 3.0F, 1.0F, ParameterUnit::None,
         ParameterCurve::Linear},
        {kOsc2Level, "osc2.level", 0.0F, 1.0F, 0.5F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kOsc2DetuneCents, "osc2.detune", -50.0F, 50.0F, 0.0F, ParameterUnit::Cents,
         ParameterCurve::Linear},
        {kOsc2PulseWidth, "osc2.pulseWidth", 0.05F, 0.95F, 0.5F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kSubLevel, "sub.level", 0.0F, 1.0F, 0.0F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kNoiseLevel, "noise.level", 0.0F, 1.0F, 0.0F, ParameterUnit::Percent,
         ParameterCurve::Linear},

        {kUnisonVoices, "unison.voices", 1.0F, 4.0F, 1.0F, ParameterUnit::None,
         ParameterCurve::Linear},
        {kUnisonDetuneCents, "unison.detune", 0.0F, 50.0F, 0.0F, ParameterUnit::Cents,
         ParameterCurve::Linear},
        {kUnisonStereo, "unison.stereo", 0.0F, 1.0F, 0.0F, ParameterUnit::Percent,
         ParameterCurve::Linear},

        {kFilterCutoff, "filter.cutoff", 20.0F, 20000.0F, 5000.0F, ParameterUnit::Hertz,
         ParameterCurve::Logarithmic},
        {kFilterResonance, "filter.resonance", 0.0F, 1.0F, 0.1F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kFilterEnvOctaves, "filter.envOctaves", 0.0F, 8.0F, 2.0F, ParameterUnit::Octaves,
         ParameterCurve::Linear},
        {kFilterAttackMs, "filter.attack", 0.5F, 10000.0F, 5.0F, ParameterUnit::Milliseconds,
         ParameterCurve::Logarithmic},
        {kFilterDecayMs, "filter.decay", 0.5F, 10000.0F, 200.0F, ParameterUnit::Milliseconds,
         ParameterCurve::Logarithmic},
        {kFilterSustain, "filter.sustain", 0.0F, 1.0F, 0.4F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kFilterReleaseMs, "filter.release", 0.5F, 20000.0F, 200.0F,
         ParameterUnit::Milliseconds, ParameterCurve::Logarithmic},

        {kAmpAttackMs, "amp.attack", 0.5F, 10000.0F, 5.0F, ParameterUnit::Milliseconds,
         ParameterCurve::Logarithmic},
        {kAmpDecayMs, "amp.decay", 0.5F, 10000.0F, 120.0F, ParameterUnit::Milliseconds,
         ParameterCurve::Logarithmic},
        {kAmpSustain, "amp.sustain", 0.0F, 1.0F, 0.7F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kAmpReleaseMs, "amp.release", 0.5F, 20000.0F, 180.0F, ParameterUnit::Milliseconds,
         ParameterCurve::Logarithmic},

        {kVelocityVolume, "velocity.volume", 0.0F, 1.0F, 0.7F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kVelocityBrightness, "velocity.brightness", 0.0F, 1.0F, 0.0F, ParameterUnit::Percent,
         ParameterCurve::Linear},

        {kLfoShape, "lfo.shape", 0.0F, 3.0F, 0.0F, ParameterUnit::None, ParameterCurve::Linear},
        {kLfoRate, "lfo.rate", 0.05F, 20.0F, 5.0F, ParameterUnit::Hertz,
         ParameterCurve::Logarithmic},
        {kLfoCutoffDepth, "lfo.cutoffDepth", 0.0F, 1.0F, 0.0F, ParameterUnit::Percent,
         ParameterCurve::Linear},
        {kLfoPitchDepthCents, "lfo.pitchDepth", 0.0F, 100.0F, 0.0F, ParameterUnit::Cents,
         ParameterCurve::Linear},

        {kMasterGain, "master.gain", 0.0F, 1.0F, 0.7F, ParameterUnit::Percent,
         ParameterCurve::Linear},
    };

    for (const ParameterDescriptor& descriptor : descriptors) {
        if (!registry.add(descriptor)) {
            return false;
        }
    }
    return true;
}

}  // namespace drumas::synth
