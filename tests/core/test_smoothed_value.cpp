#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

#include "core/SmoothedValue.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr double kRampMs = 10.0;
constexpr int kRampSamples = static_cast<int>(kSampleRate * kRampMs / 1000.0);

}  // namespace

TEST_CASE("smoothed value starts at its reset value", "[core][smoothing]") {
    drumas::core::SmoothedValue smoother;
    smoother.prepare(kSampleRate, kRampMs);
    smoother.reset(0.5F);

    REQUIRE(smoother.current() == Catch::Approx(0.5F));
    REQUIRE(smoother.target() == Catch::Approx(0.5F));
    REQUIRE_FALSE(smoother.isSmoothing());
}

TEST_CASE("smoothed value ramps towards its target without jumping", "[core][smoothing]") {
    drumas::core::SmoothedValue smoother;
    smoother.prepare(kSampleRate, kRampMs);
    smoother.reset(0.0F);

    smoother.setTargetValue(1.0F);
    REQUIRE(smoother.isSmoothing());

    float previous = smoother.current();
    float largestStep = 0.0F;
    int arrivalSample = -1;

    for (int sample = 0; sample < kRampSamples * 4; ++sample) {
        const float value = smoother.getNextValue();
        const float step = value - previous;

        // Monotonic towards the target, never past it, never NaN.
        REQUIRE(std::isfinite(value));
        REQUIRE(step >= -1e-6F);
        REQUIRE(value <= 1.0F + 1e-6F);
        largestStep = std::max(largestStep, step);

        if (arrivalSample < 0 && !smoother.isSmoothing()) {
            arrivalSample = sample;
        }
        previous = value;
    }

    REQUIRE(largestStep < 0.01F);
    REQUIRE(previous == Catch::Approx(1.0F));
    REQUIRE(arrivalSample >= 0);
    // Linear ramp configured for 10 ms: it must land on target on time (one
    // sample of slack for the exact-arrival snap).
    REQUIRE(arrivalSample <= kRampSamples);
}

TEST_CASE("smoothed value handles a decreasing target", "[core][smoothing]") {
    drumas::core::SmoothedValue smoother;
    smoother.prepare(kSampleRate, kRampMs);
    smoother.reset(1.0F);

    smoother.setTargetValue(0.0F);

    float previous = smoother.current();
    for (int sample = 0; sample < kRampSamples * 4; ++sample) {
        const float value = smoother.getNextValue();
        REQUIRE(value <= previous + 1e-6F);
        previous = value;
    }
    REQUIRE(previous == Catch::Approx(0.0F));
}

TEST_CASE("smoothed value snapToTarget skips the ramp", "[core][smoothing]") {
    drumas::core::SmoothedValue smoother;
    smoother.prepare(kSampleRate, kRampMs);
    smoother.reset(0.0F);

    smoother.setTargetValue(0.75F);
    smoother.snapToTarget();

    REQUIRE_FALSE(smoother.isSmoothing());
    REQUIRE(smoother.getNextValue() == Catch::Approx(0.75F));
}
