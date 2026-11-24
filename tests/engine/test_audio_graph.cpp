#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <vector>

#include "core/AudioTypes.h"
#include "core/IAudioProcessor.h"
#include "engine/AudioGraph.h"
#include "support/AllocationCounter.h"
#include "synth/SynthProcessor.h"

namespace {

/// Processor that records what the engine hands it and writes a constant per
/// channel, so the interleaving can be checked sample by sample.
class ProbeProcessor final : public drumas::core::IAudioProcessor {
public:
    void prepare(const drumas::core::AudioSpec& spec) override { spec_ = spec; }

    void reset() override { ++resetCount; }

    void process(std::span<const drumas::core::Command> commands, float* const* output,
                 int numChannels, int numFrames) noexcept override {
        commandCount += commands.size();
        for (const drumas::core::Command& command : commands) {
            lastCommandType = command.type;
        }
        for (int channel = 0; channel < numChannels; ++channel) {
            const float value = static_cast<float>(channel + 1) * 0.1F;
            std::fill_n(output[channel], numFrames, value);
        }
        lastFrames = numFrames;
        ++blockCount;
    }

    drumas::core::AudioSpec spec_{};
    std::size_t commandCount = 0;
    std::size_t resetCount = 0;
    drumas::core::CommandType lastCommandType = drumas::core::CommandType::None;
    int lastFrames = 0;
    int blockCount = 0;
};

}  // namespace

TEST_CASE("audio graph drains commands at the start of the block", "[engine][graph]") {
    ProbeProcessor processor;
    drumas::engine::AudioGraph graph;

    const drumas::core::AudioSpec spec{48000.0, 256, 2};
    graph.prepare(spec);
    graph.setProcessor(&processor);

    REQUIRE(graph.pushCommand(drumas::core::noteOn(69, 1.0F)));
    REQUIRE(graph.pushCommand(drumas::core::setParam(drumas::synth::kMasterGain, 0.5F)));

    std::vector<float> interleaved(2 * 256, 0.0F);
    graph.render(interleaved.data(), 2, 256);

    REQUIRE(processor.commandCount == 2);
    REQUIRE(processor.lastCommandType == drumas::core::CommandType::SetParam);
    REQUIRE(processor.blockCount == 1);

    const drumas::engine::AudioGraph::Stats stats = graph.stats();
    REQUIRE(stats.blocks == 1);
    REQUIRE(stats.frames == 256);
    REQUIRE(stats.commandsApplied == 2);
    REQUIRE(stats.commandsDropped == 0);
}

TEST_CASE("audio graph interleaves planar channels in device order", "[engine][graph]") {
    ProbeProcessor processor;
    drumas::engine::AudioGraph graph;

    const drumas::core::AudioSpec spec{48000.0, 128, 2};
    graph.prepare(spec);
    graph.setProcessor(&processor);

    std::vector<float> interleaved(2 * 128, 0.0F);
    graph.render(interleaved.data(), 2, 128);

    for (int frame = 0; frame < 128; ++frame) {
        REQUIRE(interleaved[static_cast<std::size_t>(frame) * 2] == 0.1F);
        REQUIRE(interleaved[static_cast<std::size_t>(frame) * 2 + 1] == 0.2F);
    }
}

TEST_CASE("audio graph splits blocks larger than the prepared maximum", "[engine][graph]") {
    ProbeProcessor processor;
    drumas::engine::AudioGraph graph;

    const drumas::core::AudioSpec spec{48000.0, 64, 2};
    graph.prepare(spec);
    graph.setProcessor(&processor);

    std::vector<float> interleaved(2 * 192, 0.0F);
    graph.render(interleaved.data(), 2, 192);

    REQUIRE(processor.blockCount == 3);
    REQUIRE(processor.lastFrames == 64);
    REQUIRE(graph.stats().frames == 192);
}

TEST_CASE("audio graph counts dropped commands when the queue is full", "[engine][graph]") {
    ProbeProcessor processor;
    drumas::engine::AudioGraph graph;

    const drumas::core::AudioSpec spec{48000.0, 256, 2};
    graph.prepare(spec);
    graph.setProcessor(&processor);

    std::size_t accepted = 0;
    for (std::size_t i = 0; i < drumas::engine::AudioGraph::kCommandCapacity + 4; ++i) {
        if (graph.pushCommand(drumas::core::noteOn(60, 1.0F))) {
            ++accepted;
        }
    }

    REQUIRE(accepted == drumas::engine::AudioGraph::kCommandCapacity - 1);
    REQUIRE(graph.stats().commandsDropped == 5);
}

TEST_CASE("audio graph renders silence without a processor", "[engine][graph]") {
    drumas::engine::AudioGraph graph;

    const drumas::core::AudioSpec spec{48000.0, 64, 2};
    graph.prepare(spec);

    std::vector<float> interleaved(2 * 64, 1.0F);
    graph.render(interleaved.data(), 2, 64);

    for (const float sample : interleaved) {
        REQUIRE(sample == 0.0F);
    }
}

TEST_CASE("a note commanded through the graph renders the expected pitch", "[engine][graph]") {
    drumas::synth::SynthProcessor synth;
    drumas::engine::AudioGraph graph;

    const drumas::core::AudioSpec spec{48000.0, 256, 2};
    synth.prepare(spec);
    graph.prepare(spec);
    graph.setProcessor(&synth);

    REQUIRE(graph.pushCommand(drumas::core::noteOn(69, 1.0F)));  // A4 = 440 Hz

    std::vector<float> interleaved(2 * 256, 0.0F);
    int zeroCrossings = 0;
    float previous = 0.0F;
    float peak = 0.0F;

    // One second of audio, one block at a time.
    constexpr int kBlocks = 48000 / 256;
    for (int block = 0; block < kBlocks; ++block) {
        graph.render(interleaved.data(), 2, 256);
        for (int frame = 0; frame < 256; ++frame) {
            const float sample = interleaved[static_cast<std::size_t>(frame) * 2];
            REQUIRE(std::isfinite(sample));
            peak = std::max(peak, std::fabs(sample));
            if ((previous <= 0.0F && sample > 0.0F) || (previous >= 0.0F && sample < 0.0F)) {
                ++zeroCrossings;
            }
            previous = sample;
        }
    }

    REQUIRE(zeroCrossings == Catch::Approx(880).margin(4));
    // Master gain 0.25, sustain 0.7: comfortably audible, far from clipping.
    REQUIRE(peak > 0.05F);
    REQUIRE(peak <= 1.0F);
}

TEST_CASE("rendering a block on the audio path does not allocate", "[engine][graph][realtime]") {
    // This is the documented check behind the "no allocation in the callback"
    // rule: AllocationCounter.cpp replaces global operator new/delete for the
    // whole test binary, so any allocation inside render() would be counted.
    // render() is exactly what the device callback calls.
    drumas::synth::SynthProcessor synth;
    drumas::engine::AudioGraph graph;

    const drumas::core::AudioSpec spec{48000.0, 512, 2};

    // Everything that is allowed to allocate happens here.
    synth.prepare(spec);
    graph.prepare(spec);
    graph.setProcessor(&synth);

    std::vector<float> interleaved(2 * 512, 0.0F);

    REQUIRE(graph.pushCommand(drumas::core::noteOn(69, 1.0F)));
    REQUIRE(graph.pushCommand(drumas::core::setParam(drumas::synth::kAmpAttackMs, 30.0F)));

    // Warm up once so first-call effects (lazy statics, OS page faults) are not
    // attributed to the measured blocks.
    graph.render(interleaved.data(), 2, 512);

    drumas::test::resetAllocationCount();

    constexpr int kBlocks = 128;
    double energy = 0.0;
    for (int block = 0; block < kBlocks; ++block) {
        graph.render(interleaved.data(), 2, 512);
        energy += std::fabs(interleaved[0]) + std::fabs(interleaved[1]);
    }

    REQUIRE(drumas::test::allocationCount() == 0);
    REQUIRE(energy > 0.0);  // the graph actually produced audio
    REQUIRE(graph.stats().blocks == static_cast<std::uint64_t>(kBlocks + 1));
    REQUIRE(synth.activeVoices() == 1);
}
