// Fixed-capacity parameter values mirrored from the control side.
#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>

#include "core/ParameterRegistry.h"

namespace drumas::bridge {

struct ParameterValue {
    core::ParameterDescriptor descriptor{};
    float value = 0.0F;
};

struct ParameterSnapshot {
    std::array<ParameterValue, core::ParameterRegistry::kMaxParameters> values{};
    std::size_t count = 0;
};

class ParameterMirror {
public:
    /// Control thread, before publishing the mirror to readers.
    void initialize(const core::ParameterRegistry& registry) noexcept {
        count_ = registry.size();
        for (std::size_t index = 0; index < count_; ++index) {
            entries_[index].descriptor = registry.at(index);
            entries_[index].value.store(registry.at(index).defaultValue,
                                        std::memory_order_relaxed);
        }
    }

    /// Control thread. Unknown ids and NaN are rejected; other values are
    /// clamped to the registered parameter's real range.
    [[nodiscard]] bool set(core::ParameterId id, float value) noexcept {
        if (std::isnan(value)) {
            return false;
        }
        for (std::size_t index = 0; index < count_; ++index) {
            Entry& entry = entries_[index];
            const auto& descriptor = entry.descriptor;
            if (descriptor.id == id) {
                value = value < descriptor.minValue
                            ? descriptor.minValue
                            : (value > descriptor.maxValue ? descriptor.maxValue : value);
                entry.value.store(value, std::memory_order_release);
                return true;
            }
        }
        return false;
    }

    /// Any thread after initialize().
    [[nodiscard]] bool get(core::ParameterId id, float& value) const noexcept {
        for (std::size_t index = 0; index < count_; ++index) {
            const Entry& entry = entries_[index];
            if (entry.descriptor.id == id) {
                value = entry.value.load(std::memory_order_acquire);
                return true;
            }
        }
        return false;
    }

    /// UI/control-thread snapshot. Descriptors are immutable after initialize.
    [[nodiscard]] ParameterSnapshot snapshot() const noexcept {
        ParameterSnapshot result;
        result.count = count_;
        for (std::size_t index = 0; index < count_; ++index) {
            result.values[index] =
                ParameterValue{entries_[index].descriptor,
                               entries_[index].value.load(std::memory_order_acquire)};
        }
        return result;
    }

private:
    struct Entry {
        core::ParameterDescriptor descriptor{};
        std::atomic<float> value{0.0F};
    };

    std::array<Entry, core::ParameterRegistry::kMaxParameters> entries_{};
    std::size_t count_ = 0;
};

}  // namespace drumas::bridge
