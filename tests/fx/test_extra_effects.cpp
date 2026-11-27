#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string_view>

#include "core/AudioTypes.h"
#include "fx/EffectFactory.h"
#include "fx/EffectTypes.h"
#include "fx/ExtraEffects.h"
#include "support/AllocationCounter.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockFrames = 256;
constexpr float kTwoPi = 6.283185307179586F;

using Block = std::array<float, kBlockFrames>;

struct Buffers {
    Block left{};
    Block right{};
};

void fillSine(Buffers& buffers, double& phase, float frequency, float amplitude) {
    for (int frame = 0; frame < kBlockFrames; ++frame) {
        const float sample = amplitude * std::sin(static_cast<float>(phase));
        buffers.left[static_cast<std::size_t>(frame)] = sample;
        buffers.right[static_cast<std::size_t>(frame)] = sample;
        phase += static_cast<double>(kTwoPi) * frequency / kSampleRate;
        if (phase > static_cast<double>(kTwoPi)) {
            phase -= static_cast<double>(kTwoPi);
        }
    }
}

constexpr std::array<drumas::fx::EffectTypeId, 6> kNewTypes{
    drumas::fx::kFlangerType, drumas::fx::kPhaserType, drumas::fx::kTremoloType,
    drumas::fx::kVibratoType, drumas::fx::kEqType,     drumas::fx::kLoFiType};

}  // namespace

TEST_CASE("every effect type in the cycle is built by the factory and named consistently",
          "[fx][types]") {
    for (const drumas::fx::EffectTypeId type : drumas::fx::kEffectTypeCycle) {
        REQUIRE(drumas::fx::isKnownEffectType(type));
        const auto effect = drumas::fx::createEffect(type);
        REQUIRE(effect != nullptr);
        REQUIRE(effect->typeId() == type);
        REQUIRE(effect->name() == drumas::fx::effectTypeName(type));
        REQUIRE(drumas::fx::effectTypeName(type) != std::string_view("none"));
    }
    REQUIRE_FALSE(drumas::fx::isKnownEffectType(drumas::fx::kNoEffectType));
}

TEST_CASE("new effects stay finite and bounded with extreme parameters", "[fx][stability]") {
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    for (const drumas::fx::EffectTypeId type : kNewTypes) {
        auto effect = drumas::fx::createPreparedEffect(type, spec);
        REQUIRE(effect != nullptr);
        REQUIRE(effect->parameters().size() >= 3);

        const auto& registry = effect->parameters();
        for (std::size_t index = 0; index < registry.size(); ++index) {
            const drumas::core::ParameterDescriptor descriptor = registry.at(index);
            // Every parameter can be pushed to both ends without breaking.
            effect->setParameter(descriptor.id, descriptor.minValue);
            effect->setParameter(descriptor.id, descriptor.maxValue);
            effect->setParameter(descriptor.id + 1, 1.0F);  // unknown id: ignored
        }

        Buffers buffers;
        double phase = 0.0;
        for (int block = 0; block < 120; ++block) {
            fillSine(buffers, phase, 440.0F, 0.5F);
            effect->process(buffers.left.data(), buffers.right.data(), kBlockFrames);
            for (int frame = 0; frame < kBlockFrames; ++frame) {
                const float left = buffers.left[static_cast<std::size_t>(frame)];
                const float right = buffers.right[static_cast<std::size_t>(frame)];
                REQUIRE(std::isfinite(left));
                REQUIRE(std::isfinite(right));
                REQUIRE(std::fabs(left) < 8.0F);
                REQUIRE(std::fabs(right) < 8.0F);
            }
        }
        REQUIRE(std::isfinite(effect->tailLevel()));

        effect->reset();
        REQUIRE(effect->tailLevel() == 0.0F);
    }
}

TEST_CASE("new effects do not allocate while processing", "[fx][realtime]") {
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    for (const drumas::fx::EffectTypeId type : kNewTypes) {
        auto effect = drumas::fx::createPreparedEffect(type, spec);
        REQUIRE(effect != nullptr);
        Buffers buffers;
        double phase = 0.0;
        fillSine(buffers, phase, 220.0F, 0.25F);
        effect->process(buffers.left.data(), buffers.right.data(), kBlockFrames);

        drumas::test::resetAllocationCount();
        for (int block = 0; block < 16; ++block) {
            effect->process(buffers.left.data(), buffers.right.data(), kBlockFrames);
        }
        const std::size_t allocations = drumas::test::allocationCount();
        INFO("allocations for " << effect->name() << ": " << allocations);
        REQUIRE(allocations == 0);
    }
}

TEST_CASE("tremolo modulates amplitude down to silence at full depth", "[fx][tremolo]") {
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    auto effect = drumas::fx::createPreparedEffect(drumas::fx::kTremoloType, spec);
    REQUIRE(effect != nullptr);
    effect->setParameter(drumas::core::makeParameterId("tremolo.depth"), 1.0F);
    effect->setParameter(drumas::core::makeParameterId("tremolo.rate"), 8.0F);

    Buffers buffers;
    double phase = 0.0;
    float minimum = 1.0F;
    float maximum = 0.0F;
    for (int block = 0; block < 60; ++block) {
        fillSine(buffers, phase, 220.0F, 0.5F);
        effect->process(buffers.left.data(), buffers.right.data(), kBlockFrames);
        for (int frame = 0; frame < kBlockFrames; ++frame) {
            const float sample = std::fabs(buffers.left[static_cast<std::size_t>(frame)]);
            minimum = std::min(minimum, sample);
            maximum = std::max(maximum, sample);
        }
    }
    INFO("tremolo min " << minimum << ", max " << maximum);
    REQUIRE(maximum > 0.25F);
    REQUIRE(minimum < 0.02F);
}

TEST_CASE("flanger and vibrato produce a delayed wet signal", "[fx][modulation]") {
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    for (const drumas::fx::EffectTypeId type :
         {drumas::fx::kFlangerType, drumas::fx::kVibratoType}) {
        auto effect = drumas::fx::createPreparedEffect(type, spec);
        REQUIRE(effect != nullptr);
        Buffers buffers;
        buffers.left[0] = 1.0F;
        buffers.right[0] = 1.0F;
        float laterEnergy = 0.0F;
        for (int block = 0; block < 8; ++block) {
            effect->process(buffers.left.data(), buffers.right.data(), kBlockFrames);
            if (block > 0) {
                for (const float sample : buffers.left) {
                    laterEnergy += std::fabs(sample);
                }
            }
            buffers.left.fill(0.0F);
            buffers.right.fill(0.0F);
        }
        INFO(effect->name() << " later energy " << laterEnergy);
        REQUIRE(laterEnergy > 0.0F);
    }
}

TEST_CASE("eq shelf gain changes the level of the band it targets", "[fx][eq]") {
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    auto boost = drumas::fx::createPreparedEffect(drumas::fx::kEqType, spec);
    auto cut = drumas::fx::createPreparedEffect(drumas::fx::kEqType, spec);
    REQUIRE(boost != nullptr);
    REQUIRE(cut != nullptr);
    boost->setParameter(drumas::core::makeParameterId("eq.low"), 12.0F);
    cut->setParameter(drumas::core::makeParameterId("eq.low"), -12.0F);

    auto energy = [](drumas::fx::IEffect& effect, float frequency) {
        Buffers buffers;
        double phase = 0.0;
        float total = 0.0F;
        for (int block = 0; block < 40; ++block) {
            fillSine(buffers, phase, frequency, 0.25F);
            effect.process(buffers.left.data(), buffers.right.data(), kBlockFrames);
            if (block >= 20) {
                for (const float sample : buffers.left) {
                    total += sample * sample;
                }
            }
        }
        return total;
    };

    const float boosted = energy(*boost, 100.0F);
    const float attenuated = energy(*cut, 100.0F);
    INFO("boosted " << boosted << ", attenuated " << attenuated);
    REQUIRE(boosted > attenuated * 4.0F);
}

TEST_CASE("lofi quantises to the bit depth it reports", "[fx][lofi]") {
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    auto effect = drumas::fx::createPreparedEffect(drumas::fx::kLoFiType, spec);
    REQUIRE(effect != nullptr);
    effect->setParameter(drumas::core::makeParameterId("lofi.bits"), 4.0F);
    effect->setParameter(drumas::core::makeParameterId("lofi.rate"), 48000.0F);
    effect->setParameter(drumas::core::makeParameterId("lofi.noise"), 0.0F);
    effect->setParameter(drumas::core::makeParameterId("lofi.wow"), 0.0F);

    const float step = 2.0F / 16.0F;  // 4 bits -> 16 levels over -1..1
    Buffers buffers;
    double phase = 0.0;
    float peak = 0.0F;
    for (int block = 0; block < 40; ++block) {
        fillSine(buffers, phase, 220.0F, 0.5F);
        effect->process(buffers.left.data(), buffers.right.data(), kBlockFrames);
        if (block >= 20) {
            for (const float sample : buffers.left) {
                const float levels = sample / step;
                REQUIRE(std::fabs(levels - std::round(levels)) < 0.01F);
                peak = std::max(peak, std::fabs(sample));
            }
        }
    }
    REQUIRE(peak > 0.1F);
}
