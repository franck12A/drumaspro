// ADSR envelope. DSPark's ADSREnvelope provides the per-sample recursion and the
// stage machine; this class adapts it to IEnvelope and to our stage enum.
#pragma once

#include <dspark/Core/EnvelopeGenerator.h>

#include "synth/IEnvelope.h"

namespace drumas::synth {

class AdsrEnvelope final : public IEnvelope {
public:
    void prepare(double sampleRate) noexcept override {
        impl_.prepare(sampleRate);
        impl_.reset();
    }

    void reset() noexcept override { impl_.reset(); }

    void noteOn() noexcept override { impl_.noteOn(); }

    void noteOff() noexcept override { impl_.noteOff(); }

    void setParameters(const AdsrParameters& parameters) noexcept override {
        impl_.setParameters(parameters.attackMs, parameters.decayMs, parameters.sustain,
                            parameters.releaseMs);
    }

    [[nodiscard]] float nextValue() noexcept override { return impl_.getNextValue(); }

    [[nodiscard]] float level() const noexcept override { return impl_.getCurrentValue(); }

    [[nodiscard]] EnvelopeStage stage() const noexcept override {
        switch (impl_.getState()) {
            case dspark::ADSREnvelope<float>::State::Idle:
                return EnvelopeStage::Idle;
            case dspark::ADSREnvelope<float>::State::Attack:
                return EnvelopeStage::Attack;
            case dspark::ADSREnvelope<float>::State::Decay:
                return EnvelopeStage::Decay;
            case dspark::ADSREnvelope<float>::State::Sustain:
                return EnvelopeStage::Sustain;
            case dspark::ADSREnvelope<float>::State::Release:
                return EnvelopeStage::Release;
        }
        return EnvelopeStage::Idle;
    }

    [[nodiscard]] bool isActive() const noexcept override { return impl_.isActive(); }

private:
    dspark::ADSREnvelope<float> impl_;
};

}  // namespace drumas::synth
