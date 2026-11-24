// Six serial insert slots with click-free bypass, mix, hot type changes and
// latency-compensated dry paths.
//
// Ownership: the chain owns the current effect and, during a type crossfade, the
// outgoing one. The audio thread never deletes either; it returns the outgoing
// effect through an SPSC queue and the control thread destroys it. See
// docs/adr/0008-effect-chain-and-hot-swap.md.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <dspark/Core/RingBuffer.h>

#include "core/AudioTypes.h"
#include "core/ParameterRegistry.h"
#include "core/SmoothedValue.h"
#include "core/SpscQueue.h"
#include "fx/ChainDescription.h"
#include "fx/EffectFactory.h"
#include "fx/IEffect.h"

namespace drumas::fx {

class EffectChain {
public:
    /// Largest latency any insert may report. Drive's 2x oversampler uses 32
    /// samples at the medium quality; the margin leaves room for a future
    /// higher-quality mode. Effects reporting more are clamped to this value.
    static constexpr int kMaxLatencySamples = 256;
    static constexpr std::size_t kRetireCapacity = 16;

    EffectChain() = default;
    ~EffectChain() { releaseAll(); }

    EffectChain(const EffectChain&) = delete;
    EffectChain& operator=(const EffectChain&) = delete;
    EffectChain(EffectChain&&) = delete;
    EffectChain& operator=(EffectChain&&) = delete;

    /// Control thread, stream stopped.
    void prepare(const core::AudioSpec& spec) { prepare(spec, defaultChainDescription()); }

    /// Control thread, stream stopped. Builds every configured effect here, so
    /// the first render never allocates.
    void prepare(const core::AudioSpec& spec, const ChainDescription& description) {
        spec_ = spec;
        spec_.sampleRate = spec.sampleRate > 0.0 ? spec.sampleRate : core::kDefaultSampleRate;
        spec_.maxBlockSize = std::max(spec.maxBlockSize, 1);
        spec_.channels = 2;
        releaseAll();

        wetCurrentLeft_.assign(static_cast<std::size_t>(spec_.maxBlockSize), 0.0F);
        wetCurrentRight_.assign(static_cast<std::size_t>(spec_.maxBlockSize), 0.0F);
        wetOutgoingLeft_.assign(static_cast<std::size_t>(spec_.maxBlockSize), 0.0F);
        wetOutgoingRight_.assign(static_cast<std::size_t>(spec_.maxBlockSize), 0.0F);

        for (std::size_t index = 0; index < kEffectSlotCount; ++index) {
            Slot& slot = slots_[index];
            slot.effect = nullptr;
            slot.outgoing = nullptr;
            slot.bypass = description.slots[index].bypass;
            slot.effectActive = false;
            slot.tailActive = false;
            slot.effectLatency = 0;
            slot.outgoingLatency = 0;
            slot.latency = 0;
            slot.tailFrames = 0;
            slot.silentFrames = 0;
            slot.fade = 1.0F;
            slot.fadeStep = 1.0F / std::max(1.0F,
                                            static_cast<float>(spec_.sampleRate * 0.020));
            slot.dryLeft.prepare(kMaxLatencySamples + spec_.maxBlockSize + 8);
            slot.dryRight.prepare(kMaxLatencySamples + spec_.maxBlockSize + 8);
            slot.currentWetLeft.prepare(kMaxLatencySamples + spec_.maxBlockSize + 8);
            slot.currentWetRight.prepare(kMaxLatencySamples + spec_.maxBlockSize + 8);
            slot.outgoingWetLeft.prepare(kMaxLatencySamples + spec_.maxBlockSize + 8);
            slot.outgoingWetRight.prepare(kMaxLatencySamples + spec_.maxBlockSize + 8);

            slot.mix.prepare(spec_.sampleRate, 10.0);
            slot.active.prepare(spec_.sampleRate, 20.0);
            slot.mix.reset(description.slots[index].mix);
            slot.active.reset(slot.bypass ? 0.0F : 1.0F);
            slot.effectActive = !slot.bypass;

            slot.effect = createEffect(description.slots[index].effectType).release();
            if (slot.effect != nullptr) {
                slot.effect->prepare(spec_);
                slot.effectLatency = std::clamp(slot.effect->latencySamples(), 0,
                                                kMaxLatencySamples);
                applyDescription(index, description.slots[index]);
            }
            if (slot.effect == nullptr) {
                // An empty slot is a pass-through, not a muted wet path.
                slot.bypass = true;
                slot.effectActive = false;
                slot.active.reset(0.0F);
            } else if (slot.bypass) {
                slot.effectActive = false;
            }
        }

        // Reserve the largest insert latency in the whole rack for every slot.
        // Without this, replacing a zero-latency filter with the oversampled
        // drive would change the dry-path delay mid-crossfade and could click.
        // The default rack contains drive, so the whole chain pays 32 samples
        // per slot (about 4 ms); a rack without any latency-bearing insert pays
        // zero.
        fixedSlotLatency_ = 0;
        for (const Slot& slot : slots_) {
            fixedSlotLatency_ = std::max(fixedSlotLatency_, slot.effectLatency);
        }
        for (Slot& slot : slots_) {
            slot.latency = std::max(fixedSlotLatency_, slot.effectLatency);
        }

        description_ = description;
        for (std::size_t index = 0; index < kEffectSlotCount; ++index) {
            if (slots_[index].effect != nullptr) {
                applyDescription(index, description.slots[index]);
            } else {
                description_.slots[index] = description.slots[index];
                description_.slots[index].effectType = kNoEffectType;
                description_.slots[index].bypass = slots_[index].bypass;
                description_.slots[index].parameterCount = 0;
            }
        }
    }

    /// Audio thread. Calling reset() on an effect is RT-SAFE.
    void reset() noexcept {
        for (Slot& slot : slots_) {
            if (slot.effect != nullptr) slot.effect->reset();
            if (slot.outgoing != nullptr) {
                retire(slot.outgoing);
                slot.outgoing = nullptr;
            }
            slot.dryLeft.reset();
            slot.dryRight.reset();
            slot.currentWetLeft.reset();
            slot.currentWetRight.reset();
            slot.outgoingWetLeft.reset();
            slot.outgoingWetRight.reset();
            slot.fade = 1.0F;
            slot.effectActive = !slot.bypass;
            slot.tailActive = false;
            slot.tailFrames = 0;
            slot.silentFrames = 0;
        }
    }

    /// Audio thread. Processes the whole chain in place. `numFrames` must not
    /// exceed the prepared block size; larger blocks are truncated, matching the
    /// processor's own contract.
    void process(float* left, float* right, int numFrames) noexcept {
        if (left == nullptr || right == nullptr || numFrames <= 0) return;
        flushPendingRetires();
        const int frames = std::min(numFrames, spec_.maxBlockSize);
        for (Slot& slot : slots_) {
            processSlot(slot, left, right, frames);
        }
    }

    /// Audio thread. Adopts a prepared effect. The old one is returned through
    /// takeRetired(); the new one is never destroyed here.
    void swapEffect(std::size_t slotIndex, IEffect* effect) noexcept {
        if (slotIndex >= kEffectSlotCount) {
            retire(effect);
            return;
        }
        if (effect == nullptr) return;
        Slot& slot = slots_[slotIndex];
        if (effect == slot.effect) return;

        const bool wasEmpty = slot.effect == nullptr;
        if (slot.outgoing != nullptr) retire(slot.outgoing);
        slot.outgoing = slot.effect;
        slot.outgoingLatency = slot.effectLatency;
        slot.effect = effect;
        if (wasEmpty) {
            // A prepared effect handed to an empty pass-through slot becomes
            // audible; an existing slot keeps its bypass state.
            slot.bypass = false;
            slot.active.reset(1.0F);
        }
        slot.effectLatency =
            std::clamp(effect->latencySamples(), 0, kMaxLatencySamples);
        slot.latency = std::max(slot.latency, std::max(slot.effectLatency,
                                                        slot.outgoingLatency));
        slot.fade = slot.outgoing != nullptr ? 0.0F : 1.0F;
        slot.effectActive = !slot.bypass;
        slot.tailActive = false;
        slot.tailFrames = 0;
        slot.silentFrames = 0;
        synchroniseSlotDescription(slotIndex);
    }

    /// Audio thread. Starts the bypass crossfade; an effect with a tail keeps
    /// running until it is inaudible or its tail budget is exhausted.
    void setBypass(std::size_t slotIndex, bool bypassed) noexcept {
        if (slotIndex >= kEffectSlotCount) return;
        Slot& slot = slots_[slotIndex];
        slot.bypass = bypassed;
        slot.active.setTargetValue(bypassed ? 0.0F : 1.0F);
        if (bypassed) {
            slot.tailActive = true;
            slot.tailFrames = 0;
            slot.silentFrames = 0;
            if (slot.effect != nullptr) slot.effectActive = true;
        } else {
            slot.tailActive = false;
            if (slot.effect != nullptr) slot.effectActive = true;
        }
        description_.slots[slotIndex].bypass = bypassed;
    }

    /// Audio thread.
    void setMix(std::size_t slotIndex, float wet) noexcept {
        if (slotIndex >= kEffectSlotCount || !std::isfinite(wet)) return;
        const float clamped = std::clamp(wet, 0.0F, 1.0F);
        slots_[slotIndex].mix.setTargetValue(clamped);
        description_.slots[slotIndex].mix = clamped;
    }

    /// Audio thread.
    void setParameter(std::size_t slotIndex, core::ParameterId id, float value) noexcept {
        if (slotIndex >= kEffectSlotCount) return;
        Slot& slot = slots_[slotIndex];
        if (slot.effect == nullptr) return;
        float clamped = value;
        if (!slot.effect->parameters().clampToRange(id, clamped)) return;
        slot.effect->setParameter(id, clamped);
        (void)setParameterValue(description_.slots[slotIndex], id, clamped);
    }

    /// Audio thread. The outgoing effect is handed to the control thread here;
    /// a full queue falls back to a small fixed holding area, still without
    /// allocation or deletion.
    [[nodiscard]] bool takeRetired(IEffect*& effect) noexcept {
        return retired_.pop(effect);
    }

    /// Audio thread, diagnostics.
    [[nodiscard]] int latencySamples() const noexcept {
        int total = 0;
        for (const Slot& slot : slots_) total += slot.latency;
        return total;
    }

    /// Audio thread after prepare. Not safe to read from the control thread
    /// while rendering: use the control-side mirror for UI display.
    [[nodiscard]] const ChainDescription& description() const noexcept {
        return description_;
    }

    [[nodiscard]] std::size_t slotCount() const noexcept { return kEffectSlotCount; }
    [[nodiscard]] double sampleRate() const noexcept { return spec_.sampleRate; }
    [[nodiscard]] int maxBlockSize() const noexcept { return spec_.maxBlockSize; }

private:
    struct Slot {
        IEffect* effect = nullptr;
        IEffect* outgoing = nullptr;
        bool bypass = true;
        bool effectActive = false;
        bool tailActive = false;
        int latency = 0;
        int effectLatency = 0;
        int outgoingLatency = 0;
        int tailFrames = 0;
        int silentFrames = 0;
        float fade = 1.0F;
        float fadeStep = 0.001F;
        core::SmoothedValue mix;
        core::SmoothedValue active;

        // Dry path, aligned with the slot's current latency.
        dspark::RingBuffer<float> dryLeft;
        dspark::RingBuffer<float> dryRight;
        // Wet paths, padded to the same slot latency during a type crossfade.
        dspark::RingBuffer<float> currentWetLeft;
        dspark::RingBuffer<float> currentWetRight;
        dspark::RingBuffer<float> outgoingWetLeft;
        dspark::RingBuffer<float> outgoingWetRight;
    };

    void processSlot(Slot& slot, float* left, float* right, int numFrames) noexcept {
        const bool hasCurrent = slot.effect != nullptr;
        const bool hasOutgoing = slot.outgoing != nullptr;
        if (!hasCurrent && !hasOutgoing) {
            // An empty slot is always a pass-through, even if the rack reserved
            // latency for other slots.
            if (slot.latency == 0) return;
            for (int index = 0; index < numFrames; ++index) {
                slot.dryLeft.push(left[index]);
                slot.dryRight.push(right[index]);
                left[index] = slot.dryLeft.read(slot.latency);
                right[index] = slot.dryRight.read(slot.latency);
            }
            return;
        }

        const bool processCurrent = hasCurrent && slot.effectActive;
        const bool processOutgoing = hasOutgoing;
        if (processCurrent) {
            for (int index = 0; index < numFrames; ++index) {
                wetCurrentLeft_[static_cast<std::size_t>(index)] = left[index];
                wetCurrentRight_[static_cast<std::size_t>(index)] = right[index];
            }
            slot.effect->process(wetCurrentLeft_.data(), wetCurrentRight_.data(), numFrames);
        }
        if (processOutgoing) {
            for (int index = 0; index < numFrames; ++index) {
                wetOutgoingLeft_[static_cast<std::size_t>(index)] = left[index];
                wetOutgoingRight_[static_cast<std::size_t>(index)] = right[index];
            }
            slot.outgoing->process(wetOutgoingLeft_.data(), wetOutgoingRight_.data(),
                                   numFrames);
        }

        const int currentPad = std::max(0, slot.latency - slot.effectLatency);
        const int outgoingPad = std::max(0, slot.latency - slot.outgoingLatency);
        const float fadeStep = slot.fadeStep;
        float fade = slot.fade;
        float peak = 0.0F;

        for (int index = 0; index < numFrames; ++index) {
            const float inputLeft = left[index];
            const float inputRight = right[index];
            slot.dryLeft.push(inputLeft);
            slot.dryRight.push(inputRight);
            const float dryLeft = slot.dryLeft.read(slot.latency);
            const float dryRight = slot.dryRight.read(slot.latency);

            float currentLeft = 0.0F;
            float currentRight = 0.0F;
            if (processCurrent) {
                slot.currentWetLeft.push(wetCurrentLeft_[static_cast<std::size_t>(index)]);
                slot.currentWetRight.push(wetCurrentRight_[static_cast<std::size_t>(index)]);
                currentLeft = slot.currentWetLeft.read(currentPad);
                currentRight = slot.currentWetRight.read(currentPad);
            }

            float wetLeft = currentLeft;
            float wetRight = currentRight;
            if (processOutgoing) {
                slot.outgoingWetLeft.push(
                    wetOutgoingLeft_[static_cast<std::size_t>(index)]);
                slot.outgoingWetRight.push(
                    wetOutgoingRight_[static_cast<std::size_t>(index)]);
                const float outgoingLeft = slot.outgoingWetLeft.read(outgoingPad);
                const float outgoingRight = slot.outgoingWetRight.read(outgoingPad);
                if (hasCurrent && processCurrent) {
                    const float blend = std::min(1.0F, std::max(0.0F, fade));
                    wetLeft = outgoingLeft * (1.0F - blend) + currentLeft * blend;
                    wetRight = outgoingRight * (1.0F - blend) + currentRight * blend;
                } else {
                    wetLeft = outgoingLeft;
                    wetRight = outgoingRight;
                }
            }

            const float mix = slot.mix.getNextValue();
            const float active = slot.active.getNextValue();
            const float wetGain = std::clamp(mix * active, 0.0F, 1.0F);
            const float outLeft = dryLeft * (1.0F - wetGain) + wetLeft * wetGain;
            const float outRight = dryRight * (1.0F - wetGain) + wetRight * wetGain;
            left[index] = std::isfinite(outLeft) ? outLeft : 0.0F;
            right[index] = std::isfinite(outRight) ? outRight : 0.0F;
            peak = std::max(peak, std::max(std::fabs(currentLeft), std::fabs(currentRight)));
            fade += fadeStep;
        }

        if (slot.outgoing != nullptr && fade >= 1.0F) {
            retire(slot.outgoing);
            slot.outgoing = nullptr;
        }
        slot.fade = std::min(fade, 1.0F);

        if (slot.bypass && slot.effectActive && slot.effect != nullptr) {
            slot.tailFrames += numFrames;
            if (peak < 1.0e-4F) {
                slot.silentFrames += numFrames;
            } else {
                slot.silentFrames = 0;
            }
            const int tailBudget = std::max(slot.effect->tailSamples(),
                                            static_cast<int>(spec_.sampleRate * 0.1));
            const bool inaudible = slot.active.current() <= 1.0e-4F &&
                                   slot.silentFrames >=
                                       static_cast<int>(spec_.sampleRate * 0.1);
            if (inaudible || slot.tailFrames >= tailBudget) {
                slot.effect->reset();
                slot.effectActive = false;
                slot.tailActive = false;
            }
        }
    }

    void retire(IEffect* effect) noexcept {
        if (effect == nullptr) return;
        if (retired_.push(effect)) return;
        if (pendingRetireCount_ < pendingRetire_.size()) {
            pendingRetire_[pendingRetireCount_++] = effect;
        }
        // A full retirement queue plus a full holding area can only happen if
        // the control thread stops draining; leaking is preferable to deleting
        // on the audio thread.
    }

    void flushPendingRetires() noexcept {
        std::size_t index = 0;
        while (index < pendingRetireCount_) {
            if (!retired_.push(pendingRetire_[index])) break;
            ++index;
        }
        if (index > 0) {
            for (std::size_t move = index; move < pendingRetireCount_; ++move) {
                pendingRetire_[move - index] = pendingRetire_[move];
            }
            pendingRetireCount_ -= index;
        }
    }

    void applyDescription(std::size_t slotIndex, const EffectSlotDescription& source) {
        Slot& slot = slots_[slotIndex];
        if (slot.effect == nullptr) return;
        EffectSlotDescription& target = description_.slots[slotIndex];
        target = source;
        target.effectType = slot.effect->typeId();
        target.parameterCount = 0;

        const core::ParameterRegistry& registry = slot.effect->parameters();
        for (std::size_t index = 0; index < registry.size(); ++index) {
            const core::ParameterDescriptor& descriptor = registry.at(index);
            (void)setParameterValue(target, descriptor.id, descriptor.defaultValue);
        }
        for (std::size_t index = 0; index < source.parameterCount; ++index) {
            const EffectParameterValue& value = source.parameters[index];
            float clamped = value.value;
            if (!registry.clampToRange(value.id, clamped)) continue;
            slot.effect->setParameter(value.id, clamped);
            (void)setParameterValue(target, value.id, clamped);
        }

        // Push the values that survived validation into the effect.
        for (std::size_t index = 0; index < target.parameterCount; ++index) {
            if (target.parameters[index].id != core::kInvalidParameterId) {
                slot.effect->setParameter(target.parameters[index].id,
                                          target.parameters[index].value);
            }
        }
    }

    void synchroniseSlotDescription(std::size_t slotIndex) noexcept {
        Slot& slot = slots_[slotIndex];
        EffectSlotDescription& target = description_.slots[slotIndex];
        target.bypass = slot.bypass;
        target.mix = slot.mix.target();
        target.effectType = slot.effect != nullptr ? slot.effect->typeId() : kNoEffectType;
        target.parameterCount = 0;
        if (slot.effect == nullptr) return;
        const core::ParameterRegistry& registry = slot.effect->parameters();
        for (std::size_t index = 0; index < registry.size(); ++index) {
            const core::ParameterDescriptor& descriptor = registry.at(index);
            (void)setParameterValue(target, descriptor.id, descriptor.defaultValue);
        }
    }

    void releaseAll() noexcept {
        fixedSlotLatency_ = 0;
        for (Slot& slot : slots_) {
            delete slot.effect;
            slot.effect = nullptr;
            delete slot.outgoing;
            slot.outgoing = nullptr;
        }
        IEffect* retired = nullptr;
        while (retired_.pop(retired)) delete retired;
        for (std::size_t index = 0; index < pendingRetireCount_; ++index) {
            delete pendingRetire_[index];
            pendingRetire_[index] = nullptr;
        }
        pendingRetireCount_ = 0;
    }

    std::array<Slot, kEffectSlotCount> slots_{};
    core::SpscQueue<IEffect*, kRetireCapacity> retired_;
    std::array<IEffect*, 8> pendingRetire_{};
    std::size_t pendingRetireCount_ = 0;

    std::vector<float> wetCurrentLeft_;
    std::vector<float> wetCurrentRight_;
    std::vector<float> wetOutgoingLeft_;
    std::vector<float> wetOutgoingRight_;

    int fixedSlotLatency_ = 0;
    ChainDescription description_{};
    core::AudioSpec spec_{};
};

}  // namespace drumas::fx
