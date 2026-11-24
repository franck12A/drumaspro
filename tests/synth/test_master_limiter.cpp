#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <limits>

#include "synth/MasterLimiter.h"

using drumas::synth::MasterLimiter;

TEST_CASE("master limiter is bit-transparent below the knee", "[synth][master][limiter]") {
    const float values[] = {0.0F, 0.1F, -0.1F, 0.5F, -0.5F, 0.69F, -0.69F,
                            MasterLimiter::kKnee, -MasterLimiter::kKnee};
    for (const float value : values) {
        REQUIRE(MasterLimiter::processSample(value) == value);
    }
}

TEST_CASE("master limiter never exceeds the ceiling", "[synth][master][limiter]") {
    for (const float value : {0.7F, 0.8F, 1.0F, 2.0F, 10.0F, 1.0e6F, -1.0e6F,
                              std::numeric_limits<float>::infinity(),
                              -std::numeric_limits<float>::infinity()}) {
        const float output = MasterLimiter::processSample(value);
        INFO("input " << value << " -> output " << output);
        REQUIRE(std::isfinite(output));
        REQUIRE(std::fabs(output) <= MasterLimiter::kCeiling);
    }

    // A degenerate sum from sixteen full-scale voices is still bounded.
    for (float input = 0.0F; input <= 32.0F; input += 0.01F) {
        REQUIRE(std::fabs(MasterLimiter::processSample(input)) <= MasterLimiter::kCeiling);
        REQUIRE(std::fabs(MasterLimiter::processSample(-input)) <= MasterLimiter::kCeiling);
    }
}

TEST_CASE("master limiter is monotonic and continuous", "[synth][master][limiter]") {
    float previous = MasterLimiter::processSample(-4.0F);
    constexpr float kStep = 1.0e-4F;
    for (float input = -4.0F + kStep; input <= 4.0F; input += kStep) {
        const float output = MasterLimiter::processSample(input);
        // Monotonic: a louder input never comes out quieter.
        REQUIRE(output >= previous);
        // Continuous: the soft clip's slope never exceeds 1, so a 1e-4 input
        // step cannot produce a jump (a click would).
        REQUIRE(output - previous <= kStep * 1.01F + 1.0e-7F);
        previous = output;
    }
}

TEST_CASE("master limiter is odd-symmetric", "[synth][master][limiter]") {
    for (const float value : {0.3F, 0.75F, 1.2F, 4.0F}) {
        REQUIRE(MasterLimiter::processSample(-value) ==
                Catch::Approx(-MasterLimiter::processSample(value)).margin(1e-6F));
    }
}
