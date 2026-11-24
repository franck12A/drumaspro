// Stereo reverb wrapper around DSPark's 16-line FDN.
#pragma once

#include <algorithm>
#include <cmath>

#include <dspark/Core/AudioBuffer.h>
#include <dspark/Core/AudioSpec.h>
#include <dspark/Effects/AlgorithmicReverb.h>

#include "core/ParameterRegistry.h"
#include "fx/EffectTypes.h"
#include "fx/IEffect.h"

namespace drumas::fx {

class ReverbEffect final : public IEffect {
public:
    ReverbEffect() {
        using core::ParameterCurve;
        using core::ParameterDescriptor;
        using core::ParameterUnit;
        const ParameterDescriptor descriptors[] = {
            {kDecayMs, "reverb.decay", 0.2F, 20.0F, 2.0F, ParameterUnit::None,
             ParameterCurve::Logarithmic},
            {kSize, "reverb.size", 0.01F, 1.0F, 0.6F, ParameterUnit::Percent,
             ParameterCurve::Linear},
            {kDamping, "reverb.damping", 0.0F, 1.0F, 0.35F, ParameterUnit::Percent,
             ParameterCurve::Linear},
            {kPreDelay, "reverb.preDelay", 0.0F, 200.0F, 20.0F, ParameterUnit::Milliseconds,
             ParameterCurve::Linear},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kReverbType; }
    [[nodiscard]] const char* name() const noexcept override { return "reverb"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        const dspark::AudioSpec dsparkSpec{sampleRate_, std::max(spec.maxBlockSize, 1), 2};
        reverb_.setType(dspark::AlgorithmicReverb<float>::Type::Hall);
        reverb_.setQuality(dspark::AlgorithmicReverb<float>::Quality::Full);
        reverb_.prepare(dsparkSpec);
        reverb_.setMix(1.0F);

        reverb_.setDecay(registry_.defaultValue(kDecayMs));
        reverb_.setSize(registry_.defaultValue(kSize));
        reverb_.setDamping(registry_.defaultValue(kDamping));
        reverb_.setPreDelay(registry_.defaultValue(kPreDelay));
        reverb_.reset();
        tailSamples_ = static_cast<int>(sampleRate_ * 30.0);
    }

    void reset() noexcept override {
        reverb_.reset();
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        float* channels[2] = {left, right};
        dspark::AudioBufferView<float> view(channels, 2, numFrames);
        reverb_.processBlock(view);

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
            case kDecayMs:
                reverb_.setDecay(value);
                break;
            case kSize:
                reverb_.setSize(value);
                break;
            case kDamping:
                reverb_.setDamping(value);
                break;
            case kPreDelay:
                reverb_.setPreDelay(value);
                break;
            default:
                break;
        }
    }

private:
    inline static const core::ParameterId kDecayMs = core::makeParameterId("reverb.decay");
    inline static const core::ParameterId kSize = core::makeParameterId("reverb.size");
    inline static const core::ParameterId kDamping = core::makeParameterId("reverb.damping");
    inline static const core::ParameterId kPreDelay = core::makeParameterId("reverb.preDelay");

    dspark::AlgorithmicReverb<float> reverb_;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    int tailSamples_ = 0;
    float tailLevel_ = 0.0F;
};

}  // namespace drumas::fx
