// Legacy sine oscillator kept only while the stage-2 sine voice is being
// replaced. New code uses BandLimitedOscillator; this class disappears with
// SineVoice.
#pragma once

#include <dspark/Core/Oscillator.h>

#include "synth/IOscillator.h"

namespace drumas::synth {

class SineOscillator final : public IOscillator {
public:
    void prepare(double sampleRate) noexcept override {
        impl_.prepare(sampleRate);
        impl_.setWaveform(dspark::Oscillator<float>::Waveform::Sine);
        impl_.reset();
    }

    void reset() noexcept override { impl_.reset(); }

    void setFrequency(float hz) noexcept override { impl_.setFrequency(hz); }

    void setWaveform(Waveform waveform) noexcept override {
        switch (waveform) {
            case Waveform::Sine:
                impl_.setWaveform(dspark::Oscillator<float>::Waveform::Sine);
                break;
            case Waveform::Triangle:
                impl_.setWaveform(dspark::Oscillator<float>::Waveform::Triangle);
                break;
            case Waveform::Saw:
                impl_.setWaveform(dspark::Oscillator<float>::Waveform::Saw);
                break;
            case Waveform::Square:
                impl_.setWaveform(dspark::Oscillator<float>::Waveform::Square);
                break;
        }
    }

    /// DSPark's oscillator has no pulse width; this implementation exists only
    /// for the legacy sine voice, so the request is ignored.
    void setPulseWidth(float /*width*/) noexcept override {}

    void setPhase(float phase) noexcept override { impl_.setPhase(phase); }

    [[nodiscard]] float nextSample() noexcept override { return impl_.getNextSample(); }

private:
    dspark::Oscillator<float> impl_;
};

}  // namespace drumas::synth
