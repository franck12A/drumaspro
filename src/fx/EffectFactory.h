// Allocation-on-the-control-thread factory for effect types.
#pragma once

#include <memory>
#include <utility>

#include "core/AudioTypes.h"
#include "fx/ChorusEffect.h"
#include "fx/DelayEffect.h"
#include "fx/DriveEffect.h"
#include "fx/EffectTypes.h"
#include "fx/FilterEffect.h"
#include "fx/IEffect.h"
#include "fx/ReverbEffect.h"

namespace drumas::fx {

[[nodiscard]] inline std::unique_ptr<IEffect> createEffect(EffectTypeId type) {
    switch (type) {
        case kReverbType:
            return std::make_unique<ReverbEffect>();
        case kDelayType:
            return std::make_unique<DelayEffect>();
        case kChorusType:
            return std::make_unique<ChorusEffect>();
        case kDriveType:
            return std::make_unique<DriveEffect>();
        case kFilterType:
            return std::make_unique<FilterEffect>();
        default:
            return nullptr;
    }
}

/// Control thread. Builds and prepares an effect with all of its allocations
/// before it is handed to the audio thread.
[[nodiscard]] inline std::unique_ptr<IEffect> createPreparedEffect(
    EffectTypeId type, const core::AudioSpec& spec) {
    auto effect = createEffect(type);
    if (effect != nullptr) effect->prepare(spec);
    return effect;
}

[[nodiscard]] inline bool isKnownEffectType(EffectTypeId type) noexcept {
    return type == kReverbType || type == kDelayType || type == kChorusType ||
           type == kDriveType || type == kFilterType;
}

}  // namespace drumas::fx
