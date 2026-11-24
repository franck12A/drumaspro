// Oscillator abstraction. Implementations may wrap DSPark, a wavetable engine
// or anything else; voices only ever see this interface.
#pragma once

#include <cstdint>

namespace drumas::synth {

/// Waveform family used by the synth. The values are part of the parameter
/// contract (`osc1.waveform` / `osc2.waveform` in the registry): never reorder.
enum class Waveform : std::uint8_t {
    Sine = 0,
    Triangle = 1,
    Saw = 2,
    Square = 3,  ///< Pulse: symmetric at 0.5, width controlled by setPulseWidth().
};

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

    /// Audio thread. Selects the waveform.
    virtual void setWaveform(Waveform waveform) noexcept = 0;

    /// Audio thread. Pulse duty cycle for Square, clamped to [0.05, 0.95].
    virtual void setPulseWidth(float width) noexcept = 0;

    /// Audio thread. Forces the normalized phase, in [0, 1).
    virtual void setPhase(float phase) noexcept = 0;

    /// Audio thread. Next sample, nominally in [-1, 1] (band-limited overshoot
    /// on narrow pulses may exceed it slightly).
    [[nodiscard]] virtual float nextSample() noexcept = 0;
};

}  // namespace drumas::synth
