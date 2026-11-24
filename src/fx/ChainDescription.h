// Pure data description of the six-slot effect chain.
//
// No pointers, no virtual calls, no hidden allocation: the same struct can be
// copied into a preset, sent over a queue or compared in a test. Parameter
// values are real (not normalised) and identified by the stable ids published
// by each effect's registry.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "core/ParameterRegistry.h"
#include "fx/EffectTypes.h"
#include "fx/IEffect.h"

namespace drumas::fx {

inline constexpr std::size_t kEffectSlotCount = 6;
inline constexpr std::size_t kMaxParametersPerEffect = 8;

struct EffectParameterValue {
    core::ParameterId id = core::kInvalidParameterId;
    float value = 0.0F;
};

struct EffectSlotDescription {
    EffectTypeId effectType = kNoEffectType;
    bool bypass = true;
    float mix = 0.5F;
    std::uint8_t parameterCount = 0;
    std::array<EffectParameterValue, kMaxParametersPerEffect> parameters{};
};

struct ChainDescription {
    std::array<EffectSlotDescription, kEffectSlotCount> slots{};
    /// Reserved for tempo-synced delay divisions. The delay currently exposes
    /// its time in milliseconds; this value is carried so presets and the
    /// future sync mode do not need a schema change.
    double tempoBpm = 120.0;
};

static_assert(std::is_trivially_copyable_v<EffectParameterValue>);
static_assert(std::is_trivially_copyable_v<EffectSlotDescription>);
static_assert(std::is_trivially_copyable_v<ChainDescription>);

[[nodiscard]] inline const EffectParameterValue* findParameter(
    const EffectSlotDescription& slot, core::ParameterId id) noexcept {
    for (std::size_t index = 0; index < slot.parameterCount; ++index) {
        if (slot.parameters[index].id == id) return &slot.parameters[index];
    }
    return nullptr;
}

[[nodiscard]] inline bool setParameterValue(EffectSlotDescription& slot, core::ParameterId id,
                                            float value) noexcept {
    for (std::size_t index = 0; index < slot.parameterCount; ++index) {
        if (slot.parameters[index].id == id) {
            slot.parameters[index].value = value;
            return true;
        }
    }
    if (slot.parameterCount >= kMaxParametersPerEffect) return false;
    slot.parameters[slot.parameterCount++] = EffectParameterValue{id, value};
    return true;
}

/// Default pedalboard of stage 5. Every slot starts bypassed so the instrument
/// sounds like stage 4 until the player enables an effect; slot 6 is a second
/// filter so a six-effect benchmark and audition are possible.
[[nodiscard]] inline ChainDescription defaultChainDescription() noexcept {
    ChainDescription description;
    description.slots[0].effectType = kReverbType;
    description.slots[1].effectType = kDelayType;
    description.slots[2].effectType = kChorusType;
    description.slots[3].effectType = kDriveType;
    description.slots[4].effectType = kFilterType;
    description.slots[5].effectType = kFilterType;
    return description;
}

}  // namespace drumas::fx
