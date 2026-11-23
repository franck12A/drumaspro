#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>
#include <vector>

#include "synth/SineOscillator.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kRenderFrames = 48000;  // one second

struct RenderResult {
    std::vector<float> samples;
    int zeroCrossings = 0;
    float peak = 0.0F;
    bool allFinite = true;
    bool inRange = true;
};

RenderResult render(drumas::synth::SineOscillator& oscillator) {
    RenderResult result;
    result.samples.resize(kRenderFrames);

    float previous = 0.0F;
    for (int frame = 0; frame < kRenderFrames; ++frame) {
        const float value = oscillator.nextSample();
        result.samples[static_cast<std::size_t>(frame)] = value;

        if (!std::isfinite(value)) {
            result.allFinite = false;
        }
        if (value < -1.0F - 1e-4F || value > 1.0F + 1e-4F) {
            result.inRange = false;
        }
        result.peak = std::max(result.peak, std::fabs(value));

        if ((previous <= 0.0F && value > 0.0F) || (previous >= 0.0F && value < 0.0F)) {
            ++result.zeroCrossings;
        }
        previous = value;
    }
    return result;
}

}  // namespace

TEST_CASE("sine oscillator renders the frequency it was asked for", "[synth][oscillator]") {
    drumas::synth::SineOscillator oscillator;
    oscillator.prepare(kSampleRate);

    SECTION("440 Hz") {
        oscillator.setFrequency(440.0F);
        oscillator.reset();
        const RenderResult result = render(oscillator);

        // A sine crosses zero twice per period: 880 crossings per second.
        REQUIRE(result.zeroCrossings == Catch::Approx(880).margin(2));
        REQUIRE(result.allFinite);
        REQUIRE(result.inRange);
        REQUIRE(result.peak == Catch::Approx(1.0F).margin(0.01F));
    }

    SECTION("1000 Hz") {
        oscillator.setFrequency(1000.0F);
        oscillator.reset();
        const RenderResult result = render(oscillator);

        REQUIRE(result.zeroCrossings == Catch::Approx(2000).margin(3));
        REQUIRE(result.allFinite);
        REQUIRE(result.inRange);
    }

    SECTION("100 Hz") {
        oscillator.setFrequency(100.0F);
        oscillator.reset();
        const RenderResult result = render(oscillator);

        REQUIRE(result.zeroCrossings == Catch::Approx(200).margin(2));
        REQUIRE(result.allFinite);
        REQUIRE(result.inRange);
    }
}

TEST_CASE("sine oscillator stays finite and bounded across the range", "[synth][oscillator]") {
    const float frequencies[] = {20.0F, 261.63F, 440.0F, 2000.0F, 10000.0F, 20000.0F};

    drumas::synth::SineOscillator oscillator;
    oscillator.prepare(kSampleRate);

    for (const float frequency : frequencies) {
        oscillator.setFrequency(frequency);
        oscillator.reset();

        for (int frame = 0; frame < 4096; ++frame) {
            const float value = oscillator.nextSample();
            REQUIRE(std::isfinite(value));
            REQUIRE(value >= -1.0F - 1e-4F);
            REQUIRE(value <= 1.0F + 1e-4F);
        }
    }
}

TEST_CASE("sine oscillator rejects non-finite frequencies", "[synth][oscillator]") {
    drumas::synth::SineOscillator oscillator;
    oscillator.prepare(kSampleRate);
    oscillator.setFrequency(440.0F);

    const float nan = std::numeric_limits<float>::quiet_NaN();
    oscillator.setFrequency(nan);

    for (int frame = 0; frame < 256; ++frame) {
        REQUIRE(std::isfinite(oscillator.nextSample()));
    }
}
