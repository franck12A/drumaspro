// Common contract for every insert effect.
//
// The chain owns the effect object, but only the control thread creates,
// prepares and destroys it. Once handed to the audio thread an effect is used
// exclusively there: process()/reset()/setParameter() must not allocate, lock,
// log or touch the device. Everything that needs memory is done in prepare().
#pragma once

#include <cstdint>

#include "core/AudioTypes.h"
#include "core/ParameterRegistry.h"

namespace drumas::fx {

/// Stable effect type identifier, hashed from a name exactly like a parameter id.
using EffectTypeId = std::uint32_t;
inline constexpr EffectTypeId kNoEffectType = 0;

class IEffect {
public:
    IEffect() = default;
    virtual ~IEffect() = default;

    IEffect(const IEffect&) = delete;
    IEffect& operator=(const IEffect&) = delete;
    IEffect(IEffect&&) = delete;
    IEffect& operator=(IEffect&&) = delete;

    [[nodiscard]] virtual EffectTypeId typeId() const noexcept = 0;
    [[nodiscard]] virtual const char* name() const noexcept = 0;

    /// Control thread. Allocates every buffer the effect will ever need.
    virtual void prepare(const core::AudioSpec& spec) = 0;

    /// Audio thread. Clears delay lines and recursive state.
    virtual void reset() noexcept = 0;

    /// Audio thread. Processes exactly `numFrames` stereo samples in place.
    /// The effect is 100 % wet; the chain applies dry/wet and bypass.
    virtual void process(float* left, float* right, int numFrames) noexcept = 0;

    /// Total processing delay in samples at the base sample rate. Look-ahead
    /// limiters and oversampled paths report their real group delay.
    [[nodiscard]] virtual int latencySamples() const noexcept { return 0; }

    /// Upper bound for the audible tail, in samples, after the input stops.
    [[nodiscard]] virtual int tailSamples() const noexcept { return 0; }

    /// Audio thread, diagnostics for the tail policy: peak of the last wet
    /// block. A chain can use it to stop processing a bypassed effect once its
    /// tail is inaudible.
    [[nodiscard]] virtual float tailLevel() const noexcept { return 0.0F; }

    /// Control thread after construction, read-only from the audio thread.
    [[nodiscard]] virtual const core::ParameterRegistry& parameters() const noexcept = 0;

    /// Audio thread. Value is clamped against the effect's own registry.
    virtual void setParameter(core::ParameterId id, float value) noexcept = 0;
};

}  // namespace drumas::fx
