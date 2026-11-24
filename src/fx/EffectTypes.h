// Stable ids for the effect types exposed by this stage.
#pragma once

#include <string_view>

#include "core/ParameterRegistry.h"
#include "fx/IEffect.h"

namespace drumas::fx {

inline constexpr EffectTypeId kReverbType = core::makeParameterId("fx.reverb");
inline constexpr EffectTypeId kDelayType = core::makeParameterId("fx.delay");
inline constexpr EffectTypeId kChorusType = core::makeParameterId("fx.chorus");
inline constexpr EffectTypeId kDriveType = core::makeParameterId("fx.drive");
inline constexpr EffectTypeId kFilterType = core::makeParameterId("fx.filter");
inline constexpr EffectTypeId kLimiterType = core::makeParameterId("fx.limiter");

[[nodiscard]] constexpr std::string_view effectTypeName(EffectTypeId type) noexcept {
    if (type == kReverbType) return "reverb";
    if (type == kDelayType) return "delay";
    if (type == kChorusType) return "chorus";
    if (type == kDriveType) return "drive";
    if (type == kFilterType) return "filter";
    if (type == kLimiterType) return "limiter";
    return "none";
}

}  // namespace drumas::fx
