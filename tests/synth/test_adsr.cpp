#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "synth/AdsrEnvelope.h"

namespace {

constexpr double kSampleRate = 48000.0;

drumas::synth::AdsrParameters testParameters() {
    drumas::synth::AdsrParameters parameters;
    parameters.attackMs = 20.0F;
    parameters.decayMs = 100.0F;
    parameters.sustain = 0.6F;
    parameters.releaseMs = 100.0F;
    return parameters;
}

struct EnvelopeRun {
    float peak = 0.0F;
    float largestStep = 0.0F;
    int samplesToAttackEnd = -1;
    int samplesToSustain = -1;
    int samplesToIdle = -1;
    bool monotonicDuringAttack = true;
    bool allFinite = true;
};

/// Runs the envelope for `frames` samples, optionally releasing after
/// `releaseAfter` samples (-1 means never).
EnvelopeRun run(drumas::synth::AdsrEnvelope& envelope, int frames, int releaseAfter) {
    EnvelopeRun result;
    float previous = envelope.level();

    for (int frame = 0; frame < frames; ++frame) {
        if (releaseAfter >= 0 && frame == releaseAfter) {
            envelope.noteOff();
        }

        const float value = envelope.nextValue();
        if (!std::isfinite(value)) {
            result.allFinite = false;
        }
        result.peak = std::max(result.peak, value);
        result.largestStep = std::max(result.largestStep, std::fabs(value - previous));
        if (frame > 0 && value < previous - 1e-5F &&
            envelope.stage() == drumas::synth::EnvelopeStage::Attack) {
            result.monotonicDuringAttack = false;
        }

        if (result.samplesToAttackEnd < 0 &&
            envelope.stage() != drumas::synth::EnvelopeStage::Attack) {
            result.samplesToAttackEnd = frame;
        }
        if (result.samplesToSustain < 0 &&
            envelope.stage() == drumas::synth::EnvelopeStage::Sustain) {
            result.samplesToSustain = frame;
        }
        if (result.samplesToIdle < 0 && envelope.stage() == drumas::synth::EnvelopeStage::Idle &&
            releaseAfter >= 0 && frame > releaseAfter) {
            result.samplesToIdle = frame - releaseAfter;
        }
        previous = value;
    }
    return result;
}

}  // namespace

TEST_CASE("a fresh ADSR envelope is idle and silent", "[synth][adsr]") {
    drumas::synth::AdsrEnvelope envelope;
    envelope.prepare(kSampleRate);
    envelope.setParameters(testParameters());

    REQUIRE(envelope.stage() == drumas::synth::EnvelopeStage::Idle);
    REQUIRE_FALSE(envelope.isActive());
    REQUIRE(envelope.level() == Catch::Approx(0.0F));
    REQUIRE(envelope.nextValue() == Catch::Approx(0.0F));
}

TEST_CASE("ADSR reaches sustain and holds it", "[synth][adsr]") {
    drumas::synth::AdsrEnvelope envelope;
    envelope.prepare(kSampleRate);
    envelope.setParameters(testParameters());

    envelope.noteOn();
    REQUIRE(envelope.stage() == drumas::synth::EnvelopeStage::Attack);
    REQUIRE(envelope.isActive());

    const EnvelopeRun result = run(envelope, 24000, -1);  // 500 ms, never released

    REQUIRE(result.allFinite);
    REQUIRE(result.monotonicDuringAttack);
    REQUIRE(result.peak <= 1.0F + 1e-4F);
    REQUIRE(result.samplesToAttackEnd >= 0);
    REQUIRE(result.samplesToAttackEnd <= 2000);  // 20 ms attack, generous margin
    REQUIRE(result.samplesToSustain >= 0);
    REQUIRE(result.samplesToSustain <= 6000);  // 20 ms attack + 100 ms decay, with margin

    REQUIRE(envelope.stage() == drumas::synth::EnvelopeStage::Sustain);
    REQUIRE(envelope.level() == Catch::Approx(0.6F).margin(1e-3F));
}

TEST_CASE("ADSR release returns to idle and silence", "[synth][adsr]") {
    drumas::synth::AdsrEnvelope envelope;
    envelope.prepare(kSampleRate);
    envelope.setParameters(testParameters());

    envelope.noteOn();
    constexpr int kReleaseAfter = 12000;  // 250 ms of sustain
    const EnvelopeRun result = run(envelope, 48000, kReleaseAfter);

    REQUIRE(result.allFinite);
    REQUIRE(result.samplesToIdle >= 0);
    // 100 ms release, plus slack for the exponential tail.
    REQUIRE(result.samplesToIdle <= 8000);
    REQUIRE(envelope.stage() == drumas::synth::EnvelopeStage::Idle);
    REQUIRE(envelope.level() == Catch::Approx(0.0F));
    REQUIRE_FALSE(envelope.isActive());
}

TEST_CASE("ADSR transitions never jump", "[synth][adsr]") {
    drumas::synth::AdsrEnvelope envelope;
    envelope.prepare(kSampleRate);
    envelope.setParameters(testParameters());

    envelope.noteOn();
    const EnvelopeRun result = run(envelope, 48000, 12000);

    // Stage boundaries (attack->decay->sustain->release) must not step the
    // output: anything above ~1/200 of full scale would be an audible click.
    REQUIRE(result.largestStep < 0.02F);
}

TEST_CASE("ADSR handles zero sustain and retrigger", "[synth][adsr]") {
    drumas::synth::AdsrEnvelope envelope;
    envelope.prepare(kSampleRate);

    drumas::synth::AdsrParameters parameters = testParameters();
    parameters.sustain = 0.0F;
    envelope.setParameters(parameters);

    envelope.noteOn();
    for (int frame = 0; frame < 24000; ++frame) {
        const float value = envelope.nextValue();
        REQUIRE(std::isfinite(value));
        REQUIRE(value >= 0.0F);
        REQUIRE(value <= 1.0F + 1e-4F);
    }

    // Legato retrigger: the level must not jump back to zero.
    const float levelBefore = envelope.level();
    envelope.noteOn();
    const float levelAfter = envelope.nextValue();
    REQUIRE(std::fabs(levelAfter - levelBefore) < 0.05F);
}
