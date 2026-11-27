// Stable ids for the effect types exposed by this stage.
#pragma once

#include <array>
#include <string_view>

#include "core/ParameterRegistry.h"
#include "fx/IEffect.h"

namespace drumas::fx {

inline constexpr EffectTypeId kReverbType = core::makeParameterId("fx.reverb");
inline constexpr EffectTypeId kDelayType = core::makeParameterId("fx.delay");
inline constexpr EffectTypeId kChorusType = core::makeParameterId("fx.chorus");
inline constexpr EffectTypeId kDriveType = core::makeParameterId("fx.drive");
inline constexpr EffectTypeId kFilterType = core::makeParameterId("fx.filter");
inline constexpr EffectTypeId kFlangerType = core::makeParameterId("fx.flanger");
inline constexpr EffectTypeId kPhaserType = core::makeParameterId("fx.phaser");
inline constexpr EffectTypeId kTremoloType = core::makeParameterId("fx.tremolo");
inline constexpr EffectTypeId kVibratoType = core::makeParameterId("fx.vibrato");
inline constexpr EffectTypeId kEqType = core::makeParameterId("fx.eq");
inline constexpr EffectTypeId kLoFiType = core::makeParameterId("fx.lofi");
inline constexpr EffectTypeId kLimiterType = core::makeParameterId("fx.limiter");

[[nodiscard]] constexpr std::string_view effectTypeName(EffectTypeId type) noexcept {
    if (type == kReverbType) return "reverb";
    if (type == kDelayType) return "delay";
    if (type == kChorusType) return "chorus";
    if (type == kDriveType) return "drive";
    if (type == kFilterType) return "filter";
    if (type == kFlangerType) return "flanger";
    if (type == kPhaserType) return "phaser";
    if (type == kTremoloType) return "tremolo";
    if (type == kVibratoType) return "vibrato";
    if (type == kEqType) return "eq";
    if (type == kLoFiType) return "lofi";
    if (type == kLimiterType) return "limiter";
    return "none";
}

/// Single source of truth for the slot selector, the console cycle and the
/// bridge: adding an effect here is enough for every client to see it.
inline constexpr std::array<EffectTypeId, 11> kEffectTypeCycle{
    kReverbType, kDelayType,   kChorusType, kDriveType,  kFilterType, kFlangerType,
    kPhaserType, kTremoloType, kVibratoType, kEqType,     kLoFiType,
};

}  // namespace drumas::fx
