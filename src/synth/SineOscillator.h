// Sine oscillator. The DSP implementation is DSPark's band-limited oscillator;
// this class is the only place in the project that names it for oscillators.
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

    [[nodiscard]] float nextSample() noexcept override { return impl_.getNextSample(); }

private:
    dspark::Oscillator<float> impl_;
};

}  // namespace drumas::synth
