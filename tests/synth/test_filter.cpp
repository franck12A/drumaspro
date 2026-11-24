#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

#include "synth/ResonantLowPassFilter.h"

namespace {

using drumas::synth::ResonantLowPassFilter;

constexpr double kSampleRate = 48000.0;
constexpr int kSettleFrames = 9600;  // 200 ms
constexpr int kMeasureFrames = 24000;

/// Steady-state RMS of the filter driven by a sine at `frequency`.
[[nodiscard]] double sineGain(ResonantLowPassFilter filter, double frequency) {
    const double increment = 2.0 * std::numbers::pi * frequency / kSampleRate;
    double phase = 0.0;
    double sumOfSquares = 0.0;

    for (int frame = 0; frame < kSettleFrames + kMeasureFrames; ++frame) {
        const float input = static_cast<float>(std::sin(phase));
        phase += increment;
        float left = input;
        float right = input;
        filter.processStereo(left, right);
        if (frame >= kSettleFrames) {
            sumOfSquares += static_cast<double>(left) * static_cast<double>(left);
        }
    }
    // Input is a unit sine: its RMS is 1/sqrt(2). Returning the ratio makes the
    // numbers comparable to the filter's gain directly.
    return std::sqrt(sumOfSquares / static_cast<double>(kMeasureFrames)) * std::numbers::sqrt2;
}

[[nodiscard]] ResonantLowPassFilter makeFilter(float cutoff, float resonance) {
    ResonantLowPassFilter filter;
    filter.prepare(kSampleRate);
    filter.setCutoff(cutoff);
    filter.setResonance(resonance);
    filter.reset();
    return filter;
}

}  // namespace

TEST_CASE("low-pass filter attenuates above the cutoff", "[synth][filter]") {
    const double passband = sineGain(makeFilter(1000.0F, 0.0F), 200.0);
    const double stopband = sineGain(makeFilter(1000.0F, 0.0F), 4000.0);
    const double attenuationDb = 20.0 * std::log10(stopband / passband);

    INFO("200 Hz gain " << passband << ", 4 kHz gain " << stopband << ", "
                        << attenuationDb << " dB");
    // Second order: 12 dB/octave, two octaves above cutoff = -24 dB.
    REQUIRE(attenuationDb < -18.0);
    REQUIRE(passband > 0.9);
    REQUIRE(passband < 1.1);
}

TEST_CASE("resonance boosts the cutoff region", "[synth][filter]") {
    const double flat = sineGain(makeFilter(1000.0F, 0.0F), 1000.0);
    const double resonant = sineGain(makeFilter(1000.0F, 0.95F), 1000.0);

    INFO("flat gain " << flat << ", resonant gain " << resonant);
    REQUIRE(resonant > flat * 5.0);
    REQUIRE(std::isfinite(resonant));
}

TEST_CASE("low-pass filter stays finite at the extremes and maximum resonance",
          "[synth][filter]") {
    for (const float cutoff : {20.0F, 100.0F, 1000.0F, 20000.0F}) {
        ResonantLowPassFilter filter = makeFilter(cutoff, 1.0F);

        // Impulses plus noise: the worst case for an unstable resonant loop.
        std::uint32_t state = 0x12345678U;
        for (int frame = 0; frame < 96000; ++frame) {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;
            const float noise =
                static_cast<float>(static_cast<std::int32_t>(state)) / 2147483648.0F;
            const float input = frame % 4800 == 0 ? 4.0F : noise * 0.5F;

            float left = input;
            float right = input;
            filter.processStereo(left, right);
            REQUIRE(std::isfinite(left));
            REQUIRE(std::isfinite(right));
            REQUIRE(std::fabs(left) < 1.0e6F);
            REQUIRE(std::fabs(right) < 1.0e6F);
        }
    }
}

TEST_CASE("cutoff modulation is stable with extreme per-sample sweeps", "[synth][filter]") {
    ResonantLowPassFilter filter = makeFilter(1000.0F, 1.0F);
    float peak = 0.0F;

    for (int frame = 0; frame < 96000; ++frame) {
        const float sweep = static_cast<float>(frame % 4800) / 4800.0F;
        filter.setCutoff(20.0F + sweep * 20000.0F);
        float left = std::sin(static_cast<float>(frame) * 0.13F);
        float right = left;
        filter.processStereo(left, right);
        REQUIRE(std::isfinite(left));
        peak = std::max(peak, std::fabs(left));
    }
    REQUIRE(peak < 1.0e6F);
}

TEST_CASE("changing cutoff mid-render does not produce a step", "[synth][filter]") {
    ResonantLowPassFilter filter = makeFilter(500.0F, 0.2F);
    constexpr double kIncrement = 2.0 * std::numbers::pi * 440.0 / kSampleRate;

    double phase = 0.0;
    float previous = 0.0F;
    float steadyMaxDelta = 0.0F;
    float switchMaxDelta = 0.0F;

    for (int frame = 0; frame < 48000; ++frame) {
        if (frame == 24000) {
            filter.setCutoff(5000.0F);  // one hard jump, no smoothing
        }
        float left = static_cast<float>(std::sin(phase));
        float right = left;
        phase += kIncrement;
        filter.processStereo(left, right);

        if (frame > 12000 && frame < 23990) {
            steadyMaxDelta = std::max(steadyMaxDelta, std::fabs(left - previous));
        }
        if (frame > 24000 && frame < 24100) {
            switchMaxDelta = std::max(switchMaxDelta, std::fabs(left - previous));
        }
        previous = left;
    }

    INFO("steady max delta " << steadyMaxDelta << ", switch max delta " << switchMaxDelta);
    // The integrator state is continuous, so the coefficient change can alter
    // the slope but must not create a click-sized jump.
    REQUIRE(switchMaxDelta < steadyMaxDelta * 2.0F + 0.05F);
}

TEST_CASE("filter ignores non-finite parameters", "[synth][filter]") {
    ResonantLowPassFilter filter = makeFilter(1000.0F, 0.3F);
    filter.setCutoff(std::numeric_limits<float>::quiet_NaN());
    filter.setResonance(std::numeric_limits<float>::infinity());

    float left = 0.5F;
    float right = -0.5F;
    filter.processStereo(left, right);
    REQUIRE(std::isfinite(left));
    REQUIRE(std::isfinite(right));
    REQUIRE(filter.cutoff() == Catch::Approx(1000.0F));
    REQUIRE(filter.resonance() == Catch::Approx(0.3F));
}
