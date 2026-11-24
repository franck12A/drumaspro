// Named, ranged parameter definitions shared by the synth, the effects and
// (from a later stage) the UI.
//
// A parameter is identified by a stable 32-bit id derived at compile time from
// its name (`makeParameterId`), so an id can travel in a core::Command and be
// stored in a preset without depending on an enum's order. The registry itself
// knows nothing about the synth: it only stores metadata and performs the
// value <-> normalized conversions every UI and every automation source needs.
//
// Thread contract: add() and every read used during setup run on the control
// thread. Once prepared and published, find()/toReal()/toNormalized() are
// read-only and RT-SAFE (no allocation, no locks, no mutation).
#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace drumas::core {

/// Stable parameter identifier. 0 is reserved for "no parameter".
using ParameterId = std::uint32_t;
inline constexpr ParameterId kInvalidParameterId = 0;

/// FNV-1a 32-bit hash, evaluated at compile time. Ids derived from names are
/// stable across builds and platforms as long as the name does not change; the
/// registry rejects an accidental collision exactly like a duplicate id.
[[nodiscard]] constexpr ParameterId makeParameterId(std::string_view name) noexcept {
    std::uint32_t hash = 2166136261U;
    for (const char character : name) {
        hash ^= static_cast<std::uint8_t>(character);
        hash *= 16777619U;
    }
    return hash;
}

/// Unit a parameter is expressed in. Presentation only: the DSP always works
/// in the parameter's real value, already in this unit.
enum class ParameterUnit : std::uint8_t {
    None = 0,
    Hertz,
    Milliseconds,
    Percent,  // 0..1 in the DSP; the UI may show it as a percentage
    Decibels,
    Semitones,
    Cents,
    Octaves,
    Ratio,
};

/// How the normalized 0..1 range maps onto the real range.
enum class ParameterCurve : std::uint8_t {
    Linear = 0,
    /// Logarithmic: equal fractions of the normalized range are equal ratios.
    /// Requires a strictly positive minimum so the ratio max/min is defined;
    /// used for frequency and time parameters, which the ear judges in ratios.
    Logarithmic,
};

/// Complete definition of one parameter. Plain value type, safe to copy into a
/// patch bank; `name` must point to storage that outlives the registry.
struct ParameterDescriptor {
    ParameterId id = kInvalidParameterId;
    std::string_view name{};
    float minValue = 0.0F;
    float maxValue = 1.0F;
    float defaultValue = 0.0F;
    ParameterUnit unit = ParameterUnit::None;
    ParameterCurve curve = ParameterCurve::Linear;
};

/// Fixed-capacity table of parameter definitions.
///
/// Why a fixed capacity: the registry is read by the audio thread when a
/// SetParam command is applied, and an audio-thread container that could grow
/// would allocate at the worst possible moment. 128 parameters cover the synth
/// and the six effect slots of the next stage; growing it is a one-line change.
class ParameterRegistry {
public:
    static constexpr std::size_t kMaxParameters = 128;

    ParameterRegistry() = default;

    ParameterRegistry(const ParameterRegistry&) = delete;
    ParameterRegistry& operator=(const ParameterRegistry&) = delete;
    ParameterRegistry(ParameterRegistry&&) = delete;
    ParameterRegistry& operator=(ParameterRegistry&&) = delete;

    /// Control thread. Registers a definition. Returns false and keeps the
    /// registry unchanged when the descriptor is invalid, its id or its name is
    /// already registered, or the table is full. Invalid means: an id of 0, an
    /// empty name, a non-finite bound, `min >= max`, a default outside
    /// [min, max], or a logarithmic curve with `min <= 0`.
    [[nodiscard]] bool add(const ParameterDescriptor& descriptor) noexcept {
        if (!isValid(descriptor) || contains(descriptor.id) || findByName(descriptor.name) != nullptr ||
            count_ >= kMaxParameters) {
            return false;
        }
        descriptors_[count_++] = descriptor;
        return true;
    }

    /// Control thread (or any thread once the registry is no longer modified).
    /// Returns nullptr for an unknown id. RT-SAFE.
    [[nodiscard]] const ParameterDescriptor* find(ParameterId id) const noexcept {
        for (std::size_t index = 0; index < count_; ++index) {
            if (descriptors_[index].id == id) {
                return &descriptors_[index];
            }
        }
        return nullptr;
    }

    [[nodiscard]] bool contains(ParameterId id) const noexcept { return find(id) != nullptr; }

    [[nodiscard]] std::size_t size() const noexcept { return count_; }
    [[nodiscard]] bool empty() const noexcept { return count_ == 0; }

    /// Control thread. Empties the registry so it can be rebuilt (prepare() may
    /// run more than once in tests and on device changes).
    void clear() noexcept { count_ = 0; }

    /// Control thread, or any thread once the registry is no longer modified.
    /// `index` must be below size().
    [[nodiscard]] const ParameterDescriptor& at(std::size_t index) const noexcept {
        assert(index < count_);
        return descriptors_[index];
    }

    /// Control thread. True when `descriptor` could be registered: see add().
    [[nodiscard]] static bool isValid(const ParameterDescriptor& descriptor) noexcept {
        if (descriptor.id == kInvalidParameterId || descriptor.name.empty()) {
            return false;
        }
        if (!std::isfinite(descriptor.minValue) || !std::isfinite(descriptor.maxValue) ||
            !std::isfinite(descriptor.defaultValue)) {
            return false;
        }
        if (!(descriptor.minValue < descriptor.maxValue)) {
            return false;
        }
        if (descriptor.defaultValue < descriptor.minValue ||
            descriptor.defaultValue > descriptor.maxValue) {
            return false;
        }
        if (descriptor.curve == ParameterCurve::Logarithmic && !(descriptor.minValue > 0.0F)) {
            return false;
        }
        return true;
    }

    /// Clamps `value` into the parameter's real range. Unknown ids and NaN are
    /// rejected (false); +/-infinity clamps to the corresponding bound.
    [[nodiscard]] bool clampToRange(ParameterId id, float& value) const noexcept {
        const ParameterDescriptor* descriptor = find(id);
        if (descriptor == nullptr || std::isnan(value)) {
            return false;
        }
        value = clampHelper(value, descriptor->minValue, descriptor->maxValue);
        return true;
    }

    /// Real -> normalized 0..1. Values outside the range are clamped first, so
    /// the result is always usable by a slider. Unknown ids return false.
    /// RT-SAFE.
    [[nodiscard]] bool toNormalized(ParameterId id, float real, float& normalized) const noexcept {
        const ParameterDescriptor* descriptor = find(id);
        if (descriptor == nullptr || std::isnan(real)) {
            return false;
        }
        const float clamped =
            clampHelper(real, descriptor->minValue, descriptor->maxValue);
        if (descriptor->curve == ParameterCurve::Logarithmic) {
            normalized = std::log(clamped / descriptor->minValue) /
                         std::log(descriptor->maxValue / descriptor->minValue);
        } else {
            normalized = (clamped - descriptor->minValue) /
                         (descriptor->maxValue - descriptor->minValue);
        }
        normalized = clampHelper(normalized, 0.0F, 1.0F);
        return true;
    }

    /// Normalized 0..1 -> real. The normalized value is clamped first, so the
    /// result always lies inside [min, max]. Unknown ids return false. RT-SAFE.
    [[nodiscard]] bool toReal(ParameterId id, float normalized, float& real) const noexcept {
        const ParameterDescriptor* descriptor = find(id);
        if (descriptor == nullptr || std::isnan(normalized)) {
            return false;
        }
        const float clampedNormalized = clampHelper(normalized, 0.0F, 1.0F);
        if (descriptor->curve == ParameterCurve::Logarithmic) {
            real = descriptor->minValue *
                   std::pow(descriptor->maxValue / descriptor->minValue, clampedNormalized);
        } else {
            real = descriptor->minValue +
                   clampedNormalized * (descriptor->maxValue - descriptor->minValue);
        }
        return true;
    }

    /// Default real value, or 0 for an unknown id. RT-SAFE.
    [[nodiscard]] float defaultValue(ParameterId id) const noexcept {
        const ParameterDescriptor* descriptor = find(id);
        return descriptor != nullptr ? descriptor->defaultValue : 0.0F;
    }

private:
    /// Local replacement for std::clamp: the two-argument form is not affected,
    /// but keeping it here makes the noexcept contract explicit.
    [[nodiscard]] static constexpr float clampHelper(float value, float low,
                                                    float high) noexcept {
        return value < low ? low : (value > high ? high : value);
    }

    [[nodiscard]] const ParameterDescriptor* findByName(std::string_view name) const noexcept {
        for (std::size_t index = 0; index < count_; ++index) {
            if (descriptors_[index].name == name) {
                return &descriptors_[index];
            }
        }
        return nullptr;
    }

    std::array<ParameterDescriptor, kMaxParameters> descriptors_{};
    std::size_t count_ = 0;
};

}  // namespace drumas::core
