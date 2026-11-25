#include <catch2/catch_test_macros.hpp>

#include "core/AudioTypes.h"
#include "engine/AudioGraph.h"
#include "fx/EffectFactory.h"
#include "synth/SynthProcessor.h"

TEST_CASE("shutdown can discard an unapplied effect swap", "[engine][shutdown]") {
    drumas::synth::SynthProcessor processor;
    const drumas::core::AudioSpec spec{48000.0, 256, 2};
    processor.prepare(spec);

    drumas::engine::AudioGraph graph;
    graph.prepare(spec);
    graph.setProcessor(&processor);

    auto effect = drumas::fx::createPreparedEffect(drumas::fx::kFilterType, spec);
    REQUIRE(effect != nullptr);
    REQUIRE(graph.pushCommand(drumas::core::swapEffect(
        0, reinterpret_cast<std::uintptr_t>(effect.get()))));
    effect.release();

    drumas::core::Command pending;
    REQUIRE(graph.popPendingCommand(pending));
    REQUIRE(pending.type == drumas::core::CommandType::SwapEffect);
    processor.discardCommand(pending);
    REQUIRE_FALSE(graph.popPendingCommand(pending));

    graph.reset();
}
