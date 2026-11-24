#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

#include "core/AudioTypes.h"
#include "fx/EffectTypes.h"
#include "synth/PatchBank.h"
#include "synth/SynthProcessor.h"

namespace {

using Clock = std::chrono::steady_clock;

constexpr double kSampleRate = 48000.0;
constexpr int kBlockFrames = 256;
constexpr int kBlocks = 200;

}  // namespace

TEST_CASE("callback cost with 32 voices and maximum unison is reported",
          "[synth][performance]") {
    // Not a pass/fail on time: the number depends on the machine and on the
    // build. It is printed so the stage report can quote a real measurement;
    // the acceptance threshold is checked by hand on the release build.
    drumas::synth::SynthProcessor synth;
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    synth.prepare(spec);

    synth.applyCommand(drumas::core::selectPatch(0));  // Pad, unison 4
    // The default chain is bypassed so stage 4 remains the reference sound; the
    // performance case enables all six slots and the master limiter.
    for (std::size_t slot = 0; slot < drumas::fx::kEffectSlotCount; ++slot) {
        synth.applyCommand(
            drumas::core::setEffectBypass(static_cast<std::uint8_t>(slot), false));
    }
    constexpr int kFirstNote = 36;
    for (int index = 0; index < static_cast<int>(drumas::synth::VoicePool::kMaxVoices); ++index) {
        synth.applyCommand(
            drumas::core::noteOn(static_cast<std::uint8_t>(kFirstNote + index), 0.8F));
    }

    std::vector<float> left(kBlockFrames);
    std::vector<float> right(kBlockFrames);
    std::array<float*, 2> outputs{left.data(), right.data()};
    const std::span<const drumas::core::Command> noCommands;

    // Warm up: lazy branches, caches and the first minBLEP corrections.
    for (int block = 0; block < 32; ++block) {
        synth.process(noCommands, outputs.data(), 2, kBlockFrames);
    }
    REQUIRE(synth.activeVoices() ==
            static_cast<std::uint32_t>(drumas::synth::VoicePool::kMaxVoices));

    const auto start = Clock::now();
    for (int block = 0; block < kBlocks; ++block) {
        synth.process(noCommands, outputs.data(), 2, kBlockFrames);
    }
    const auto end = Clock::now();

    const double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    const double msPerBlock = totalMs / static_cast<double>(kBlocks);
    const double budgetMs = 1000.0 * static_cast<double>(kBlockFrames) / kSampleRate;
    const double percent = 100.0 * msPerBlock / budgetMs;

    std::printf(
        "\n[performance] 32 voices, 2 osc x 4 unison + sub/filter/2 ADSR/LFO + "
        "6 inserts + limiter: %.3f ms/block, %.1f%% of the %.3f ms budget "
        "(block %d, %.0f Hz, latency %d samples): %s\n",
        msPerBlock, percent, budgetMs, kBlockFrames, kSampleRate,
        synth.latencySamples(), percent < 60.0 ? "PASS" : "OVER 60%");
    std::fflush(stdout);

    REQUIRE(msPerBlock > 0.0);
    REQUIRE(synth.latencySamples() > 0);
}
