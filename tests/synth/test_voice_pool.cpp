#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "synth/PatchBank.h"
#include "synth/VoicePool.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockFrames = 256;

drumas::synth::Patch testPatch(int unisonVoices = 1) {
    drumas::synth::Patch patch;
    patch.name = "Test";
    patch.osc1Waveform = drumas::synth::Waveform::Saw;
    patch.osc1Level = 0.5F;
    patch.osc2Waveform = drumas::synth::Waveform::Square;
    patch.osc2Level = 0.3F;
    patch.unisonVoices = unisonVoices;
    patch.unisonDetuneCents = 6.0F;
    patch.unisonStereo = 0.5F;
    patch.cutoffHz = 4000.0F;
    patch.resonance = 0.1F;
    patch.filterEnvOctaves = 0.0F;
    patch.ampEnvelope = drumas::synth::AdsrParameters{1.0F, 10.0F, 0.8F, 10.0F};
    patch.filterEnvelope = drumas::synth::AdsrParameters{1.0F, 10.0F, 0.8F, 10.0F};
    patch.velocityVolume = 0.0F;
    patch.velocityBrightness = 0.0F;
    patch.lfoPitchDepthCents = 0.0F;
    patch.lfoCutoffDepth = 0.0F;
    return patch;
}

struct StereoBlock {
    std::array<float, kBlockFrames> left{};
    std::array<float, kBlockFrames> right{};
    std::array<float, kBlockFrames> lfo{};
};

void renderBlock(drumas::synth::VoicePool& pool, StereoBlock& block) {
    block.left.fill(0.0F);
    block.right.fill(0.0F);
    block.lfo.fill(0.0F);
    pool.renderAdd(block.left.data(), block.right.data(), kBlockFrames, block.lfo.data());
}

}  // namespace

TEST_CASE("voice pool starts silent", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);

    REQUIRE(pool.activeVoiceCount() == 0);
    REQUIRE(pool.stealCount() == 0);
}

TEST_CASE("voice pool allocates a voice per note", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    pool.noteOn(60, 1.0F, patch, live);
    REQUIRE(pool.activeVoiceCount() == 1);
    REQUIRE(pool.voice(0).note() == 60);
    REQUIRE(pool.voice(0).isHeld());
    REQUIRE(pool.stealCount() == 0);

    pool.noteOn(64, 0.5F, patch, live);
    REQUIRE(pool.activeVoiceCount() == 2);

    // Same note again retriggers the existing voice instead of taking another.
    pool.noteOn(60, 0.25F, patch, live);
    REQUIRE(pool.activeVoiceCount() == 2);
    REQUIRE(pool.stealCount() == 0);
}

TEST_CASE("voice pool steals when it runs out of voices", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    for (std::size_t index = 0; index < drumas::synth::VoicePool::kMaxVoices; ++index) {
        pool.noteOn(static_cast<std::uint8_t>(30 + index), 1.0F, patch, live);
    }
    REQUIRE(pool.activeVoiceCount() == drumas::synth::VoicePool::kMaxVoices);
    REQUIRE(pool.stealCount() == 0);

    pool.noteOn(90, 1.0F, patch, live);
    REQUIRE(pool.activeVoiceCount() == drumas::synth::VoicePool::kMaxVoices);
    REQUIRE(pool.stealCount() == 1);
}

TEST_CASE("each voice captures the patch it started with", "[synth][voices][patches]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);

    const drumas::synth::Patch& pad = drumas::synth::kFactoryPatches[0];
    const drumas::synth::Patch& pluck = drumas::synth::kFactoryPatches[1];
    pool.noteOn(60, 1.0F, pad, drumas::synth::liveParametersFrom(pad));
    pool.noteOn(64, 1.0F, pluck, drumas::synth::liveParametersFrom(pluck));

    REQUIRE(pool.voice(0).patch().name == std::string_view("Pad"));
    REQUIRE(pool.voice(1).patch().name == std::string_view("Pluck"));
}

TEST_CASE("note off releases the voices playing that note", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    pool.noteOn(60, 1.0F, patch, live);
    pool.noteOn(64, 1.0F, patch, live);

    pool.noteOff(60);
    REQUIRE_FALSE(pool.voice(0).isHeld());
    REQUIRE(pool.voice(1).isHeld());

    // The released voice keeps its tail until the envelope reaches idle.
    StereoBlock block;
    for (int iteration = 0; iteration < 40; ++iteration) {
        renderBlock(pool, block);
    }
    REQUIRE(pool.voice(0).isFree());
    REQUIRE(pool.activeVoiceCount() == 1);
}

TEST_CASE("oscillator levels are live and ramp on sounding voices", "[synth][voices][parameters]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    drumas::synth::Patch patch = testPatch();
    patch.osc1Level = 1.0F;
    patch.osc2Level = 0.0F;
    patch.subLevel = 0.0F;
    patch.noiseLevel = 0.0F;
    drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    pool.noteOn(60, 1.0F, patch, live);
    StereoBlock block;
    float fullPeak = 0.0F;
    for (int iteration = 0; iteration < 80; ++iteration) {
        renderBlock(pool, block);
        for (const float sample : block.left) {
            fullPeak = std::max(fullPeak, std::fabs(sample));
        }
    }
    REQUIRE(fullPeak > 0.01F);

    live.osc1Level = 0.0F;
    pool.setLiveParameters(live);
    float silentPeak = 0.0F;
    for (int iteration = 0; iteration < 80; ++iteration) {
        renderBlock(pool, block);
        if (iteration >= 40) {
            for (const float sample : block.left) {
                silentPeak = std::max(silentPeak, std::fabs(sample));
            }
        }
    }
    REQUIRE(silentPeak < fullPeak * 0.01F);
}

TEST_CASE("rendering voices produces finite, bounded stereo audio", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch(4);
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    pool.noteOn(57, 1.0F, patch, live);
    pool.noteOn(60, 0.8F, patch, live);
    pool.noteOn(64, 0.6F, patch, live);

    StereoBlock block;
    for (int iteration = 0; iteration < 200; ++iteration) {
        renderBlock(pool, block);
        for (int frame = 0; frame < kBlockFrames; ++frame) {
            REQUIRE(std::isfinite(block.left[static_cast<std::size_t>(frame)]));
            REQUIRE(std::isfinite(block.right[static_cast<std::size_t>(frame)]));
            // Three full voices before master gain: a generous bound that still
            // catches runaway resonance or unison summing.
            REQUIRE(std::fabs(block.left[static_cast<std::size_t>(frame)]) <= 3.0F);
            REQUIRE(std::fabs(block.right[static_cast<std::size_t>(frame)]) <= 3.0F);
        }
    }
}
