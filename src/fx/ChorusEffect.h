// Stereo chorus wrapper around DSPark's four-voice chorus.
#pragma once

#include <algorithm>
#include <cmath>

#include <dspark/Core/AudioBuffer.h>
#include <dspark/Core/AudioSpec.h>
#include <dspark/Effects/Chorus.h>

#include "core/ParameterRegistry.h"
#include "fx/EffectTypes.h"
#include "fx/IEffect.h"

namespace drumas::fx {

class ChorusEffect final : public IEffect {
public:
    ChorusEffect() {
        using core::ParameterCurve;
        using core::ParameterDescriptor;
        using core::ParameterUnit;
        const ParameterDescriptor descriptors[] = {
            {kRate, "chorus.rate", 0.05F, 5.0F, 0.5F, ParameterUnit::Hertz,
             ParameterCurve::Logarithmic},
            {kDepthMs, "chorus.depth", 0.5F, 20.0F, 6.0F, ParameterUnit::Milliseconds,
             ParameterCurve::Linear},
            {kVoices, "chorus.voices", 1.0F, 4.0F, 3.0F, ParameterUnit::None,
             ParameterCurve::Linear},
            {kSpread, "chorus.spread", 0.0F, 1.0F, 0.7F, ParameterUnit::Percent,
             ParameterCurve::Linear},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kChorusType; }
    [[nodiscard]] const char* name() const noexcept override { return "chorus"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        chorus_.setMix(1.0F);
        chorus_.setCenterDelay(12.0F);
        chorus_.setFeedback(0.0F);
        chorus_.setRate(registry_.defaultValue(kRate));
        chorus_.setDepthMs(registry_.defaultValue(kDepthMs));
        chorus_.setVoices(static_cast<int>(registry_.defaultValue(kVoices)));
        chorus_.setStereoSpread(registry_.defaultValue(kSpread));
        const dspark::AudioSpec dsparkSpec{sampleRate_, std::max(spec.maxBlockSize, 1), 2};
        chorus_.prepare(dsparkSpec);
        chorus_.setMix(1.0F);
        chorus_.reset();
        tailSamples_ = static_cast<int>(sampleRate_ * 0.06);
    }

    void reset() noexcept override {
        chorus_.reset();
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        float* channels[2] = {left, right};
        dspark::AudioBufferView<float> view(channels, 2, numFrames);
        chorus_.processBlock(view);

        float peak = 0.0F;
        for (int index = 0; index < numFrames; ++index) {
            const float absLeft = std::fabs(left[index]);
            const float absRight = std::fabs(right[index]);
            if (!std::isfinite(left[index]) || !std::isfinite(right[index])) {
                left[index] = 0.0F;
                right[index] = 0.0F;
            } else {
                peak = std::max(peak, std::max(absLeft, absRight));
            }
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
            case kRate:
                chorus_.setRate(value);
                break;
            case kDepthMs:
                chorus_.setDepthMs(value);
                break;
            case kVoices:
                chorus_.setVoices(static_cast<int>(value + 0.5F));
                break;
            case kSpread:
                chorus_.setStereoSpread(value);
                break;
            default:
                break;
        }
    }

private:
    inline static const core::ParameterId kRate = core::makeParameterId("chorus.rate");
    inline static const core::ParameterId kDepthMs = core::makeParameterId("chorus.depth");
    inline static const core::ParameterId kVoices = core::makeParameterId("chorus.voices");
    inline static const core::ParameterId kSpread = core::makeParameterId("chorus.spread");

    dspark::Chorus<float> chorus_;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    int tailSamples_ = 0;
    float tailLevel_ = 0.0F;
};

}  // namespace drumas::fx
