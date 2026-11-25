// Control-side value mirror of the effect chain description.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

#include "fx/ChainDescription.h"

namespace drumas::bridge {

class ChainMirror {
public:
    ChainMirror() noexcept : description_(fx::defaultChainDescription()) {}

    [[nodiscard]] fx::ChainDescription snapshot() const noexcept { return description_; }

    [[nodiscard]] bool setEffectType(std::size_t slot, fx::EffectTypeId type) noexcept {
        if (slot >= description_.slots.size()) {
            return false;
        }
        description_.slots[slot].effectType = type;
        return true;
    }

    [[nodiscard]] bool setBypass(std::size_t slot, bool bypassed) noexcept {
        if (slot >= description_.slots.size()) {
            return false;
        }
        description_.slots[slot].bypass = bypassed;
        return true;
    }

    [[nodiscard]] bool setMix(std::size_t slot, float mix) noexcept {
        if (slot >= description_.slots.size() || !std::isfinite(mix)) {
            return false;
        }
        description_.slots[slot].mix = std::clamp(mix, 0.0F, 1.0F);
        return true;
    }

    [[nodiscard]] bool setParameter(std::size_t slot, core::ParameterId id,
                                   float value) noexcept {
        if (slot >= description_.slots.size() || !std::isfinite(value)) {
            return false;
        }
        return fx::setParameterValue(description_.slots[slot], id, value);
    }

private:
    fx::ChainDescription description_;
};

}  // namespace drumas::bridge
