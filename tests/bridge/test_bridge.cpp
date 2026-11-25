#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <span>

#include "bridge/ChainMirror.h"
#include "bridge/ChordExpander.h"
#include "bridge/ParameterMirror.h"
#include "core/AudioTypes.h"
#include "core/ParameterRegistry.h"
#include "fx/EffectTypes.h"
#include "support/AllocationCounter.h"

namespace {

class TestCommandSink final : public drumas::bridge::CommandSink {
public:
    std::array<drumas::core::Command, 8> commands{};
    std::size_t count = 0;
    std::size_t capacity = commands.size();

    [[nodiscard]] bool pushBatch(
        std::span<const drumas::core::Command> batch) noexcept override {
        if (batch.size() > capacity - count) {
            return false;
        }
        for (const auto& command : batch) {
            commands[count++] = command;
        }
        return true;
    }
};

}  // namespace

TEST_CASE("chord expansion enqueues every note as one command batch", "[bridge][chord]") {
    TestCommandSink sink;
    drumas::bridge::ChordExpander expander;

    REQUIRE(expander.noteOn(
        sink, {drumas::music::PitchClass::C, drumas::music::ChordQuality::Major, 0, 4}, 0.75F));
    REQUIRE(sink.count == 3);
    REQUIRE(sink.commands[0].note == 60);
    REQUIRE(sink.commands[1].note == 64);
    REQUIRE(sink.commands[2].note == 67);
    REQUIRE(sink.commands[0].value == 0.75F);
    REQUIRE(expander.droppedChords() == 0);
}

TEST_CASE("chord expansion does not allocate", "[bridge][realtime]") {
    TestCommandSink sink;
    drumas::bridge::ChordExpander expander;
    drumas::test::resetAllocationCount();
    const bool enqueued = expander.noteOn(
        sink, {drumas::music::PitchClass::C, drumas::music::ChordQuality::Major7, 0, 4});
    const std::size_t allocations = drumas::test::allocationCount();

    REQUIRE(enqueued);
    REQUIRE(allocations == 0);
}

TEST_CASE("a chord is wholly rejected and counted when queue capacity is insufficient",
          "[bridge][chord]") {
    TestCommandSink sink;
    sink.count = 6;
    drumas::bridge::ChordExpander expander;

    REQUIRE_FALSE(expander.noteOn(
        sink, {drumas::music::PitchClass::C, drumas::music::ChordQuality::Major7, 0, 4}));
    REQUIRE(sink.count == 6);
    REQUIRE(expander.droppedChords() == 1);
}

TEST_CASE("chord release is also enqueued as one note-off batch", "[bridge][chord]") {
    TestCommandSink sink;
    drumas::bridge::ChordExpander expander;

    REQUIRE(expander.noteOff(
        sink, {drumas::music::PitchClass::C, drumas::music::ChordQuality::Major, 0, 4}));
    REQUIRE(sink.count == 3);
    for (std::size_t index = 0; index < sink.count; ++index) {
        REQUIRE(sink.commands[index].type == drumas::core::CommandType::NoteOff);
    }
}

TEST_CASE("parameter mirror clamps values and rejects unknown or NaN inputs",
          "[bridge][parameters]") {
    drumas::core::ParameterRegistry registry;
    constexpr auto id = drumas::core::makeParameterId("test.gain");
    REQUIRE(registry.add({id, "test.gain", 0.0F, 1.0F, 0.25F,
                          drumas::core::ParameterUnit::Percent,
                          drumas::core::ParameterCurve::Linear}));

    drumas::bridge::ParameterMirror mirror;
    mirror.initialize(registry);
    float value = 0.0F;
    REQUIRE(mirror.get(id, value));
    REQUIRE(value == 0.25F);
    REQUIRE(mirror.set(id, 2.0F));
    REQUIRE(mirror.get(id, value));
    REQUIRE(value == 1.0F);
    REQUIRE_FALSE(mirror.set(id, NAN));
    REQUIRE_FALSE(mirror.set(id + 1, 0.5F));
    const auto snapshot = mirror.snapshot();
    REQUIRE(snapshot.count == 1);
    REQUIRE(snapshot.values[0].descriptor.id == id);
    REQUIRE(snapshot.values[0].value == 1.0F);
}

TEST_CASE("chain mirror preserves six-slot defaults and accepts bounded updates",
          "[bridge][chain]") {
    drumas::bridge::ChainMirror mirror;
    auto snapshot = mirror.snapshot();
    REQUIRE(snapshot.slots.size() == 6);
    REQUIRE(snapshot.slots[0].effectType == drumas::fx::kReverbType);
    REQUIRE(snapshot.slots[0].bypass);

    REQUIRE(mirror.setEffectType(0, drumas::fx::kFilterType));
    REQUIRE(mirror.setBypass(0, false));
    REQUIRE(mirror.setMix(0, 2.0F));
    REQUIRE(mirror.setParameter(0, 123, 700.0F));
    REQUIRE_FALSE(mirror.setMix(6, 0.5F));
    REQUIRE_FALSE(mirror.setMix(0, std::numeric_limits<float>::quiet_NaN()));

    snapshot = mirror.snapshot();
    REQUIRE(snapshot.slots[0].effectType == drumas::fx::kFilterType);
    REQUIRE_FALSE(snapshot.slots[0].bypass);
    REQUIRE(snapshot.slots[0].mix == 1.0F);
    REQUIRE(snapshot.slots[0].parameterCount == 1);
    REQUIRE(snapshot.slots[0].parameters[0].id == 123);
}
