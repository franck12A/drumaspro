// Master look-ahead limiter, the final gain stage before the device.
#pragma once

#include <algorithm>
#include <cmath>

#include <dspark/Core/AudioBuffer.h>
#include <dspark/Core/AudioSpec.h>
#include <dspark/Effects/Limiter.h>

#include "core/AudioTypes.h"
#include "fx/SafetyClip.h"

namespace drumas::fx {

class MasterLimiter {
public:
    static constexpr float kCeilingDb = -1.0F;
    static constexpr float kReleaseMs = 80.0F;
    static constexpr float kLookaheadMs = 1.0F;

    void prepare(const core::AudioSpec& spec) {
        const double sampleRate =
            spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        const int channels = std::clamp(spec.channels, 1, 2);
        limiter_.setCeiling(kCeilingDb);
        limiter_.setRelease(kReleaseMs);
        limiter_.setLookahead(kLookaheadMs);
        limiter_.setTruePeak(false);
        limiter_.setAdaptiveRelease(false);
        limiter_.setSafetyClip(false);
        limiter_.prepare(sampleRate, channels, kLookaheadMs);
    }

    void reset() noexcept { limiter_.reset(); }

    void process(float* left, float* right, int numFrames) noexcept {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        float* channels[2] = {left, right};
        dspark::AudioBufferView<float> view(channels, 2, numFrames);
        limiter_.processBlock(view);
        for (int index = 0; index < numFrames; ++index) {
            left[index] = SafetyClip::processSample(left[index]);
            right[index] = SafetyClip::processSample(right[index]);
        }
    }

    [[nodiscard]] int latencySamples() const noexcept { return limiter_.getLatency(); }
    [[nodiscard]] float ceilingDb() const noexcept { return kCeilingDb; }
    [[nodiscard]] float releaseMs() const noexcept { return kReleaseMs; }
    [[nodiscard]] float lookaheadMs() const noexcept { return kLookaheadMs; }

private:
    dspark::Limiter<float> limiter_;
};

}  // namespace drumas::fx
