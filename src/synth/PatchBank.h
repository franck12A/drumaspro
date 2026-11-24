// The factory patch bank. Adding an instrument is adding an entry here.
//
// Values are starting points, not a claim of final taste: the timbre and the
// balance of each patch are on the human-listening checklist delivered with
// this stage.
#pragma once

#include <array>

#include "synth/Patch.h"

namespace drumas::synth {

inline constexpr int kFactoryPatchCount = 3;
inline constexpr int kDefaultPatchIndex = 0;

inline constexpr std::array<Patch, kFactoryPatchCount> kFactoryPatches = {{
    // ------------------------------------------------------------------ Pad --
    Patch{
        .name = "Pad",
        .osc1Waveform = Waveform::Saw,
        .osc1Level = 0.55F,
        .osc1DetuneCents = -7.0F,
        .osc2Waveform = Waveform::Saw,
        .osc2Level = 0.45F,
        .osc2DetuneCents = 7.0F,
        .subLevel = 0.12F,
        .noiseLevel = 0.02F,
        .unisonVoices = 4,
        .unisonDetuneCents = 14.0F,
        .unisonStereo = 0.8F,
        .cutoffHz = 750.0F,
        .resonance = 0.18F,
        .filterEnvOctaves = 2.2F,
        .ampEnvelope = AdsrParameters{700.0F, 1500.0F, 0.85F, 2600.0F},
        .filterEnvelope = AdsrParameters{1300.0F, 2000.0F, 0.55F, 2200.0F},
        .velocityVolume = 0.35F,
        .velocityBrightness = 0.25F,
        .lfoShape = LfoShape::Sine,
        .lfoRateHz = 0.22F,
        .lfoCutoffDepth = 0.22F,
        .lfoPitchDepthCents = 4.0F,
    },

    // ---------------------------------------------------------------- Pluck --
    Patch{
        .name = "Pluck",
        .osc1Waveform = Waveform::Saw,
        .osc1Level = 0.62F,
        .osc2Waveform = Waveform::Square,
        .osc2Level = 0.38F,
        .osc2DetuneCents = 6.0F,
        .osc2PulseWidth = 0.32F,
        .subLevel = 0.0F,
        .noiseLevel = 0.02F,
        .unisonVoices = 2,
        .unisonDetuneCents = 8.0F,
        .unisonStereo = 0.35F,
        .cutoffHz = 480.0F,
        .resonance = 0.42F,
        .filterEnvOctaves = 4.0F,
        .ampEnvelope = AdsrParameters{0.8F, 260.0F, 0.03F, 220.0F},
        .filterEnvelope = AdsrParameters{0.4F, 170.0F, 0.0F, 180.0F},
        .velocityVolume = 0.75F,
        .velocityBrightness = 0.65F,
        .lfoShape = LfoShape::Sine,
        .lfoRateHz = 5.0F,
        .lfoCutoffDepth = 0.0F,
        .lfoPitchDepthCents = 0.0F,
    },

    // ----------------------------------------------------------------- Keys --
    Patch{
        .name = "Keys",
        .osc1Waveform = Waveform::Triangle,
        .osc1Level = 0.75F,
        .osc1DetuneCents = -3.0F,
        .osc2Waveform = Waveform::Sine,
        .osc2Level = 0.65F,
        .osc2DetuneCents = 3.0F,
        .subLevel = 0.0F,
        .noiseLevel = 0.008F,
        .unisonVoices = 2,
        .unisonDetuneCents = 3.0F,
        .unisonStereo = 0.2F,
        .cutoffHz = 3200.0F,
        .resonance = 0.12F,
        .filterEnvOctaves = 1.6F,
        .ampEnvelope = AdsrParameters{2.5F, 1200.0F, 0.35F, 800.0F},
        .filterEnvelope = AdsrParameters{0.8F, 550.0F, 0.35F, 500.0F},
        .velocityVolume = 0.80F,
        .velocityBrightness = 0.85F,
        .lfoShape = LfoShape::Sine,
        .lfoRateHz = 4.8F,
        .lfoCutoffDepth = 0.04F,
        .lfoPitchDepthCents = 0.0F,
    },
}};

}  // namespace drumas::synth
