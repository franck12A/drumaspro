// Oscillator abstraction. Implementations may wrap DSPark, a wavetable engine
// or anything else; voices only ever see this interface.
#pragma once

namespace drumas::synth {

class IOscillator {
public:
    IOscillator() = default;
    virtual ~IOscillator() = default;

    IOscillator(const IOscillator&) = delete;
    IOscillator& operator=(const IOscillator&) = delete;
    IOscillator(IOscillator&&) = delete;
    IOscillator& operator=(IOscillator&&) = delete;

    /// Setup thread. Not real-time safe in general (implementations may build
    /// lookup tables), never concurrent with the audio thread.
    virtual void prepare(double sampleRate) noexcept = 0;

    /// Audio thread. Phase and internal state go back to their initial values.
    virtual void reset() noexcept = 0;

    /// Audio thread. Frequency in Hz, clamped internally to [0, Nyquist].
    virtual void setFrequency(float hz) noexcept = 0;

    /// Audio thread. Next sample in [-1, 1].
    [[nodiscard]] virtual float nextSample() noexcept = 0;
};

}  // namespace drumas::synth
