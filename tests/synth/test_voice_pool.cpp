#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "synth/VoicePool.h"

namespace {

constexpr double kSampleRate = 48000.0;

drumas::synth::AdsrParameters fastEnvelope() {
    drumas::synth::AdsrParameters parameters;
    parameters.attackMs = 1.0F;
    parameters.decayMs = 10.0F;
    parameters.sustain = 0.8F;
    parameters.releaseMs = 10.0F;
    return parameters;
}

}  // namespace

TEST_CASE("voice pool starts silent", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    pool.setEnvelopeParameters(fastEnvelope());

    REQUIRE(pool.activeVoiceCount() == 0);
    REQUIRE(pool.stealCount() == 0);
}

TEST_CASE("voice pool allocates a voice per note", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    pool.setEnvelopeParameters(fastEnvelope());

    pool.noteOn(60, 1.0F);
    REQUIRE(pool.activeVoiceCount() == 1);
    REQUIRE(pool.voice(0).note() == 60);
    REQUIRE(pool.voice(0).isHeld());
    REQUIRE(pool.stealCount() == 0);

    pool.noteOn(64, 0.5F);
    REQUIRE(pool.activeVoiceCount() == 2);

    // Same note again retriggers the existing voice instead of stealing.
    pool.noteOn(60, 0.25F);
    REQUIRE(pool.activeVoiceCount() == 2);
    REQUIRE(pool.stealCount() == 0);
}

TEST_CASE("voice pool steals when it runs out of voices", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    pool.setEnvelopeParameters(fastEnvelope());

    for (std::size_t i = 0; i < drumas::synth::VoicePool::kMaxVoices; ++i) {
        pool.noteOn(static_cast<std::uint8_t>(60 + i), 1.0F);
    }
    REQUIRE(pool.activeVoiceCount() == drumas::synth::VoicePool::kMaxVoices);
    REQUIRE(pool.stealCount() == 0);

    pool.noteOn(90, 1.0F);
    REQUIRE(pool.activeVoiceCount() == drumas::synth::VoicePool::kMaxVoices);
    REQUIRE(pool.stealCount() == 1);
}

TEST_CASE("note off releases the voices playing that note", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    pool.setEnvelopeParameters(fastEnvelope());

    pool.noteOn(60, 1.0F);
    pool.noteOn(64, 1.0F);

    pool.noteOff(60);
    REQUIRE_FALSE(pool.voice(0).isHeld());
    REQUIRE(pool.voice(1).isHeld());

    // The released voice keeps its tail until the envelope reaches idle.
    std::vector<float> mix(256, 0.0F);
    for (int block = 0; block < 40; ++block) {
        std::fill(mix.begin(), mix.end(), 0.0F);
        pool.renderAdd(mix.data(), static_cast<int>(mix.size()));
    }
    REQUIRE(pool.voice(0).isFree());
    REQUIRE(pool.activeVoiceCount() == 1);
}

TEST_CASE("rendering voices produces finite, bounded audio", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    pool.setEnvelopeParameters(fastEnvelope());

    pool.noteOn(57, 1.0F);
    pool.noteOn(60, 0.8F);
    pool.noteOn(64, 0.6F);

    std::vector<float> mix(512, 0.0F);
    for (int block = 0; block < 200; ++block) {
        std::fill(mix.begin(), mix.end(), 0.0F);
        pool.renderAdd(mix.data(), static_cast<int>(mix.size()));

        for (const float sample : mix) {
            REQUIRE(std::isfinite(sample));
            REQUIRE(std::fabs(sample) <= 3.0F);  // three voices, before master gain
        }
    }
}
