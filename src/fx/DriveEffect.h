// Saturated insert with 2x oversampling, pre/post tone filters and automatic
// RMS compensation.
#pragma once

#include <algorithm>
#include <cmath>

#include <dspark/Core/AudioBuffer.h>
#include <dspark/Core/AudioSpec.h>
#include <dspark/Core/DspMath.h>
#include <dspark/Core/Oversampling.h>
#include <dspark/Core/StateVariableFilter.h>

#include "core/DenormalGuard.h"
#include "core/ParameterRegistry.h"
#include "core/SmoothedValue.h"
#include "fx/EffectTypes.h"
#include "fx/IEffect.h"

namespace drumas::fx {

class DriveEffect final : public IEffect {
public:
    /// `oversampling` is exposed so the aliasing test can render the same
    /// nonlinearity with and without the 2x path. Production always uses 2x.
    explicit DriveEffect(bool oversampling = true)
        : oversampling_(oversampling ? 2 : 1, dspark::Oversampling<float>::Quality::Medium),
          oversamplingEnabled_(oversampling) {
        using core::ParameterCurve;
        using core::ParameterDescriptor;
        using core::ParameterUnit;
        const ParameterDescriptor descriptors[] = {
            {kDrive, "drive.amount", 0.0F, 1.0F, 0.35F, ParameterUnit::Percent,
             ParameterCurve::Linear},
            {kTone, "drive.tone", 500.0F, 16000.0F, 7000.0F, ParameterUnit::Hertz,
             ParameterCurve::Logarithmic},
            {kOutput, "drive.output", -12.0F, 12.0F, 0.0F, ParameterUnit::Decibels,
             ParameterCurve::Linear},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kDriveType; }
    [[nodiscard]] const char* name() const noexcept override { return "drive"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        const dspark::AudioSpec dsparkSpec{sampleRate_, std::max(spec.maxBlockSize, 1), 2};
        oversampling_.prepare(dsparkSpec);

        for (auto* filter : {&preLeft_, &preRight_}) {
            filter->prepare(dsparkSpec);
            filter->setMode(dspark::StateVariableFilter<float>::Mode::HighPass);
            filter->setCutoff(30.0F);
            filter->setResonance(0.0F);
        }
        for (auto* filter : {&toneLeft_, &toneRight_}) {
            filter->prepare(dsparkSpec);
            filter->setMode(dspark::StateVariableFilter<float>::Mode::LowPass);
            filter->setResonance(0.0F);
        }

        drive_.prepare(sampleRate_, 10.0);
        tone_.prepare(sampleRate_, 10.0);
        tone_.setExponential();
        outputTrim_.prepare(sampleRate_, 10.0);
        outputTrim_.setExponential();
        compensation_.prepare(sampleRate_, 20.0);
        compensation_.setExponential();

        drive_.reset(registry_.defaultValue(kDrive));
        tone_.reset(registry_.defaultValue(kTone));
        outputTrim_.reset(dbToLinear(registry_.defaultValue(kOutput)));
        compensation_.reset(1.0F);
        toneLeft_.setCutoff(tone_.current());
        toneRight_.setCutoff(tone_.current());
    }

    void reset() noexcept override {
        oversampling_.reset();
        preLeft_.reset();
        preRight_.reset();
        toneLeft_.reset();
        toneRight_.reset();
        compensation_.reset(1.0F);
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        const core::DenormalGuard denormalGuard;

        double inputEnergy = 0.0;
        for (int index = 0; index < numFrames; ++index) {
            if (!std::isfinite(left[index])) left[index] = 0.0F;
            if (!std::isfinite(right[index])) right[index] = 0.0F;
            inputEnergy += static_cast<double>(left[index]) * left[index] +
                           static_cast<double>(right[index]) * right[index];
            left[index] = preLeft_.processSample(left[index], 0);
            right[index] = preRight_.processSample(right[index], 0);
        }

        float* channels[2] = {left, right};
        dspark::AudioBufferView<float> view(channels, 2, numFrames);
        const float drive = drive_.getNextValue();
        const float preGain = 1.0F + drive * 29.0F;
        auto highRate = oversampling_.upsample(view);
        for (int channel = 0; channel < highRate.getNumChannels(); ++channel) {
            float* samples = highRate.getChannel(channel);
            for (int index = 0; index < highRate.getNumSamples(); ++index) {
                samples[index] = dspark::fastTanh(samples[index] * preGain);
            }
        }
        oversampling_.downsample(view);

        double outputEnergy = 0.0;
        float peak = 0.0F;
        for (int index = 0; index < numFrames; ++index) {
            const float cutoff = tone_.getNextValue();
            toneLeft_.setCutoff(cutoff);
            toneRight_.setCutoff(cutoff);
            const float comp = compensation_.getNextValue();
            const float trim = outputTrim_.getNextValue();
            const float postLeft = toneLeft_.processSample(left[index], 0);
            const float postRight = toneRight_.processSample(right[index], 0);
            outputEnergy += static_cast<double>(postLeft) * postLeft +
                            static_cast<double>(postRight) * postRight;
            left[index] = postLeft * comp * trim;
            right[index] = postRight * comp * trim;
            if (!std::isfinite(left[index]) || !std::isfinite(right[index])) {
                reset();
                left[index] = 0.0F;
                right[index] = 0.0F;
                continue;
            }
            peak = std::max(peak, std::max(std::fabs(left[index]), std::fabs(right[index])));
        }

        const double inputRms = std::sqrt(inputEnergy / static_cast<double>(numFrames * 2));
        const double outputRms = std::sqrt(outputEnergy / static_cast<double>(numFrames * 2));
        if (outputRms > 1.0e-9) {
            const float target = static_cast<float>(
                std::clamp(inputRms / outputRms, 0.05, 8.0));
            compensation_.setTargetValue(target);
        }
        tailLevel_ = peak;
    }

    [[nodiscard]] int latencySamples() const noexcept override {
        return oversamplingEnabled_ ? oversampling_.getLatency() : 0;
    }

    [[nodiscard]] float tailLevel() const noexcept override { return tailLevel_; }
    [[nodiscard]] const core::ParameterRegistry& parameters() const noexcept override {
        return registry_;
    }

    void setParameter(core::ParameterId id, float value) noexcept override {
        if (!registry_.clampToRange(id, value)) return;
        switch (id) {
            case kDrive:
                drive_.setTargetValue(value);
                break;
            case kTone:
                tone_.setTargetValue(value);
                break;
            case kOutput:
                outputTrim_.setTargetValue(dbToLinear(value));
                break;
            default:
                break;
        }
    }

private:
    [[nodiscard]] static float dbToLinear(float db) noexcept {
        return std::pow(10.0F, db / 20.0F);
    }

    inline static const core::ParameterId kDrive = core::makeParameterId("drive.amount");
    inline static const core::ParameterId kTone = core::makeParameterId("drive.tone");
    inline static const core::ParameterId kOutput = core::makeParameterId("drive.output");

    dspark::Oversampling<float> oversampling_;
    bool oversamplingEnabled_ = true;
    dspark::StateVariableFilter<float> preLeft_;
    dspark::StateVariableFilter<float> preRight_;
    dspark::StateVariableFilter<float> toneLeft_;
    dspark::StateVariableFilter<float> toneRight_;
    core::SmoothedValue drive_;
    core::SmoothedValue tone_;
    core::SmoothedValue outputTrim_;
    core::SmoothedValue compensation_;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    float tailLevel_ = 0.0F;
};

}  // namespace drumas::fx
