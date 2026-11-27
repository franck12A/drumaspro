// Effects added in stage 5B: modulation (flanger, phaser, tremolo, vibrato),
// a three-band tone EQ and a lo-fi/vinyl colour.
//
// All of them are plain stereo in-place processors written here: no new
// dependency, no allocation after prepare(), no locking, no I/O. Parameter
// changes are smoothed with core::SmoothedValue so moving a control does not
// click. Delays use a power-of-two circular buffer and linear interpolation.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/DenormalGuard.h"
#include "core/ParameterRegistry.h"
#include "core/SmoothedValue.h"
#include "fx/EffectTypes.h"
#include "fx/IEffect.h"

namespace drumas::fx {
namespace detail {

/// Power-of-two circular delay line with fractional reads. Allocation happens
/// only in prepare().
class FractionalDelay {
public:
    void prepare(double sampleRate, double maxDelayMs) {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : core::kDefaultSampleRate;
        const auto needed = static_cast<std::size_t>(
            std::ceil(sampleRate_ * maxDelayMs * 0.001)) + 4;
        std::size_t size = 8;
        while (size < needed) {
            size <<= 1U;
        }
        buffer_.assign(size, 0.0F);
        mask_ = size - 1;
        write_ = 0;
    }

    void reset() noexcept {
        std::fill(buffer_.begin(), buffer_.end(), 0.0F);
        write_ = 0;
    }

    void write(float sample) noexcept {
        buffer_[write_] = sample;
        write_ = (write_ + 1) & mask_;
    }

    [[nodiscard]] float read(double delaySamples) const noexcept {
        const double clamped =
            std::clamp(delaySamples, 1.0, static_cast<double>(mask_ - 2));
        const double position = static_cast<double>(write_) - clamped;
        const auto base = static_cast<std::size_t>(std::floor(position));
        const float fraction = static_cast<float>(position - std::floor(position));
        const std::size_t index0 = base & mask_;
        const std::size_t index1 = (index0 + 1) & mask_;
        return buffer_[index0] + (buffer_[index1] - buffer_[index0]) * fraction;
    }

    [[nodiscard]] bool ready() const noexcept { return !buffer_.empty(); }

private:
    std::vector<float> buffer_;
    std::size_t mask_ = 0;
    std::size_t write_ = 0;
    double sampleRate_ = core::kDefaultSampleRate;
};

/// Sine/triangle/square LFO in 0..1, advanced per sample.
class UnipolarLfo {
public:
    void prepare(double sampleRate) noexcept {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : core::kDefaultSampleRate;
        phase_ = 0.0;
    }

    void reset() noexcept { phase_ = 0.0; }

    void setRate(float hz) noexcept {
        const double clamped = std::clamp(static_cast<double>(hz), 0.01, 40.0);
        increment_ = clamped / sampleRate_;
    }

    /// Advances the phase once. Two channels must call advance() once and then
    /// value()/valueAtOffset() so the stereo pair stays at the same rate.
    void advance() noexcept {
        phase_ += increment_;
        if (phase_ >= 1.0) {
            phase_ -= 1.0;
        }
    }

    /// phaseOffset in turns (0..1) for stereo widening.
    [[nodiscard]] float value(float phaseOffset, int shape) const noexcept {
        const double shifted = phase_ + static_cast<double>(phaseOffset);
        const double wrapped = shifted - std::floor(shifted);
        switch (shape) {
            case 1:  // triangle
                return static_cast<float>(1.0 - 4.0 * std::fabs(wrapped - 0.5));
            case 2:  // square
                return wrapped < 0.5 ? 1.0F : 0.0F;
            default:
                return static_cast<float>(0.5 + 0.5 * std::sin(wrapped * 6.283185307179586));
        }
    }

    [[nodiscard]] float next(int shape = 0) noexcept {
        advance();
        return value(0.0F, shape);
    }

private:
    double sampleRate_ = core::kDefaultSampleRate;
    double phase_ = 0.0;
    double increment_ = 0.0;
};

/// RBJ biquad, direct form I. Coefficients are recomputed on the control values,
/// never per sample.
struct Biquad {
    float b0 = 1.0F;
    float b1 = 0.0F;
    float b2 = 0.0F;
    float a1 = 0.0F;
    float a2 = 0.0F;

    void setLowShelf(double sampleRate, double frequency, double gainDb) noexcept {
        const double a = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * 3.141592653589793 * frequency / sampleRate;
        const double cosW = std::cos(w);
        const double alpha = std::sin(w) / (2.0 * std::sqrt(2.0));
        const double twoSqrtAAlpha = 2.0 * std::sqrt(a) * alpha;
        const double a0 = (a + 1.0) + (a - 1.0) * cosW + twoSqrtAAlpha;
        b0 = static_cast<float>((a * ((a + 1.0) - (a - 1.0) * cosW + twoSqrtAAlpha)) / a0);
        b1 = static_cast<float>((2.0 * a * ((a - 1.0) - (a + 1.0) * cosW)) / a0);
        b2 = static_cast<float>((a * ((a + 1.0) - (a - 1.0) * cosW - twoSqrtAAlpha)) / a0);
        a1 = static_cast<float>((-2.0 * ((a - 1.0) + (a + 1.0) * cosW)) / a0);
        a2 = static_cast<float>(((a + 1.0) + (a - 1.0) * cosW - twoSqrtAAlpha) / a0);
    }

    void setHighShelf(double sampleRate, double frequency, double gainDb) noexcept {
        const double a = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * 3.141592653589793 * frequency / sampleRate;
        const double cosW = std::cos(w);
        const double alpha = std::sin(w) / (2.0 * std::sqrt(2.0));
        const double twoSqrtAAlpha = 2.0 * std::sqrt(a) * alpha;
        const double a0 = (a + 1.0) - (a - 1.0) * cosW + twoSqrtAAlpha;
        b0 = static_cast<float>((a * ((a + 1.0) + (a - 1.0) * cosW + twoSqrtAAlpha)) / a0);
        b1 = static_cast<float>((-2.0 * a * ((a - 1.0) + (a + 1.0) * cosW)) / a0);
        b2 = static_cast<float>((a * ((a + 1.0) + (a - 1.0) * cosW - twoSqrtAAlpha)) / a0);
        a1 = static_cast<float>((2.0 * ((a - 1.0) - (a + 1.0) * cosW)) / a0);
        a2 = static_cast<float>(((a + 1.0) - (a - 1.0) * cosW - twoSqrtAAlpha) / a0);
    }

    void setPeaking(double sampleRate, double frequency, double q, double gainDb) noexcept {
        const double a = std::pow(10.0, gainDb / 40.0);
        const double w = 2.0 * 3.141592653589793 * frequency / sampleRate;
        const double cosW = std::cos(w);
        const double alpha = std::sin(w) / (2.0 * std::max(q, 0.1));
        const double a0 = 1.0 + alpha / a;
        b0 = static_cast<float>((1.0 + alpha * a) / a0);
        b1 = static_cast<float>((-2.0 * cosW) / a0);
        b2 = static_cast<float>((1.0 - alpha * a) / a0);
        a1 = static_cast<float>((-2.0 * cosW) / a0);
        a2 = static_cast<float>((1.0 - alpha / a) / a0);
    }

    struct State {
        float x1 = 0.0F;
        float x2 = 0.0F;
        float y1 = 0.0F;
        float y2 = 0.0F;
    };

    [[nodiscard]] float process(float input, State& state) const noexcept {
        const float output = b0 * input + b1 * state.x1 + b2 * state.x2 -
                             a1 * state.y1 - a2 * state.y2;
        state.x2 = state.x1;
        state.x1 = input;
        state.y2 = state.y1;
        state.y1 = output;
        return output;
    }
};

}  // namespace detail

// ---------------------------------------------------------------------------
// Tremolo: amplitude modulation, optional stereo phase offset.
// ---------------------------------------------------------------------------
class TremoloEffect final : public IEffect {
public:
    TremoloEffect() {
        const core::ParameterDescriptor descriptors[] = {
            {kRate, "tremolo.rate", 0.05F, 20.0F, 5.0F, core::ParameterUnit::Hertz,
             core::ParameterCurve::Logarithmic},
            {kDepth, "tremolo.depth", 0.0F, 1.0F, 0.5F, core::ParameterUnit::Percent,
             core::ParameterCurve::Linear},
            {kShape, "tremolo.shape", 0.0F, 2.0F, 0.0F, core::ParameterUnit::None,
             core::ParameterCurve::Linear},
            {kStereo, "tremolo.stereo", 0.0F, 1.0F, 0.0F, core::ParameterUnit::Percent,
             core::ParameterCurve::Linear},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kTremoloType; }
    [[nodiscard]] const char* name() const noexcept override { return "tremolo"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        lfo_.prepare(sampleRate_);
        lfo_.setRate(registry_.defaultValue(kRate));
        depth_.prepare(sampleRate_, 10.0);
        depth_.reset(registry_.defaultValue(kDepth));
        stereo_.prepare(sampleRate_, 10.0);
        stereo_.reset(registry_.defaultValue(kStereo));
        shape_ = 0;
        tailSamples_ = 0;
    }

    void reset() noexcept override {
        lfo_.reset();
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        const core::DenormalGuard guard;
        float peak = 0.0F;
        for (int index = 0; index < numFrames; ++index) {
            const float depth = depth_.getNextValue();
            const float phaseOffset = 0.25F * stereo_.getNextValue();
            lfo_.advance();
            const float modLeft = lfo_.value(0.0F, shape_);
            const float modRight = lfo_.value(phaseOffset, shape_);
            // depth = 0 keeps unity gain; depth = 1 dips to silence.
            const float gainLeft = 1.0F - depth * (1.0F - modLeft);
            const float gainRight = 1.0F - depth * (1.0F - modRight);
            left[index] *= gainLeft;
            right[index] *= gainRight;
            peak = std::max(peak, std::max(std::fabs(left[index]), std::fabs(right[index])));
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
                lfo_.setRate(value);
                break;
            case kDepth:
                depth_.setTargetValue(value);
                break;
            case kShape:
                shape_ = std::clamp(static_cast<int>(value + 0.5F), 0, 2);
                break;
            case kStereo:
                stereo_.setTargetValue(value);
                break;
            default:
                break;
        }
    }

private:
    inline static const core::ParameterId kRate = core::makeParameterId("tremolo.rate");
    inline static const core::ParameterId kDepth = core::makeParameterId("tremolo.depth");
    inline static const core::ParameterId kShape = core::makeParameterId("tremolo.shape");
    inline static const core::ParameterId kStereo = core::makeParameterId("tremolo.stereo");

    detail::UnipolarLfo lfo_;
    core::SmoothedValue depth_;
    core::SmoothedValue stereo_;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    int shape_ = 0;
    int tailSamples_ = 0;
    float tailLevel_ = 0.0F;
};

// ---------------------------------------------------------------------------
// Vibrato: modulated short delay (pitch wobble).
// ---------------------------------------------------------------------------
class VibratoEffect final : public IEffect {
public:
    VibratoEffect() {
        const core::ParameterDescriptor descriptors[] = {
            {kRate, "vibrato.rate", 0.05F, 10.0F, 5.0F, core::ParameterUnit::Hertz,
             core::ParameterCurve::Logarithmic},
            {kDepthMs, "vibrato.depth", 0.0F, 5.0F, 1.5F, core::ParameterUnit::Milliseconds,
             core::ParameterCurve::Linear},
            {kDelayMs, "vibrato.delay", 5.0F, 20.0F, 8.0F, core::ParameterUnit::Milliseconds,
             core::ParameterCurve::Linear},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kVibratoType; }
    [[nodiscard]] const char* name() const noexcept override { return "vibrato"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        lfo_.prepare(sampleRate_);
        lfo_.setRate(registry_.defaultValue(kRate));
        delayLeft_.prepare(sampleRate_, 24.0);
        delayRight_.prepare(sampleRate_, 24.0);
        depth_.prepare(sampleRate_, 10.0);
        depth_.reset(registry_.defaultValue(kDepthMs));
        baseDelay_.prepare(sampleRate_, 10.0);
        baseDelay_.reset(registry_.defaultValue(kDelayMs));
        tailSamples_ = static_cast<int>(sampleRate_ * 0.03);
    }

    void reset() noexcept override {
        delayLeft_.reset();
        delayRight_.reset();
        lfo_.reset();
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        const core::DenormalGuard guard;
        float peak = 0.0F;
        for (int index = 0; index < numFrames; ++index) {
            const float baseMs = baseDelay_.getNextValue();
            const float depthMs = depth_.getNextValue();
            // Bipolar modulation around the base delay; both channels share it.
            const float modulation = lfo_.next() * 2.0F - 1.0F;
            const double delaySamples =
                static_cast<double>(baseMs + modulation * depthMs) * sampleRate_ * 0.001;
            const float outLeft = delayLeft_.read(delaySamples);
            const float outRight = delayRight_.read(delaySamples);
            delayLeft_.write(left[index]);
            delayRight_.write(right[index]);
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

    void setParameter(core::ParameterId id, float value) noexcept override {
        if (!registry_.clampToRange(id, value)) return;
        switch (id) {
            case kRate:
                lfo_.setRate(value);
                break;
            case kDepthMs:
                depth_.setTargetValue(value);
                break;
            case kDelayMs:
                baseDelay_.setTargetValue(value);
                break;
            default:
                break;
        }
    }

private:
    inline static const core::ParameterId kRate = core::makeParameterId("vibrato.rate");
    inline static const core::ParameterId kDepthMs = core::makeParameterId("vibrato.depth");
    inline static const core::ParameterId kDelayMs = core::makeParameterId("vibrato.delay");

    detail::UnipolarLfo lfo_;
    detail::FractionalDelay delayLeft_;
    detail::FractionalDelay delayRight_;
    core::SmoothedValue depth_;
    core::SmoothedValue baseDelay_;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    int tailSamples_ = 0;
    float tailLevel_ = 0.0F;
};

// ---------------------------------------------------------------------------
// Flanger: short modulated delay with feedback, mixed with dry.
// ---------------------------------------------------------------------------
class FlangerEffect final : public IEffect {
public:
    FlangerEffect() {
        const core::ParameterDescriptor descriptors[] = {
            {kRate, "flanger.rate", 0.05F, 5.0F, 0.25F, core::ParameterUnit::Hertz,
             core::ParameterCurve::Logarithmic},
            {kDepthMs, "flanger.depth", 0.0F, 8.0F, 2.0F, core::ParameterUnit::Milliseconds,
             core::ParameterCurve::Linear},
            {kFeedback, "flanger.feedback", 0.0F, 0.9F, 0.4F, core::ParameterUnit::Percent,
             core::ParameterCurve::Linear},
            {kDelayMs, "flanger.delay", 0.5F, 10.0F, 2.5F, core::ParameterUnit::Milliseconds,
             core::ParameterCurve::Linear},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kFlangerType; }
    [[nodiscard]] const char* name() const noexcept override { return "flanger"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        lfo_.prepare(sampleRate_);
        lfo_.setRate(registry_.defaultValue(kRate));
        delayLeft_.prepare(sampleRate_, 24.0);
        delayRight_.prepare(sampleRate_, 24.0);
        depth_.prepare(sampleRate_, 10.0);
        depth_.reset(registry_.defaultValue(kDepthMs));
        baseDelay_.prepare(sampleRate_, 10.0);
        baseDelay_.reset(registry_.defaultValue(kDelayMs));
        feedback_.prepare(sampleRate_, 10.0);
        feedback_.reset(registry_.defaultValue(kFeedback));
        tailSamples_ = static_cast<int>(sampleRate_ * 0.15);
    }

    void reset() noexcept override {
        delayLeft_.reset();
        delayRight_.reset();
        lfo_.reset();
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        const core::DenormalGuard guard;
        float peak = 0.0F;
        for (int index = 0; index < numFrames; ++index) {
            const float baseMs = baseDelay_.getNextValue();
            const float depthMs = depth_.getNextValue();
            const float feedback = feedback_.getNextValue();
            lfo_.advance();
            const float modulation = lfo_.value(0.0F, 0);
            // A quarter-turn offset between channels widens the comb.
            const float modulationRight = lfo_.value(0.25F, 0);
            const double delayLeftSamples =
                static_cast<double>(baseMs + modulation * depthMs) * sampleRate_ * 0.001;
            const double delayRightSamples =
                static_cast<double>(baseMs + modulationRight * depthMs) * sampleRate_ * 0.001;

            const float wetLeft = delayLeft_.read(delayLeftSamples);
            const float wetRight = delayRight_.read(delayRightSamples);
            delayLeft_.write(left[index] + wetLeft * feedback);
            delayRight_.write(right[index] + wetRight * feedback);

            const float outLeft = 0.7F * (left[index] + wetLeft);
            const float outRight = 0.7F * (right[index] + wetRight);
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

    void setParameter(core::ParameterId id, float value) noexcept override {
        if (!registry_.clampToRange(id, value)) return;
        switch (id) {
            case kRate:
                lfo_.setRate(value);
                break;
            case kDepthMs:
                depth_.setTargetValue(value);
                break;
            case kFeedback:
                feedback_.setTargetValue(value);
                break;
            case kDelayMs:
                baseDelay_.setTargetValue(value);
                break;
            default:
                break;
        }
    }

private:
    inline static const core::ParameterId kRate = core::makeParameterId("flanger.rate");
    inline static const core::ParameterId kDepthMs = core::makeParameterId("flanger.depth");
    inline static const core::ParameterId kFeedback = core::makeParameterId("flanger.feedback");
    inline static const core::ParameterId kDelayMs = core::makeParameterId("flanger.delay");

    detail::UnipolarLfo lfo_;
    detail::FractionalDelay delayLeft_;
    detail::FractionalDelay delayRight_;
    core::SmoothedValue depth_;
    core::SmoothedValue baseDelay_;
    core::SmoothedValue feedback_;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    int tailSamples_ = 0;
    float tailLevel_ = 0.0F;
};

// ---------------------------------------------------------------------------
// Phaser: four cascaded first-order all-passes per channel, LFO-swept.
// ---------------------------------------------------------------------------
class PhaserEffect final : public IEffect {
public:
    PhaserEffect() {
        const core::ParameterDescriptor descriptors[] = {
            {kRate, "phaser.rate", 0.05F, 5.0F, 0.3F, core::ParameterUnit::Hertz,
             core::ParameterCurve::Logarithmic},
            {kDepth, "phaser.depth", 0.0F, 1.0F, 0.7F, core::ParameterUnit::Percent,
             core::ParameterCurve::Linear},
            {kFeedback, "phaser.feedback", 0.0F, 0.9F, 0.3F, core::ParameterUnit::Percent,
             core::ParameterCurve::Linear},
            {kFrequency, "phaser.frequency", 100.0F, 4000.0F, 700.0F,
             core::ParameterUnit::Hertz, core::ParameterCurve::Logarithmic},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kPhaserType; }
    [[nodiscard]] const char* name() const noexcept override { return "phaser"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        lfo_.prepare(sampleRate_);
        lfo_.setRate(registry_.defaultValue(kRate));
        depth_.prepare(sampleRate_, 10.0);
        depth_.reset(registry_.defaultValue(kDepth));
        feedback_.prepare(sampleRate_, 10.0);
        feedback_.reset(registry_.defaultValue(kFeedback));
        frequency_.prepare(sampleRate_, 10.0);
        frequency_.setExponential();
        frequency_.reset(registry_.defaultValue(kFrequency));
        tailSamples_ = static_cast<int>(sampleRate_ * 0.05);
    }

    void reset() noexcept override {
        for (auto& channel : state_) {
            channel.fill(0.0F);
        }
        feedbackLeft_ = 0.0F;
        feedbackRight_ = 0.0F;
        lfo_.reset();
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        const core::DenormalGuard guard;
        float peak = 0.0F;
        // The sweep coefficient is recomputed once per block (5 ms): far faster
        // than the LFO can move and much cheaper than four tan() calls per
        // sample, while the parameter ramps still advance per sample.
        const float modulation = lfo_.next() * 2.0F - 1.0F;

        for (int index = 0; index < numFrames; ++index) {
            const float depth = depth_.getNextValue();
            const float feedback = feedback_.getNextValue();
            const float baseFrequency = frequency_.getNextValue();
            const float frequency =
                std::clamp(baseFrequency * std::exp2(modulation * depth * 2.0F), 60.0F,
                           static_cast<float>(sampleRate_ * 0.45));
            const float tangent = std::tan(3.141592653589793F * frequency /
                                           static_cast<float>(sampleRate_));
            const float coefficient = (tangent - 1.0F) / (tangent + 1.0F);
            float dryLeft = left[index] + feedbackLeft_ * feedback;
            float dryRight = right[index] + feedbackRight_ * feedback;
            for (int stage = 0; stage < kStages; ++stage) {
                const float inputLeft = dryLeft;
                dryLeft = -coefficient * inputLeft + state_[0][static_cast<std::size_t>(stage)] +
                          coefficient * state_[1][static_cast<std::size_t>(stage)];
                state_[0][static_cast<std::size_t>(stage)] = inputLeft;
                state_[1][static_cast<std::size_t>(stage)] = dryLeft;

                const float inputRight = dryRight;
                dryRight = -coefficient * inputRight + state_[2][static_cast<std::size_t>(stage)] +
                           coefficient * state_[3][static_cast<std::size_t>(stage)];
                state_[2][static_cast<std::size_t>(stage)] = inputRight;
                state_[3][static_cast<std::size_t>(stage)] = dryRight;
            }
            feedbackLeft_ = dryLeft;
            feedbackRight_ = dryRight;
            const float outLeft = 0.7F * (left[index] + dryLeft);
            const float outRight = 0.7F * (right[index] + dryRight);
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

    void setParameter(core::ParameterId id, float value) noexcept override {
        if (!registry_.clampToRange(id, value)) return;
        switch (id) {
            case kRate:
                lfo_.setRate(value);
                break;
            case kDepth:
                depth_.setTargetValue(value);
                break;
            case kFeedback:
                feedback_.setTargetValue(value);
                break;
            case kFrequency:
                frequency_.setTargetValue(value);
                break;
            default:
                break;
        }
    }

private:
    static constexpr int kStages = 4;
    inline static const core::ParameterId kRate = core::makeParameterId("phaser.rate");
    inline static const core::ParameterId kDepth = core::makeParameterId("phaser.depth");
    inline static const core::ParameterId kFeedback = core::makeParameterId("phaser.feedback");
    inline static const core::ParameterId kFrequency =
        core::makeParameterId("phaser.frequency");

    detail::UnipolarLfo lfo_;
    core::SmoothedValue depth_;
    core::SmoothedValue feedback_;
    core::SmoothedValue frequency_;
    /// [left in, left out, right in, right out] x stages.
    std::array<std::array<float, kStages>, 4> state_{};
    float feedbackLeft_ = 0.0F;
    float feedbackRight_ = 0.0F;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    int tailSamples_ = 0;
    float tailLevel_ = 0.0F;
};

// ---------------------------------------------------------------------------
// Three-band tone EQ: low shelf, peaking mid, high shelf.
// ---------------------------------------------------------------------------
class EqEffect final : public IEffect {
public:
    EqEffect() {
        const core::ParameterDescriptor descriptors[] = {
            {kLow, "eq.low", -12.0F, 12.0F, 0.0F, core::ParameterUnit::Decibels,
             core::ParameterCurve::Linear},
            {kMid, "eq.mid", -12.0F, 12.0F, 0.0F, core::ParameterUnit::Decibels,
             core::ParameterCurve::Linear},
            {kHigh, "eq.high", -12.0F, 12.0F, 0.0F, core::ParameterUnit::Decibels,
             core::ParameterCurve::Linear},
            {kMidFrequency, "eq.midFrequency", 200.0F, 5000.0F, 1000.0F,
             core::ParameterUnit::Hertz, core::ParameterCurve::Logarithmic},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kEqType; }
    [[nodiscard]] const char* name() const noexcept override { return "eq"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        targetLow_ = currentLow_ = registry_.defaultValue(kLow);
        targetMid_ = currentMid_ = registry_.defaultValue(kMid);
        targetHigh_ = currentHigh_ = registry_.defaultValue(kHigh);
        targetMidFrequency_ = currentMidFrequency_ = registry_.defaultValue(kMidFrequency);
        updateCoefficients();
        tailSamples_ = 0;
    }

    void reset() noexcept override {
        for (auto& state : lowState_) state = {};
        for (auto& state : midState_) state = {};
        for (auto& state : highState_) state = {};
        tailLevel_ = 0.0F;
        updateCoefficients();
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        const core::DenormalGuard guard;
        // Per-block one-pole on the gains: a full +/-12 dB move settles in
        // ~30 ms without recomputing coefficients per sample.
        const float previousLow = currentLow_;
        const float previousMid = currentMid_;
        const float previousHigh = currentHigh_;
        const float previousFrequency = currentMidFrequency_;
        currentLow_ += (targetLow_ - currentLow_) * 0.35F;
        currentMid_ += (targetMid_ - currentMid_) * 0.35F;
        currentHigh_ += (targetHigh_ - currentHigh_) * 0.35F;
        currentMidFrequency_ += (targetMidFrequency_ - currentMidFrequency_) * 0.35F;
        if (currentLow_ != previousLow || currentMid_ != previousMid ||
            currentHigh_ != previousHigh || currentMidFrequency_ != previousFrequency) {
            updateCoefficients();
        }
        float peak = 0.0F;
        for (int index = 0; index < numFrames; ++index) {
            float sampleLeft = lowBand_.process(left[index], lowState_[0]);
            sampleLeft = midBand_.process(sampleLeft, midState_[0]);
            sampleLeft = highBand_.process(sampleLeft, highState_[0]);
            float sampleRight = lowBand_.process(right[index], lowState_[1]);
            sampleRight = midBand_.process(sampleRight, midState_[1]);
            sampleRight = highBand_.process(sampleRight, highState_[1]);
            left[index] = sampleLeft;
            right[index] = sampleRight;
            peak = std::max(peak, std::max(std::fabs(sampleLeft), std::fabs(sampleRight)));
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
            case kLow:
                targetLow_ = value;
                break;
            case kMid:
                targetMid_ = value;
                break;
            case kHigh:
                targetHigh_ = value;
                break;
            case kMidFrequency:
                targetMidFrequency_ = value;
                break;
            default:
                break;
        }
    }

private:
    void updateCoefficients() noexcept {
        lowBand_.setLowShelf(sampleRate_, 200.0, static_cast<double>(currentLow_));
        midBand_.setPeaking(sampleRate_, static_cast<double>(currentMidFrequency_), 0.9,
                            static_cast<double>(currentMid_));
        highBand_.setHighShelf(sampleRate_, 4000.0, static_cast<double>(currentHigh_));
    }

    inline static const core::ParameterId kLow = core::makeParameterId("eq.low");
    inline static const core::ParameterId kMid = core::makeParameterId("eq.mid");
    inline static const core::ParameterId kHigh = core::makeParameterId("eq.high");
    inline static const core::ParameterId kMidFrequency =
        core::makeParameterId("eq.midFrequency");

    detail::Biquad lowBand_;
    detail::Biquad midBand_;
    detail::Biquad highBand_;
    std::array<detail::Biquad::State, 2> lowState_{};
    std::array<detail::Biquad::State, 2> midState_{};
    std::array<detail::Biquad::State, 2> highState_{};
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    float targetLow_ = 0.0F;
    float targetMid_ = 0.0F;
    float targetHigh_ = 0.0F;
    float targetMidFrequency_ = 1000.0F;
    float currentLow_ = 0.0F;
    float currentMid_ = 0.0F;
    float currentHigh_ = 0.0F;
    float currentMidFrequency_ = 1000.0F;
    int tailSamples_ = 0;
    float tailLevel_ = 0.0F;
};

// ---------------------------------------------------------------------------
// Lo-fi: bit-depth reduction, sample-and-hold decimation, vinyl noise and a
// slow wow. The decimation is deliberate and documented: it aliases by design,
// which is the effect; the chain limiter still bounds the output.
// ---------------------------------------------------------------------------
class LoFiEffect final : public IEffect {
public:
    LoFiEffect() {
        const core::ParameterDescriptor descriptors[] = {
            {kBits, "lofi.bits", 4.0F, 16.0F, 10.0F, core::ParameterUnit::None,
             core::ParameterCurve::Linear},
            {kRate, "lofi.rate", 1000.0F, 48000.0F, 24000.0F, core::ParameterUnit::Hertz,
             core::ParameterCurve::Logarithmic},
            {kNoise, "lofi.noise", 0.0F, 1.0F, 0.08F, core::ParameterUnit::Percent,
             core::ParameterCurve::Linear},
            {kWow, "lofi.wow", 0.0F, 1.0F, 0.2F, core::ParameterUnit::Percent,
             core::ParameterCurve::Linear},
        };
        for (const auto& descriptor : descriptors) (void)registry_.add(descriptor);
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kLoFiType; }
    [[nodiscard]] const char* name() const noexcept override { return "lofi"; }

    void prepare(const core::AudioSpec& spec) override {
        sampleRate_ = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        wowLineLeft_.prepare(sampleRate_, 8.0);
        wowLineRight_.prepare(sampleRate_, 8.0);
        wowLfo_.prepare(sampleRate_);
        wowLfo_.setRate(0.4F);
        quantizeStep_ = stepForBits(registry_.defaultValue(kBits));
        holdSamples_ = std::max(1, static_cast<int>(sampleRate_ / registry_.defaultValue(kRate)));
        noise_.prepare(sampleRate_, 10.0);
        noise_.reset(registry_.defaultValue(kNoise));
        wow_.prepare(sampleRate_, 10.0);
        wow_.reset(registry_.defaultValue(kWow));
        tailSamples_ = static_cast<int>(sampleRate_ * 0.02);
    }

    void reset() noexcept override {
        wowLineLeft_.reset();
        wowLineRight_.reset();
        wowLfo_.reset();
        holdCounter_ = 0;
        heldLeft_ = 0.0F;
        heldRight_ = 0.0F;
        tailLevel_ = 0.0F;
    }

    void process(float* left, float* right, int numFrames) noexcept override {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        const core::DenormalGuard guard;
        float peak = 0.0F;
        for (int index = 0; index < numFrames; ++index) {
            const float wow = wow_.getNextValue();
            const float noise = noise_.getNextValue();
            const float wowModulation = (wowLfo_.next() * 2.0F - 1.0F) * wow;
            const double wowDelay = 1.5 * sampleRate_ * 0.001 * wowModulation +
                                    2.0 * sampleRate_ * 0.001;

            const float wobbledLeft = wowLineLeft_.read(wowDelay);
            const float wobbledRight = wowLineRight_.read(wowDelay);
            wowLineLeft_.write(left[index]);
            wowLineRight_.write(right[index]);

            // Sample-and-hold decimation, then quantisation.
            if (holdCounter_ == 0) {
                heldLeft_ = quantise(wobbledLeft);
                heldRight_ = quantise(wobbledRight);
                holdCounter_ = std::max(1, holdSamples_);
            }
            --holdCounter_;

            const float noiseLeft = nextNoise() * noise * 0.02F;
            const float noiseRight = nextNoise() * noise * 0.02F;
            left[index] = heldLeft_ + noiseLeft;
            right[index] = heldRight_ + noiseRight;
            peak = std::max(peak, std::max(std::fabs(left[index]), std::fabs(right[index])));
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
            case kBits:
                quantizeStep_ = stepForBits(value);
                break;
            case kRate:
                holdSamples_ = std::max(1, static_cast<int>(sampleRate_ / value));
                holdCounter_ = std::min(holdCounter_, holdSamples_);
                break;
            case kNoise:
                noise_.setTargetValue(value);
                break;
            case kWow:
                wow_.setTargetValue(value);
                break;
            default:
                break;
        }
    }

private:
    [[nodiscard]] static float stepForBits(float bits) noexcept {
        const float levels = std::exp2(std::clamp(bits, 4.0F, 16.0F));
        return 2.0F / levels;
    }

    [[nodiscard]] float quantise(float sample) const noexcept {
        const float clamped = std::clamp(sample, -1.0F, 1.0F);
        return quantizeStep_ > 0.0F ? std::round(clamped / quantizeStep_) * quantizeStep_
                                    : clamped;
    }

    [[nodiscard]] float nextNoise() noexcept {
        noiseState_ ^= noiseState_ << 13U;
        noiseState_ ^= noiseState_ >> 17U;
        noiseState_ ^= noiseState_ << 5U;
        return static_cast<float>(static_cast<std::int32_t>(noiseState_)) *
               (1.0F / 2147483648.0F);
    }

    inline static const core::ParameterId kBits = core::makeParameterId("lofi.bits");
    inline static const core::ParameterId kRate = core::makeParameterId("lofi.rate");
    inline static const core::ParameterId kNoise = core::makeParameterId("lofi.noise");
    inline static const core::ParameterId kWow = core::makeParameterId("lofi.wow");

    detail::FractionalDelay wowLineLeft_;
    detail::FractionalDelay wowLineRight_;
    detail::UnipolarLfo wowLfo_;
    core::SmoothedValue noise_;
    core::SmoothedValue wow_;
    core::ParameterRegistry registry_;
    double sampleRate_ = core::kDefaultSampleRate;
    float quantizeStep_ = 0.002F;
    int holdSamples_ = 2;
    int holdCounter_ = 0;
    float heldLeft_ = 0.0F;
    float heldRight_ = 0.0F;
    std::uint32_t noiseState_ = 0x9E3779B9U;
    int tailSamples_ = 0;
    float tailLevel_ = 0.0F;
};

}  // namespace drumas::fx
