#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "core/AudioTypes.h"
#include "synth/PatchBank.h"
#include "synth/SynthProcessor.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockFrames = 256;

struct Harness {
    drumas::synth::SynthProcessor synth;
    std::vector<float> left = std::vector<float>(kBlockFrames);
    std::vector<float> right = std::vector<float>(kBlockFrames);
    std::array<float*, 2> outputs{left.data(), right.data()};
    float previousLeft = 0.0F;
    float previousRight = 0.0F;

    Harness() {
        const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
        synth.prepare(spec);
    }

    /// Renders one block with at most one command and returns the largest
    /// sample-to-sample step observed in that block.
    float render(const drumas::core::Command* command) {
        std::fill(left.begin(), left.end(), 0.0F);
        std::fill(right.begin(), right.end(), 0.0F);

        std::span<const drumas::core::Command> commands;
        if (command != nullptr) {
            commands = std::span<const drumas::core::Command>(command, 1);
        }
        synth.process(commands, outputs.data(), 2, kBlockFrames);

        float largestStep = 0.0F;
        for (int frame = 0; frame < kBlockFrames; ++frame) {
            const float currentLeft = left[static_cast<std::size_t>(frame)];
            const float currentRight = right[static_cast<std::size_t>(frame)];
            largestStep = std::max({largestStep, std::fabs(currentLeft - previousLeft),
                                    std::fabs(currentRight - previousRight)});
            previousLeft = currentLeft;
            previousRight = currentRight;
        }
        return largestStep;
    }

    [[nodiscard]] float maxStepOf(int blocks) {
        float largestStep = 0.0F;
        for (int block = 0; block < blocks; ++block) {
            largestStep = std::max(largestStep, render(nullptr));
        }
        return largestStep;
    }

    [[nodiscard]] const drumas::synth::Patch* patchOfNote(std::uint8_t note) const {
        for (std::size_t index = 0; index < drumas::synth::VoicePool::kMaxVoices; ++index) {
            const drumas::synth::SynthVoice& voice = synth.voices().voice(index);
            if (voice.isActive() && voice.note() == note) {
                return &voice.patch();
            }
        }
        return nullptr;
    }
};

}  // namespace

TEST_CASE("selecting a patch mid-chord leaves sounding voices on the old patch",
          "[synth][patches][hot-swap]") {
    Harness harness;

    drumas::core::Command selectPad = drumas::core::selectPatch(0);
    harness.render(&selectPad);
    drumas::core::Command noteOn = drumas::core::noteOn(60, 0.8F);
    harness.render(&noteOn);
    (void)harness.maxStepOf(60);  // settle past the attack

    const float steadyStep = harness.maxStepOf(40);

    drumas::core::Command selectPluck = drumas::core::selectPatch(1);
    const float switchStep = harness.render(&selectPluck);
    const float afterSwitchStep = harness.maxStepOf(40);

    const drumas::synth::Patch* heldPatch = harness.patchOfNote(60);
    REQUIRE(heldPatch != nullptr);
    REQUIRE(heldPatch->name == std::string_view("Pad"));

    INFO("steady " << steadyStep << ", switch " << switchStep << ", after "
                   << afterSwitchStep);
    // The switch sends no parameter to the sounding voice: the output must stay
    // as continuous as before it.
    REQUIRE(switchStep <= steadyStep * 2.0F + 0.02F);
    REQUIRE(afterSwitchStep <= steadyStep * 2.0F + 0.02F);

    // A new note does use the new patch.
    drumas::core::Command newNote = drumas::core::noteOn(67, 0.8F);
    harness.render(&newNote);
    const drumas::synth::Patch* newPatch = harness.patchOfNote(67);
    REQUIRE(newPatch != nullptr);
    REQUIRE(newPatch->name == std::string_view("Pluck"));
}

TEST_CASE("moving cutoff and resonance during a note stays continuous",
          "[synth][patches][hot-swap][parameters]") {
    Harness harness;

    drumas::core::Command selectPad = drumas::core::selectPatch(0);
    harness.render(&selectPad);
    drumas::core::Command noteOn = drumas::core::noteOn(60, 0.8F);
    harness.render(&noteOn);
    (void)harness.maxStepOf(80);

    const float before = harness.maxStepOf(40);

    drumas::core::Command cutoff =
        drumas::core::setParam(drumas::synth::kFilterCutoff, 200.0F);
    const float cutoffStep = harness.render(&cutoff);
    const float afterCutoff = harness.maxStepOf(40);

    drumas::core::Command resonance =
        drumas::core::setParam(drumas::synth::kFilterResonance, 0.85F);
    const float resonanceStep = harness.render(&resonance);
    const float afterResonance = harness.maxStepOf(40);

    drumas::core::Command release =
        drumas::core::setParam(drumas::synth::kAmpReleaseMs, 4000.0F);
    const float releaseStep = harness.render(&release);

    INFO("before " << before << ", cutoff " << cutoffStep << ", after cutoff "
                   << afterCutoff << ", resonance " << resonanceStep << ", after resonance "
                   << afterResonance << ", release " << releaseStep);
    REQUIRE(cutoffStep <= before * 2.0F + 0.02F);
    REQUIRE(afterCutoff <= before * 2.0F + 0.02F);
    REQUIRE(resonanceStep <= before * 2.0F + 0.02F);
    REQUIRE(afterResonance <= before * 2.0F + 0.02F);
    REQUIRE(releaseStep <= before * 2.0F + 0.02F);
}

TEST_CASE("a new voice starts at the live parameter value", "[synth][patches][parameters]") {
    Harness harness;

    drumas::core::Command selectKeys = drumas::core::selectPatch(2);
    harness.render(&selectKeys);

    // Move cutoff far below the patch's 3.2 kHz before playing.
    drumas::core::Command cutoff =
        drumas::core::setParam(drumas::synth::kFilterCutoff, 120.0F);
    harness.render(&cutoff);

    drumas::core::Command noteOn = drumas::core::noteOn(69, 0.8F);
    harness.render(&noteOn);

    // The note is dark but audible: a very low cutoff leaves little energy.
    float peak = 0.0F;
    for (int block = 0; block < 40; ++block) {
        harness.render(nullptr);
        for (const float sample : harness.left) {
            peak = std::max(peak, std::fabs(sample));
        }
    }
    REQUIRE(peak > 0.0F);
    REQUIRE(peak < 1.0F);
}
