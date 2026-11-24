#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <memory>
#include <vector>

#include "core/AudioTypes.h"
#include "core/ParameterRegistry.h"
#include "fx/ChainDescription.h"
#include "fx/EffectChain.h"
#include "support/AllocationCounter.h"

namespace {

using drumas::core::AudioSpec;
using drumas::fx::ChainDescription;
using drumas::fx::EffectChain;
using drumas::fx::EffectTypeId;
using drumas::fx::IEffect;
using drumas::fx::kEffectSlotCount;
using drumas::fx::kNoEffectType;

class GainEffect final : public IEffect {
public:
    GainEffect(float gain, int latency) : gain_(gain), latency_(latency) {
        (void)registry_.add({kGain, "test.gain", 0.0F, 10.0F, gain, 
                             drumas::core::ParameterUnit::None,
                             drumas::core::ParameterCurve::Linear});
    }

    [[nodiscard]] EffectTypeId typeId() const noexcept override { return kType; }
    [[nodiscard]] const char* name() const noexcept override { return "test-gain"; }
    void prepare(const AudioSpec&) override {}
    void reset() noexcept override { peak_ = 0.0F; }

    void process(float* left, float* right, int numFrames) noexcept override {
        peak_ = 0.0F;
        for (int index = 0; index < numFrames; ++index) {
            left[index] *= gain_;
            right[index] *= gain_;
            peak_ = std::max(peak_, std::max(std::fabs(left[index]),
                                             std::fabs(right[index])));
        }
    }

    [[nodiscard]] int latencySamples() const noexcept override { return latency_; }
    [[nodiscard]] float tailLevel() const noexcept override { return peak_; }
    [[nodiscard]] const drumas::core::ParameterRegistry& parameters() const noexcept override {
        return registry_;
    }

    void setParameter(drumas::core::ParameterId id, float value) noexcept override {
        if (!registry_.clampToRange(id, value)) return;
        if (id == kGain) gain_ = value;
    }

    static constexpr EffectTypeId kType = drumas::core::makeParameterId("test.gain.effect");

private:
    inline static const drumas::core::ParameterId kGain =
        drumas::core::makeParameterId("test.gain");
    drumas::core::ParameterRegistry registry_;
    float gain_ = 1.0F;
    int latency_ = 0;
    float peak_ = 0.0F;
};

[[nodiscard]] ChainDescription emptyChain() {
    ChainDescription description;
    for (auto& slot : description.slots) {
        slot.effectType = kNoEffectType;
        slot.bypass = false;
        slot.mix = 1.0F;
    }
    return description;
}

void render(EffectChain& chain, std::vector<float>& left, std::vector<float>& right) {
    chain.process(left.data(), right.data(), static_cast<int>(left.size()));
}

}  // namespace

TEST_CASE("the chain description carries types, bypass, mix and values",
          "[fx][chain][description]") {
    EffectChain chain;
    const AudioSpec spec{48000.0, 64, 2};
    chain.prepare(spec);

    const ChainDescription& description = chain.description();
    REQUIRE(description.slots[0].effectType == drumas::fx::kReverbType);
    REQUIRE(description.slots[3].effectType == drumas::fx::kDriveType);
    REQUIRE(description.slots[0].bypass);
    REQUIRE(description.slots[0].parameterCount >= 3);
    REQUIRE(drumas::fx::findParameter(description.slots[0],
                                      drumas::core::makeParameterId("reverb.decay")) !=
            nullptr);
}

TEST_CASE("effect chain processes slots in series", "[fx][chain]") {
    EffectChain chain;
    const AudioSpec spec{48000.0, 64, 2};
    chain.prepare(spec, emptyChain());

    chain.swapEffect(0, new GainEffect(2.0F, 0));
    chain.swapEffect(1, new GainEffect(3.0F, 0));

    std::vector<float> left(64, 0.1F);
    std::vector<float> right(64, -0.1F);
    render(chain, left, right);

    REQUIRE(left[32] == Catch::Approx(0.6F).margin(1.0e-6F));
    REQUIRE(right[32] == Catch::Approx(-0.6F).margin(1.0e-6F));
    REQUIRE(chain.latencySamples() == 0);
}

TEST_CASE("bypass is exact and keeps the slot latency", "[fx][chain][bypass]") {
    EffectChain chain;
    const AudioSpec spec{48000.0, 64, 2};
    chain.prepare(spec, emptyChain());
    chain.swapEffect(0, new GainEffect(1.0F, 5));
    REQUIRE(chain.latencySamples() == 5);

    std::vector<float> left(64, 0.25F);
    std::vector<float> right(64, 0.25F);
    render(chain, left, right);
    for (int index = 5; index < 64; ++index) {
        REQUIRE(left[static_cast<std::size_t>(index)] == Catch::Approx(0.25F));
    }

    chain.setBypass(0, true);
    for (int block = 0; block < 200; ++block) {
        std::fill(left.begin(), left.end(), 0.25F);
        std::fill(right.begin(), right.end(), 0.25F);
        render(chain, left, right);
    }
    // The dry path is still compensated by the same five samples.
    for (int index = 5; index < 64; ++index) {
        REQUIRE(left[static_cast<std::size_t>(index)] == Catch::Approx(0.25F));
    }
}

TEST_CASE("hot effect swaps crossfade instead of jumping", "[fx][chain][swap]") {
    EffectChain chain;
    const AudioSpec spec{48000.0, 64, 2};
    chain.prepare(spec, emptyChain());
    chain.swapEffect(0, new GainEffect(1.0F, 0));

    std::vector<float> left(64, 0.5F);
    std::vector<float> right(64, 0.5F);
    render(chain, left, right);
    REQUIRE(left[32] == Catch::Approx(0.5F));

    chain.swapEffect(0, new GainEffect(-1.0F, 0));
    float previous = left[63];
    float maxStep = 0.0F;
    for (int block = 0; block < 40; ++block) {
        std::fill(left.begin(), left.end(), 0.5F);
        std::fill(right.begin(), right.end(), 0.5F);
        render(chain, left, right);
        for (const float sample : left) {
            maxStep = std::max(maxStep, std::fabs(sample - previous));
            previous = sample;
        }
    }
    INFO("max sample-to-sample step during a +1 -> -1 crossfade: " << maxStep);
    REQUIRE(maxStep < 0.02F);
    REQUIRE(previous == Catch::Approx(-0.5F).margin(1.0e-5F));

    IEffect* retired = nullptr;
    bool collected = false;
    while (chain.takeRetired(retired)) {
        collected = true;
        delete retired;
    }
    REQUIRE(collected);
}

TEST_CASE("swapping an effect does not allocate on the audio path", "[fx][chain][realtime]") {
    EffectChain chain;
    const AudioSpec spec{48000.0, 128, 2};
    chain.prepare(spec, emptyChain());

    auto* replacement = new GainEffect(1.0F, 0);
    replacement->prepare(spec);

    std::vector<float> left(128, 0.1F);
    std::vector<float> right(128, 0.1F);
    drumas::test::resetAllocationCount();
    chain.swapEffect(0, replacement);
    render(chain, left, right);
    REQUIRE(drumas::test::allocationCount() == 0);

    IEffect* retired = nullptr;
    while (chain.takeRetired(retired)) delete retired;
}

TEST_CASE("all six slots can be active without changing the chain order",
          "[fx][chain][slots]") {
    EffectChain chain;
    const AudioSpec spec{48000.0, 32, 2};
    chain.prepare(spec, emptyChain());
    for (std::size_t index = 0; index < kEffectSlotCount; ++index) {
        chain.swapEffect(index, new GainEffect(1.01F, 0));
    }

    std::vector<float> left(32, 1.0F);
    std::vector<float> right(32, -1.0F);
    render(chain, left, right);
    const float expected = static_cast<float>(std::pow(1.01, 6.0));
    REQUIRE(left[0] == Catch::Approx(expected).epsilon(1.0e-5));
    REQUIRE(right[0] == Catch::Approx(-expected).epsilon(1.0e-5));
}
