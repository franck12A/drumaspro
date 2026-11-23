// Envelope abstraction and the parameter bundle every ADSR-like envelope needs.
#pragma once

#include <cstdint>

namespace drumas::synth {

/// ADSR settings in the units a UI would expose.
struct AdsrParameters {
    float attackMs = 5.0F;
    float decayMs = 120.0F;
    float sustain = 0.7F;  // 0..1
    float releaseMs = 180.0F;
};

/// Envelope phase, mirrored by implementations so tests and diagnostics do not
/// depend on a DSP library's own enum.
enum class EnvelopeStage : std::uint8_t {
    Idle = 0,
    Attack,
    Decay,
    Sustain,
    Release,
};

class IEnvelope {
public:
    IEnvelope() = default;
    virtual ~IEnvelope() = default;

    IEnvelope(const IEnvelope&) = delete;
    IEnvelope& operator=(const IEnvelope&) = delete;
    IEnvelope(IEnvelope&&) = delete;
    IEnvelope& operator=(IEnvelope&&) = delete;

    /// Setup thread.
    virtual void prepare(double sampleRate) noexcept = 0;

    /// Audio thread. Back to Idle with a zero output.
    virtual void reset() noexcept = 0;

    /// Audio thread. Starts the attack from the current level (legato, so that
    /// retriggering a sounding voice does not produce a step).
    virtual void noteOn() noexcept = 0;

    /// Audio thread. Starts the release from the current level.
    virtual void noteOff() noexcept = 0;

    /// Audio thread. Recomputes per-sample rates; no allocation.
    virtual void setParameters(const AdsrParameters& parameters) noexcept = 0;

    /// Audio thread. Advances the state machine one sample, returns 0..1.
    [[nodiscard]] virtual float nextValue() noexcept = 0;

    [[nodiscard]] virtual float level() const noexcept = 0;
    [[nodiscard]] virtual EnvelopeStage stage() const noexcept = 0;

    /// True until the envelope has reached Idle after a release.
    [[nodiscard]] virtual bool isActive() const noexcept = 0;
};

}  // namespace drumas::synth
