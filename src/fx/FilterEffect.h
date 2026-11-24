// Resonant multimode filter insert: low-pass, high-pass and band-pass.
#pragma once

#include <algorithm>
#include <cmath>

#include <dspark/Core/AudioSpec.h>
#include <dspark/Core/StateVariableFilter.h>

#include "core/DenormalGuard.h"
#include "core/ParameterRegistry.h"
#include "core/SmoothedValue.h"
#include "fx/EffectTypes.h"
#include "fx/IEffect.h"

namespace drumas::fx {

class FilterEffect final : public IEffect {
public:
    FilterEffect() {
        using core::ParameterCurve;
        using core::ParameterDescriptor;
        using core::ParameterUnit;
        const ParameterDescriptor descriptors[] = {
            {kCutoff, "filter.cutoff", 20.0F, 20000.0F, 4000.0F, ParameterUnit::Hertz,
             ParameterCurve::Logarithmic},
            {kResonance, "filter.resonance", 0.0F, 1.0F, 0.15F, ParameterUnit::Percent,
             ParameterCurve::Linear},
            {kMode, "filter.mode", 0.0F, 2.0F, 0.0F, ParameterUnit::None,
             ParameterCurve::Linear},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kFilterType; }
    [[nodiscard]] const char* name() const noexcept override { return "filter"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        const dspark::AudioSpec dsparkSpec{sampleRate_, std::max(spec.maxBlockSize, 1), 2};
        filterLeft_.prepare(dsparkSpec);
        filterRight_.prepare(dsparkSpec);

        cutoff_.prepare(sampleRate_, 10.0);
        cutoff_.setExponential();
        resonance_.prepare(sampleRate_, 10.0);
        cutoff_.reset(registry_.defaultValue(kCutoff));
        resonance_.reset(registry_.defaultValue(kResonance));
        filterLeft_.setCutoff(cutoff_.current());
        filterLeft_.setResonance(resonance_.current());
        filterRight_.setCutoff(cutoff_.current());
        filterRight_.setResonance(resonance_.current());
        setMode(registry_.defaultValue(kMode));
        tailSamples_ = static_cast<int>(sampleRate_ * 0.1);
    }

    void reset() noexcept override {
        filterLeft_.reset();
        filterRight_.reset();
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        const core::DenormalGuard denormalGuard;
        float peak = 0.0F;
        for (int index = 0; index < numFrames; ++index) {
            const float cutoff = cutoff_.getNextValue();
            const float resonance = resonance_.getNextValue();
            filterLeft_.setCutoff(cutoff);
            filterLeft_.setResonance(resonance);
            filterRight_.setCutoff(cutoff);
            filterRight_.setResonance(resonance);

            float outLeft = filterLeft_.processSample(left[index], 0);
            float outRight = filterRight_.processSample(right[index], 0);
            if (!std::isfinite(outLeft)) {
                filterLeft_.reset();
                outLeft = 0.0F;
            }
            if (!std::isfinite(outRight)) {
                filterRight_.reset();
                outRight = 0.0F;
            }
            left[index] = outLeft;
            right[index] = outRight;
            peak = std::max(peak, std::max(std::fabs(outLeft), std::fabs(outRight)));
        }
        tailLevel_ = peak;
    }

    [[nodiscard]] int tailSamples() const noexcept override { return tailSamples_; }
    [[nodiscard]] float tailLevel() const noexcept override { return tailLevel_; }
    [[nodiscard]] const core::ParameterRegistry& parameters() const noexcept override {
        return registry_;
    }

    /// Diagnostics used by the filter response test.
    [[nodiscard]] float currentCutoff() const noexcept { return cutoff_.current(); }
    [[nodiscard]] float currentResonance() const noexcept { return resonance_.current(); }

    void setParameter(core::ParameterId id, float value) noexcept override {
        if (!registry_.clampToRange(id, value)) return;
        switch (id) {
            case kCutoff:
                cutoff_.setTargetValue(value);
                break;
            case kResonance:
                resonance_.setTargetValue(value);
                break;
            case kMode:
                setMode(value);
                break;
            default:
                break;
        }
    }

private:
    void setMode(float value) noexcept {
        const int mode = std::clamp(static_cast<int>(value + 0.5F), 0, 2);
        const auto dsparkMode = mode == 1   ? dspark::StateVariableFilter<float>::Mode::HighPass
                                : mode == 2 ? dspark::StateVariableFilter<float>::Mode::BandPass
                                            : dspark::StateVariableFilter<float>::Mode::LowPass;
        filterLeft_.setMode(dsparkMode);
        filterRight_.setMode(dsparkMode);
    }

    inline static const core::ParameterId kCutoff = core::makeParameterId("filter.cutoff");
    inline static const core::ParameterId kResonance =
        core::makeParameterId("filter.resonance");
    inline static const core::ParameterId kMode = core::makeParameterId("filter.mode");

    dspark::StateVariableFilter<float> filterLeft_;
    dspark::StateVariableFilter<float> filterRight_;
    core::SmoothedValue cutoff_;
    core::SmoothedValue resonance_;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    int tailSamples_ = 0;
    float tailLevel_ = 0.0F;
};

}  // namespace drumas::fx
