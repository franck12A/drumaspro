// Stereo delay with true ping-pong cross-feedback and a low-pass in the loop.
//
// The feedback path is deliberately cross-fed (ping-pong = 1) instead of using
// two independent delays: an impulse in the left channel answers in the right
// channel on the next repeat, which is what makes the effect read as ping-pong.
#pragma once

#include <algorithm>
#include <cmath>

#include <dspark/Core/AudioSpec.h>
#include <dspark/Core/RingBuffer.h>

#include "core/DenormalGuard.h"
#include "core/ParameterRegistry.h"
#include "core/SmoothedValue.h"
#include "fx/EffectTypes.h"
#include "fx/IEffect.h"

namespace drumas::fx {

class DelayEffect final : public IEffect {
public:
    DelayEffect() {
        using core::ParameterCurve;
        using core::ParameterDescriptor;
        using core::ParameterUnit;
        const ParameterDescriptor descriptors[] = {
            {kTimeMs, "delay.time", 10.0F, 2000.0F, 350.0F, ParameterUnit::Milliseconds,
             ParameterCurve::Logarithmic},
            {kFeedback, "delay.feedback", 0.0F, 0.95F, 0.35F, ParameterUnit::Percent,
             ParameterCurve::Linear},
            {kTone, "delay.tone", 200.0F, 16000.0F, 6000.0F, ParameterUnit::Hertz,
             ParameterCurve::Logarithmic},
            {kPingPong, "delay.pingPong", 0.0F, 1.0F, 1.0F, ParameterUnit::Percent,
             ParameterCurve::Linear},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kDelayType; }
    [[nodiscard]] const char* name() const noexcept override { return "delay"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        maxDelaySamples_ = static_cast<int>(sampleRate_ * 2.0) + 8;
        left_.prepare(maxDelaySamples_ + 4);
        right_.prepare(maxDelaySamples_ + 4);

        time_.prepare(sampleRate_, 50.0);
        time_.setExponential();
        feedback_.prepare(sampleRate_, 10.0);
        tone_.prepare(sampleRate_, 10.0);
        tone_.setExponential();
        pingPong_.prepare(sampleRate_, 10.0);

        time_.reset(registry_.defaultValue(kTimeMs));
        feedback_.reset(registry_.defaultValue(kFeedback));
        tone_.reset(registry_.defaultValue(kTone));
        pingPong_.reset(registry_.defaultValue(kPingPong));
        timeSamplesTarget_ = time_.current() * static_cast<float>(sampleRate_) / 1000.0F;
        time_.reset(timeSamplesTarget_);
        lowPassLeft_ = 0.0F;
        lowPassRight_ = 0.0F;
        tailSamples_ = static_cast<int>(sampleRate_ * 20.0);
    }

    void reset() noexcept override {
        left_.reset();
        right_.reset();
        lowPassLeft_ = 0.0F;
        lowPassRight_ = 0.0F;
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        const core::DenormalGuard denormalGuard;
        float peak = 0.0F;

        for (int index = 0; index < numFrames; ++index) {
            const float feedback = feedback_.getNextValue();
            const float pingPong = pingPong_.getNextValue();
            const float tone = tone_.getNextValue();
            const float toneCoefficient = std::min(
                1.0F, 6.283185307F * tone / static_cast<float>(sampleRate_));
            const float delaySamples = std::clamp(time_.getNextValue(), 1.0F,
                                                  static_cast<float>(maxDelaySamples_ - 4));
            const float wetLeft = left_.readInterpolated<dspark::InterpMethod::Hermite>(
                delaySamples);
            const float wetRight = right_.readInterpolated<dspark::InterpMethod::Hermite>(
                delaySamples);

            const float crossLeft = wetLeft * (1.0F - pingPong) + wetRight * pingPong;
            const float crossRight = wetRight * (1.0F - pingPong) + wetLeft * pingPong;
            lowPassLeft_ += toneCoefficient * (crossLeft - lowPassLeft_);
            lowPassRight_ += toneCoefficient * (crossRight - lowPassRight_);

            const float writeLeft = left[index] + lowPassLeft_ * feedback;
            const float writeRight = right[index] + lowPassRight_ * feedback;
            if (!std::isfinite(writeLeft) || !std::isfinite(writeRight)) {
                reset();
                left[index] = 0.0F;
                right[index] = 0.0F;
                continue;
            }
            left_.push(writeLeft);
            right_.push(writeRight);

            left[index] = wetLeft;
            right[index] = wetRight;
            peak = std::max(peak, std::max(std::fabs(wetLeft), std::fabs(wetRight)));
        }
        tailLevel_ = peak;
    }

    [[nodiscard]] int tailSamples() const noexcept override { return tailSamples_; }
    [[nodiscard]] float tailLevel() const noexcept override { return tailLevel_; }
    [[nodiscard]] const core::ParameterRegistry& parameters() const noexcept override {
        return registry_;
    }

    void setParameter(core::ParameterId id, float value) noexcept override {
        if (!registry_.clampToRange(id, value)) return;
        switch (id) {
            case kTimeMs:
                time_.setTargetValue(value * static_cast<float>(sampleRate_) / 1000.0F);
                break;
            case kFeedback:
                feedback_.setTargetValue(value);
                break;
            case kTone:
                tone_.setTargetValue(value);
                break;
            case kPingPong:
                pingPong_.setTargetValue(value);
                break;
            default:
                break;
        }
    }

private:
    inline static const core::ParameterId kTimeMs = core::makeParameterId("delay.time");
    inline static const core::ParameterId kFeedback =
        core::makeParameterId("delay.feedback");
    inline static const core::ParameterId kTone = core::makeParameterId("delay.tone");
    inline static const core::ParameterId kPingPong =
        core::makeParameterId("delay.pingPong");

    dspark::RingBuffer<float> left_;
    dspark::RingBuffer<float> right_;
    core::SmoothedValue time_;
    core::SmoothedValue feedback_;
    core::SmoothedValue tone_;
    core::SmoothedValue pingPong_;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    int maxDelaySamples_ = 0;
    int tailSamples_ = 0;
    float timeSamplesTarget_ = 0.0F;
    float lowPassLeft_ = 0.0F;
    float lowPassRight_ = 0.0F;
    float tailLevel_ = 0.0F;
};

}  // namespace drumas::fx
