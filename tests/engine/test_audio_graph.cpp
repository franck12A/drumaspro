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
#include "fx/EffectFactory.h"
#include "fx/EffectTypes.h"
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
            const float value = silent ? 0.0F : static_cast<float>(channel + 1) * 0.1F;
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
    bool silent = false;
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

TEST_CASE("audio graph publishes output peak and decays it without blocking",
          "[engine][graph][realtime]") {
    ProbeProcessor processor;
    drumas::engine::AudioGraph graph;
    const drumas::core::AudioSpec spec{48000.0, 256, 2};
    graph.prepare(spec);
    graph.setProcessor(&processor);
    std::vector<float> output(2 * 256, 0.0F);

    graph.render(output.data(), 2, 256);
    REQUIRE(graph.stats().outputPeak == Catch::Approx(0.2F));

    processor.silent = true;
    graph.render(output.data(), 2, 256);
    REQUIRE(graph.stats().outputPeak > 0.0F);
    REQUIRE(graph.stats().outputPeak < 0.2F);
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

TEST_CASE("audio graph rejects command batches without partially enqueueing them",
          "[engine][graph][batch]") {
    ProbeProcessor processor;
    drumas::engine::AudioGraph graph;
    const drumas::core::AudioSpec spec{48000.0, 256, 2};
    graph.prepare(spec);
    graph.setProcessor(&processor);

    constexpr std::size_t kAcceptedBeforeBatch =
        drumas::engine::AudioGraph::kCommandCapacity - 2;
    for (std::size_t index = 0; index < kAcceptedBeforeBatch; ++index) {
        REQUIRE(graph.pushCommand(drumas::core::noteOn(60, 1.0F)));
    }

    const std::array<drumas::core::Command, 2> chordCommands{
        drumas::core::noteOn(64, 1.0F), drumas::core::noteOn(67, 1.0F)};
    REQUIRE_FALSE(graph.pushCommands(chordCommands));
    REQUIRE(graph.stats().commandsDropped == chordCommands.size());

    std::vector<float> output(2 * 256, 0.0F);
    graph.render(output.data(), 2, 256);
    REQUIRE(processor.commandCount == kAcceptedBeforeBatch);
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

TEST_CASE("a note commanded through the graph renders audible audio", "[engine][graph]") {
    drumas::synth::SynthProcessor synth;
    drumas::engine::AudioGraph graph;

    const drumas::core::AudioSpec spec{48000.0, 256, 2};
    synth.prepare(spec);
    graph.prepare(spec);
    graph.setProcessor(&synth);

    REQUIRE(graph.pushCommand(drumas::core::selectPatch(2)));  // Keys
    REQUIRE(graph.pushCommand(drumas::core::noteOn(69, 1.0F)));

    std::vector<float> interleaved(2 * 256, 0.0F);
    float peak = 0.0F;

    // One second of audio, one block at a time.
    constexpr int kBlocks = 48000 / 256;
    for (int block = 0; block < kBlocks; ++block) {
        graph.render(interleaved.data(), 2, 256);
        for (int frame = 0; frame < 256; ++frame) {
            const float sample = interleaved[static_cast<std::size_t>(frame) * 2];
            REQUIRE(std::isfinite(sample));
            peak = std::max(peak, std::fabs(sample));
        }
    }

    REQUIRE(peak > 0.02F);
    REQUIRE(peak <= 1.0F);  // the master soft clip guarantees this
    REQUIRE(synth.activeVoices() == 1);
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

    // Worst case of the stage: Pad uses unison 4 (eight oscillators per voice),
    // every one of the 32 voices is sounding, and the filter/LFO are running.
    REQUIRE(graph.pushCommand(drumas::core::selectPatch(0)));
    for (int note = 40; note < 40 + static_cast<int>(drumas::synth::VoicePool::kMaxVoices);
         ++note) {
        REQUIRE(graph.pushCommand(drumas::core::noteOn(static_cast<std::uint8_t>(note), 0.7F)));
    }
    REQUIRE(graph.pushCommand(drumas::core::setParam(drumas::synth::kFilterCutoff, 900.0F)));

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
    REQUIRE(synth.activeVoices() ==
            static_cast<std::uint32_t>(drumas::synth::VoicePool::kMaxVoices));
}

TEST_CASE("six inserts and the look-ahead limiter run without allocating",
          "[engine][graph][fx][realtime]") {
    drumas::synth::SynthProcessor synth;
    drumas::engine::AudioGraph graph;
    const drumas::core::AudioSpec spec{48000.0, 512, 2};
    synth.prepare(spec);
    graph.prepare(spec);
    graph.setProcessor(&synth);

    REQUIRE(graph.pushCommand(drumas::core::selectPatch(0)));
    for (std::size_t slot = 0; slot < drumas::fx::kEffectSlotCount; ++slot) {
        REQUIRE(graph.pushCommand(drumas::core::setEffectBypass(
            static_cast<std::uint8_t>(slot), false)));
        REQUIRE(graph.pushCommand(drumas::core::setEffectMix(
            static_cast<std::uint8_t>(slot), 0.5F)));
    }
    for (int note = 40; note < 40 + static_cast<int>(drumas::synth::VoicePool::kMaxVoices);
         ++note) {
        REQUIRE(graph.pushCommand(drumas::core::noteOn(static_cast<std::uint8_t>(note), 0.7F)));
    }

    std::vector<float> interleaved(2 * 512, 0.0F);
    graph.render(interleaved.data(), 2, 512);
    REQUIRE(synth.latencySamples() > 0);

    drumas::test::resetAllocationCount();
    for (int block = 0; block < 64; ++block) {
        graph.render(interleaved.data(), 2, 512);
    }
    REQUIRE(drumas::test::allocationCount() == 0);

    float peak = 0.0F;
    for (const float sample : interleaved) {
        REQUIRE(std::isfinite(sample));
        peak = std::max(peak, std::fabs(sample));
    }
    // -1 dBFS ceiling; the safety clip is below it.
    REQUIRE(peak <= 0.9F);
    REQUIRE(peak > 0.0F);
}

TEST_CASE("a hot effect swap in the full graph does not allocate",
          "[engine][graph][fx][realtime]") {
    drumas::synth::SynthProcessor synth;
    drumas::engine::AudioGraph graph;
    const drumas::core::AudioSpec spec{48000.0, 256, 2};
    synth.prepare(spec);
    graph.prepare(spec);
    graph.setProcessor(&synth);

    auto replacement = drumas::fx::createPreparedEffect(drumas::fx::kFilterType, spec);
    REQUIRE(replacement != nullptr);
    synth.applyCommand(drumas::core::swapEffect(
        0, reinterpret_cast<std::uintptr_t>(replacement.get())));
    replacement.release();

    std::vector<float> interleaved(2 * 256, 0.0F);
    drumas::test::resetAllocationCount();
    for (int block = 0; block < 32; ++block) {
        graph.render(interleaved.data(), 2, 256);
    }
    REQUIRE(drumas::test::allocationCount() == 0);

    drumas::fx::IEffect* retired = nullptr;
    while (synth.takeRetiredEffect(retired)) delete retired;
}
