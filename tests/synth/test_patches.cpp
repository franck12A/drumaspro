#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "core/AudioTypes.h"
#include "synth/PatchBank.h"
#include "synth/SynthProcessor.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockFrames = 512;
constexpr int kChordNotes[] = {60, 64, 67, 71};  // C major 7

struct RenderStats {
    float peak = 0.0F;
    float largestStep = 0.0F;
    bool allFinite = true;
    std::uint32_t maxActiveVoices = 0;
};

/// Renders `blocks` blocks of a held four-note chord through the real
/// processor and returns peak/continuity statistics of the stereo mix.
[[nodiscard]] RenderStats renderChord(int patchIndex, int blocks) {
    drumas::synth::SynthProcessor synth;
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    synth.prepare(spec);

    drumas::core::Command select = drumas::core::selectPatch(static_cast<std::uint8_t>(patchIndex));
    synth.applyCommand(select);
    for (const int note : kChordNotes) {
        synth.applyCommand(drumas::core::noteOn(static_cast<std::uint8_t>(note), 0.8F));
    }

    std::vector<float> left(kBlockFrames);
    std::vector<float> right(kBlockFrames);
    std::array<float*, 2> outputs{left.data(), right.data()};

    RenderStats stats;
    float previousLeft = 0.0F;
    float previousRight = 0.0F;
    const std::span<const drumas::core::Command> noCommands;

    for (int block = 0; block < blocks; ++block) {
        synth.process(noCommands, outputs.data(), 2, kBlockFrames);
        stats.maxActiveVoices = std::max(stats.maxActiveVoices, synth.activeVoices());

        for (int frame = 0; frame < kBlockFrames; ++frame) {
            const float currentLeft = left[static_cast<std::size_t>(frame)];
            const float currentRight = right[static_cast<std::size_t>(frame)];
            if (!std::isfinite(currentLeft) || !std::isfinite(currentRight)) {
                stats.allFinite = false;
            }
            stats.peak = std::max({stats.peak, std::fabs(currentLeft), std::fabs(currentRight)});
            stats.largestStep = std::max({stats.largestStep,
                                          std::fabs(currentLeft - previousLeft),
                                          std::fabs(currentRight - previousRight)});
            previousLeft = currentLeft;
            previousRight = currentRight;
        }
    }
    return stats;
}

}  // namespace

TEST_CASE("every factory patch renders a four-note chord without artefacts",
          "[synth][patches]") {
    for (int patchIndex = 0; patchIndex < drumas::synth::kFactoryPatchCount; ++patchIndex) {
        const drumas::synth::Patch& patch =
            drumas::synth::kFactoryPatches[static_cast<std::size_t>(patchIndex)];
        const RenderStats stats = renderChord(patchIndex, 375);  // 4 seconds

        INFO("patch " << patch.name << ": peak " << stats.peak << ", largest step "
                      << stats.largestStep);

        REQUIRE(stats.allFinite);
        REQUIRE(stats.maxActiveVoices == 4);
        REQUIRE(stats.peak > 0.01F);   // the chord is audible
        REQUIRE(stats.peak <= 1.0F);   // the master soft clip holds the ceiling
        // A click between samples would be an order of magnitude above the
        // waveform's own steepest transition (a band-limited saw edge).
        REQUIRE(stats.largestStep < 0.8F);
    }
}

TEST_CASE("the factory patches use different voice settings", "[synth][patches]") {
    const auto& patches = drumas::synth::kFactoryPatches;

    REQUIRE(patches[0].unisonVoices == 4);
    REQUIRE(patches[0].ampEnvelope.attackMs > 100.0F);
    REQUIRE(patches[0].ampEnvelope.releaseMs > 1000.0F);

    REQUIRE(patches[1].ampEnvelope.attackMs < 5.0F);
    REQUIRE(patches[1].ampEnvelope.sustain < 0.1F);
    REQUIRE(patches[1].filterEnvOctaves > 3.0F);

    REQUIRE(patches[2].velocityBrightness > 0.5F);
    REQUIRE(patches[2].osc2Waveform == drumas::synth::Waveform::Sine);

    for (const drumas::synth::Patch& patch : patches) {
        REQUIRE_FALSE(patch.name.empty());
        REQUIRE(patch.unisonVoices >= 1);
        REQUIRE(patch.unisonVoices <= drumas::synth::SynthVoice::kMaxUnison);
    }
}
