#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

#include "core/AudioTypes.h"
#include "core/ParameterRegistry.h"
#include "fx/ChorusEffect.h"
#include "fx/DelayEffect.h"
#include "fx/DriveEffect.h"
#include "fx/FilterEffect.h"
#include "fx/MasterLimiter.h"
#include "fx/ReverbEffect.h"

namespace {

using drumas::core::AudioSpec;
using drumas::core::ParameterId;
using drumas::core::makeParameterId;

constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 256;

constexpr ParameterId kFilterCutoff = makeParameterId("filter.cutoff");
constexpr ParameterId kFilterResonance = makeParameterId("filter.resonance");
constexpr ParameterId kFilterMode = makeParameterId("filter.mode");
constexpr ParameterId kDelayTime = makeParameterId("delay.time");
constexpr ParameterId kDelayFeedback = makeParameterId("delay.feedback");
constexpr ParameterId kDelayTone = makeParameterId("delay.tone");
constexpr ParameterId kDelayPingPong = makeParameterId("delay.pingPong");
constexpr ParameterId kReverbDecay = makeParameterId("reverb.decay");
constexpr ParameterId kReverbSize = makeParameterId("reverb.size");
constexpr ParameterId kReverbDamping = makeParameterId("reverb.damping");
constexpr ParameterId kReverbPreDelay = makeParameterId("reverb.preDelay");
constexpr ParameterId kDriveAmount = makeParameterId("drive.amount");
constexpr ParameterId kDriveTone = makeParameterId("drive.tone");
constexpr ParameterId kDriveOutput = makeParameterId("drive.output");

[[nodiscard]] AudioSpec spec(int block = kBlock) {
    return AudioSpec{kSampleRate, block, 2};
}

void processBlock(drumas::fx::IEffect& effect, std::vector<float>& left,
                  std::vector<float>& right, int offset, int frames) {
    effect.process(left.data() + offset, right.data() + offset, frames);
}

void processAll(drumas::fx::IEffect& effect, std::vector<float>& left,
                std::vector<float>& right) {
    const int frames = static_cast<int>(left.size());
    for (int offset = 0; offset < frames; offset += kBlock) {
        processBlock(effect, left, right, offset, std::min(kBlock, frames - offset));
    }
}

[[nodiscard]] std::vector<float> renderSine(drumas::fx::IEffect& effect, double frequency,
                                            float amplitude, int frames) {
    std::vector<float> left(static_cast<std::size_t>(frames));
    std::vector<float> right(static_cast<std::size_t>(frames));
    const double increment = 2.0 * std::numbers::pi * frequency / kSampleRate;
    for (int index = 0; index < frames; ++index) {
        left[static_cast<std::size_t>(index)] =
            amplitude * static_cast<float>(std::sin(increment * static_cast<double>(index)));
        right[static_cast<std::size_t>(index)] = left[static_cast<std::size_t>(index)];
    }
    for (int offset = 0; offset < frames; offset += kBlock) {
        processBlock(effect, left, right, offset, std::min(kBlock, frames - offset));
    }
    return left;
}

[[nodiscard]] double rms(const std::vector<float>& samples, int start) {
    double sum = 0.0;
    for (int index = start; index < static_cast<int>(samples.size()); ++index) {
        sum += static_cast<double>(samples[static_cast<std::size_t>(index)]) *
               samples[static_cast<std::size_t>(index)];
    }
    return std::sqrt(sum / static_cast<double>(samples.size() - start));
}

/// Single-bin tone magnitude. Frequencies are chosen away from the fundamental
/// so window leakage cannot masquerade as an alias.
[[nodiscard]] double toneMagnitude(const std::vector<float>& samples, double frequency) {
    const double omega = 2.0 * std::numbers::pi * frequency / kSampleRate;
    const double coefficient = 2.0 * std::cos(omega);
    double s1 = 0.0;
    double s2 = 0.0;
    for (const float sample : samples) {
        const double s0 = static_cast<double>(sample) + coefficient * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    const double power = s1 * s1 + s2 * s2 - coefficient * s1 * s2;
    return std::sqrt(std::max(0.0, power)) / static_cast<double>(samples.size());
}

}  // namespace

TEST_CASE("filter attenuates the expected side of the cutoff", "[fx][filter]") {
    drumas::fx::FilterEffect filter;
    filter.prepare(spec());
    filter.setParameter(kFilterCutoff, 500.0F);
    filter.setParameter(kFilterResonance, 0.1F);
    filter.setParameter(kFilterMode, 0.0F);  // low-pass

    const std::vector<float> lowPassed = renderSine(filter, 1000.0, 0.25F, 48000);
    INFO("filter low-pass RMS: " << rms(lowPassed, 12000)
                                << " cutoff " << filter.currentCutoff()
                                << " resonance " << filter.currentResonance());
    REQUIRE(rms(lowPassed, 12000) < 0.25 * 0.35);

    filter.reset();
    filter.setParameter(kFilterCutoff, 5000.0F);
    filter.setParameter(kFilterMode, 1.0F);  // high-pass
    const std::vector<float> highPassed = renderSine(filter, 500.0, 0.25F, 48000);
    REQUIRE(rms(highPassed, 12000) < 0.25 * 0.35);

    filter.reset();
    filter.setParameter(kFilterCutoff, 1000.0F);
    filter.setParameter(kFilterResonance, 0.9F);
    filter.setParameter(kFilterMode, 2.0F);  // band-pass
    const std::vector<float> bandPassed = renderSine(filter, 1000.0, 0.1F, 48000);
    REQUIRE(rms(bandPassed, 12000) > 0.01);
}

TEST_CASE("filter stays finite at maximum resonance", "[fx][filter][stability]") {
    drumas::fx::FilterEffect filter;
    filter.prepare(spec());
    filter.setParameter(kFilterCutoff, 120.0F);
    filter.setParameter(kFilterResonance, 1.0F);
    filter.setParameter(kFilterMode, 0.0F);

    std::vector<float> left(48000, 0.0F);
    std::vector<float> right(48000, 0.0F);
    left[0] = 1.0F;
    right[0] = 1.0F;
    processAll(filter, left, right);
    float peak = 0.0F;
    for (const float sample : left) {
        REQUIRE(std::isfinite(sample));
        peak = std::max(peak, std::fabs(sample));
    }
    REQUIRE(peak < 20.0F);
}

TEST_CASE("delay places the first repeat within one sample", "[fx][delay][timing]") {
    drumas::fx::DelayEffect delay;
    delay.prepare(spec());
    delay.setParameter(kDelayTime, 100.0F);
    delay.setParameter(kDelayFeedback, 0.5F);
    delay.setParameter(kDelayTone, 16000.0F);
    delay.setParameter(kDelayPingPong, 0.0F);

    // Let the 50 ms time smoother settle before measuring.
    std::vector<float> silent(48000, 0.0F);
    std::vector<float> silentRight(48000, 0.0F);
    processAll(delay, silent, silentRight);

    constexpr int kFrames = 24000;
    std::vector<float> left(kFrames, 0.0F);
    std::vector<float> right(kFrames, 0.0F);
    left[0] = 1.0F;
    processAll(delay, left, right);

    int firstLeft = -1;
    int firstRight = -1;
    for (int index = 0; index < kFrames; ++index) {
        if (firstLeft < 0 && std::fabs(left[static_cast<std::size_t>(index)]) > 0.05F) {
            firstLeft = index;
        }
        if (firstRight < 0 && std::fabs(right[static_cast<std::size_t>(index)]) > 0.05F) {
            firstRight = index;
        }
    }
    INFO("first left repeat at " << firstLeft << ", first right at " << firstRight);
    REQUIRE(std::abs(firstLeft - 4800) <= 1);
    REQUIRE(firstRight == -1);  // ping-pong is off
}

TEST_CASE("delay ping-pong alternates channels", "[fx][delay][pingpong]") {
    drumas::fx::DelayEffect delay;
    delay.prepare(spec());
    delay.setParameter(kDelayTime, 100.0F);
    delay.setParameter(kDelayFeedback, 0.6F);
    delay.setParameter(kDelayTone, 16000.0F);
    delay.setParameter(kDelayPingPong, 1.0F);

    std::vector<float> silent(48000, 0.0F);
    std::vector<float> silentRight(48000, 0.0F);
    processAll(delay, silent, silentRight);

    constexpr int kFrames = 20000;
    std::vector<float> left(kFrames, 0.0F);
    std::vector<float> right(kFrames, 0.0F);
    left[0] = 1.0F;
    processAll(delay, left, right);

    const int firstEcho = 4800;
    float leftPeak = 0.0F;
    float rightPeak = 0.0F;
    for (int index = firstEcho - 2; index <= firstEcho + 2; ++index) {
        leftPeak = std::max(leftPeak, std::fabs(left[static_cast<std::size_t>(index)]));
        rightPeak = std::max(rightPeak, std::fabs(right[static_cast<std::size_t>(index)]));
    }
    REQUIRE(leftPeak > 0.05F);
    REQUIRE(rightPeak < 0.01F);
    float secondRightPeak = 0.0F;
    for (int index = firstEcho * 2 - 2; index <= firstEcho * 2 + 2; ++index) {
        secondRightPeak =
            std::max(secondRightPeak, std::fabs(right[static_cast<std::size_t>(index)]));
    }
    REQUIRE(secondRightPeak > 0.03F);
}

TEST_CASE("delay feedback at the maximum remains bounded", "[fx][delay][stability]") {
    drumas::fx::DelayEffect delay;
    delay.prepare(spec());
    delay.setParameter(kDelayTime, 20.0F);
    delay.setParameter(kDelayFeedback, 0.95F);
    delay.setParameter(kDelayTone, 12000.0F);
    delay.setParameter(kDelayPingPong, 1.0F);

    constexpr int kFrames = 48000 * 10;
    std::vector<float> left(kFrames, 0.0F);
    std::vector<float> right(kFrames, 0.0F);
    left[0] = 1.0F;
    processAll(delay, left, right);
    float peak = 0.0F;
    for (int index = 0; index < kFrames; ++index) {
        REQUIRE(std::isfinite(left[static_cast<std::size_t>(index)]));
        REQUIRE(std::isfinite(right[static_cast<std::size_t>(index)]));
        peak = std::max(peak, std::fabs(left[static_cast<std::size_t>(index)]));
    }
    INFO("maximum delay feedback peak: " << peak);
    REQUIRE(peak < 2.0F);
}

TEST_CASE("reverb impulse response is finite and decays", "[fx][reverb]") {
    drumas::fx::ReverbEffect reverb;
    reverb.prepare(spec());
    reverb.setParameter(kReverbDecay, 1.0F);
    reverb.setParameter(kReverbSize, 0.6F);
    reverb.setParameter(kReverbDamping, 0.3F);
    reverb.setParameter(kReverbPreDelay, 10.0F);

    constexpr int kFrames = 48000 * 3;
    std::vector<float> left(kFrames, 0.0F);
    std::vector<float> right(kFrames, 0.0F);
    left[0] = 1.0F;
    right[0] = 1.0F;
    processAll(reverb, left, right);

    double totalEnergy = 0.0;
    std::vector<double> energy(static_cast<std::size_t>(kFrames), 0.0);
    for (int index = kFrames - 1; index >= 0; --index) {
        const double sample = left[static_cast<std::size_t>(index)];
        totalEnergy += sample * sample;
        energy[static_cast<std::size_t>(index)] = totalEnergy;
    }
    REQUIRE(totalEnergy > 0.0);

    const double initial = energy[0];
    int first = -1;
    int last = -1;
    for (int index = 0; index < kFrames; ++index) {
        const double relative = energy[static_cast<std::size_t>(index)] / initial;
        if (first < 0 && relative <= 0.316) first = index;       // -5 dB
        if (last < 0 && relative <= 0.000316) last = index;      // -35 dB
    }
    REQUIRE(first > 0);
    REQUIRE(last > first);
    const double rt60 = 60.0 * static_cast<double>(last - first) /
                        (30.0 * kSampleRate);
    INFO("reverb measured RT60: " << rt60 << " s (requested 1.0 s)");
    REQUIRE(rt60 > 0.45);
    REQUIRE(rt60 < 2.2);

    float peak = 0.0F;
    for (const float sample : left) {
        REQUIRE(std::isfinite(sample));
        peak = std::max(peak, std::fabs(sample));
    }
    REQUIRE(peak < 2.0F);
}

TEST_CASE("reverb stays finite at every extreme", "[fx][reverb][stability]") {
    for (const float decay : {0.2F, 20.0F}) {
        for (const float size : {0.01F, 1.0F}) {
            for (const float damping : {0.0F, 1.0F}) {
                drumas::fx::ReverbEffect reverb;
                reverb.prepare(spec());
                reverb.setParameter(kReverbDecay, decay);
                reverb.setParameter(kReverbSize, size);
                reverb.setParameter(kReverbDamping, damping);
                reverb.setParameter(kReverbPreDelay, 200.0F);

                std::vector<float> left(4800, 0.0F);
                std::vector<float> right(4800, 0.0F);
                left[0] = 1.0F;
                processAll(reverb, left, right);
                for (const float sample : left) {
                    REQUIRE(std::isfinite(sample));
                    REQUIRE(std::fabs(sample) < 10.0F);
                }
            }
        }
    }
}

TEST_CASE("chorus produces a finite stereo signal without jumps", "[fx][chorus]") {
    drumas::fx::ChorusEffect chorus;
    chorus.prepare(spec());
    const std::vector<float> left = renderSine(chorus, 440.0, 0.2F, 48000);
    float peak = 0.0F;
    float maxStep = 0.0F;
    float previous = 0.0F;
    for (const float sample : left) {
        REQUIRE(std::isfinite(sample));
        peak = std::max(peak, std::fabs(sample));
        maxStep = std::max(maxStep, std::fabs(sample - previous));
        previous = sample;
    }
    INFO("chorus peak " << peak << ", max step " << maxStep);
    REQUIRE(peak < 1.0F);
    REQUIRE(maxStep < 0.25F);
}

TEST_CASE("drive is finite, compensated and less aliased with 2x oversampling",
          "[fx][drive][alias]") {
    drumas::fx::DriveEffect drive;
    drive.prepare(spec());
    drive.setParameter(kDriveAmount, 0.8F);
    drive.setParameter(kDriveTone, 16000.0F);
    drive.setParameter(kDriveOutput, 0.0F);

    constexpr int kFrames = 65536;
    std::vector<float> left(kFrames, 0.0F);
    std::vector<float> right(kFrames, 0.0F);
    const double frequency = 7000.0;
    for (int index = 0; index < kFrames; ++index) {
        left[static_cast<std::size_t>(index)] =
            0.1F * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency *
                                               static_cast<double>(index) / kSampleRate));
        right[static_cast<std::size_t>(index)] = left[static_cast<std::size_t>(index)];
    }
    processAll(drive, left, right);

    float peak = 0.0F;
    for (const float sample : left) {
        REQUIRE(std::isfinite(sample));
        peak = std::max(peak, std::fabs(sample));
    }
    REQUIRE(peak < 3.0F);

    double inputEnergy = 0.0;
    double outputEnergy = 0.0;
    for (int index = 12000; index < kFrames; ++index) {
        inputEnergy += 0.01 * 0.5;
        outputEnergy += static_cast<double>(left[static_cast<std::size_t>(index)]) *
                        left[static_cast<std::size_t>(index)];
    }
    const double inputRms = std::sqrt(inputEnergy / static_cast<double>(kFrames - 12000));
    const double outputRms = std::sqrt(outputEnergy / static_cast<double>(kFrames - 12000));
    INFO("drive RMS in " << inputRms << ", out " << outputRms);
    REQUIRE(outputRms > inputRms * 0.5);
    REQUIRE(outputRms < inputRms * 2.0);

    drumas::fx::DriveEffect naive(false);
    naive.prepare(spec());
    naive.setParameter(kDriveAmount, 0.8F);
    naive.setParameter(kDriveTone, 16000.0F);
    naive.setParameter(kDriveOutput, 0.0F);
    std::vector<float> naiveLeft(kFrames, 0.0F);
    std::vector<float> naiveRight(kFrames, 0.0F);
    for (int index = 0; index < kFrames; ++index) {
        naiveLeft[static_cast<std::size_t>(index)] =
            0.1F * static_cast<float>(std::sin(2.0 * std::numbers::pi * frequency *
                                               static_cast<double>(index) / kSampleRate));
        naiveRight[static_cast<std::size_t>(index)] =
            naiveLeft[static_cast<std::size_t>(index)];
    }
    for (int offset = 0; offset < kFrames; offset += kBlock) {
        const int frames = std::min(kBlock, kFrames - offset);
        naive.process(naiveLeft.data() + offset, naiveRight.data() + offset, frames);
    }

    const double oversampledAlias =
        20.0 * std::log10((toneMagnitude(left, 13000.0) + 1e-30) /
                          (toneMagnitude(left, 7000.0) + 1e-30));
    const double naiveAlias =
        20.0 * std::log10((toneMagnitude(naiveLeft, 13000.0) + 1e-30) /
                          (toneMagnitude(naiveLeft, 7000.0) + 1e-30));
    INFO("alias at 13 kHz: 2x " << oversampledAlias << " dBc, naive " << naiveAlias
                                << " dBc, improvement " << (naiveAlias - oversampledAlias)
                                << " dB");
    REQUIRE(oversampledAlias < naiveAlias - 3.0);
}

TEST_CASE("a long silent filter tail does not become a denormal CPU spike",
          "[fx][denormal]") {
    drumas::fx::FilterEffect filter;
    filter.prepare(spec());
    filter.setParameter(kFilterCutoff, 180.0F);
    filter.setParameter(kFilterResonance, 1.0F);
    filter.setParameter(kFilterMode, 0.0F);

    std::vector<float> left(kBlock, 0.0F);
    std::vector<float> right(kBlock, 0.0F);
    left[0] = 1.0F;
    right[0] = 1.0F;
    for (int block = 0; block < 1000; ++block) {
        filter.process(left.data(), right.data(), kBlock);
        std::fill(left.begin(), left.end(), 0.0F);
        std::fill(right.begin(), right.end(), 0.0F);
    }

    // Warm caches, then compare a run near the start of a silent tail with the
    // same work after the filter state has decayed for many seconds.
    auto measure = [&]() {
        const auto start = std::chrono::steady_clock::now();
        for (int block = 0; block < 400; ++block) {
            std::fill(left.begin(), left.end(), 0.0F);
            std::fill(right.begin(), right.end(), 0.0F);
            filter.process(left.data(), right.data(), kBlock);
        }
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - start)
            .count();
    };
    const double first = measure();
    for (int block = 0; block < 4000; ++block) {
        std::fill(left.begin(), left.end(), 0.0F);
        std::fill(right.begin(), right.end(), 0.0F);
        filter.process(left.data(), right.data(), kBlock);
    }
    const double last = measure();
    float tailPeak = 0.0F;
    for (const float sample : left) tailPeak = std::max(tailPeak, std::fabs(sample));
    INFO("silent-tail block time: " << first << " ms vs " << last << " ms"
                                     << ", cutoff " << filter.currentCutoff()
                                     << ", resonance " << filter.currentResonance()
                                     << ", tail peak " << tailPeak);
    for (const float sample : left) {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::fabs(sample) < 1.0e-5F);
    }
    // A denormal tail can be one to two orders of magnitude slower. Five times
    // plus a small absolute allowance catches that without depending on the
    // machine's absolute speed.
    REQUIRE(last < first * 5.0 + 2.0);
}

TEST_CASE("master limiter enforces its ceiling and reports look-ahead latency",
          "[fx][limiter]") {
    drumas::fx::MasterLimiter limiter;
    limiter.prepare(spec());
    REQUIRE(limiter.lookaheadMs() == Catch::Approx(1.0F));
    REQUIRE(limiter.releaseMs() == Catch::Approx(80.0F));
    INFO("limiter latency: " << limiter.latencySamples()
                             << " samples = " << (1000.0 * limiter.latencySamples() /
                                                  kSampleRate)
                             << " ms");
    REQUIRE(limiter.latencySamples() >= 47);
    REQUIRE(limiter.latencySamples() <= 49);

    std::vector<float> left(kBlock, 2.0F);
    std::vector<float> right(kBlock, -2.0F);
    float peak = 0.0F;
    for (int block = 0; block < 200; ++block) {
        std::fill(left.begin(), left.end(), 2.0F);
        std::fill(right.begin(), right.end(), -2.0F);
        limiter.process(left.data(), right.data(), kBlock);
        for (int index = 0; index < kBlock; ++index) {
            REQUIRE(std::isfinite(left[static_cast<std::size_t>(index)]));
            peak = std::max(peak, std::fabs(left[static_cast<std::size_t>(index)]));
        }
    }
    INFO("limiter maximum output: " << peak);
    const float ceiling = std::pow(10.0F, -1.0F / 20.0F);
    REQUIRE(peak <= ceiling + 1.0e-4F);
}
