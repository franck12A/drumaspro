#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "synth/PatchBank.h"
#include "synth/VoicePool.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockFrames = 256;

drumas::synth::Patch testPatch(int unisonVoices = 1) {
    drumas::synth::Patch patch;
    patch.name = "Test";
    patch.osc1Waveform = drumas::synth::Waveform::Saw;
    patch.osc1Level = 0.5F;
    patch.osc2Waveform = drumas::synth::Waveform::Square;
    patch.osc2Level = 0.3F;
    patch.unisonVoices = unisonVoices;
    patch.unisonDetuneCents = 6.0F;
    patch.unisonStereo = 0.5F;
    patch.cutoffHz = 4000.0F;
    patch.resonance = 0.1F;
    patch.filterEnvOctaves = 0.0F;
    patch.ampEnvelope = drumas::synth::AdsrParameters{1.0F, 10.0F, 0.8F, 10.0F};
    patch.filterEnvelope = drumas::synth::AdsrParameters{1.0F, 10.0F, 0.8F, 10.0F};
    patch.velocityVolume = 0.0F;
    patch.velocityBrightness = 0.0F;
    patch.lfoPitchDepthCents = 0.0F;
    patch.lfoCutoffDepth = 0.0F;
    return patch;
}

struct StereoBlock {
    std::array<float, kBlockFrames> left{};
    std::array<float, kBlockFrames> right{};
    std::array<float, kBlockFrames> lfo{};
};

void renderBlock(drumas::synth::VoicePool& pool, StereoBlock& block) {
    block.left.fill(0.0F);
    block.right.fill(0.0F);
    block.lfo.fill(0.0F);
    pool.renderAdd(block.left.data(), block.right.data(), kBlockFrames, block.lfo.data());
}

}  // namespace

TEST_CASE("voice pool starts silent", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);

    REQUIRE(pool.activeVoiceCount() == 0);
    REQUIRE(pool.stealCount() == 0);
}

TEST_CASE("voice pool allocates a voice per note", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    pool.noteOn(60, 1.0F, patch, live);
    REQUIRE(pool.activeVoiceCount() == 1);
    REQUIRE(pool.voice(0).note() == 60);
    REQUIRE(pool.voice(0).isHeld());
    REQUIRE(pool.stealCount() == 0);

    pool.noteOn(64, 0.5F, patch, live);
    REQUIRE(pool.activeVoiceCount() == 2);

    // Same note again retriggers the existing voice instead of taking another.
    pool.noteOn(60, 0.25F, patch, live);
    REQUIRE(pool.activeVoiceCount() == 2);
    REQUIRE(pool.stealCount() == 0);
}

TEST_CASE("voice pool steals when it runs out of voices", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    for (std::size_t index = 0; index < drumas::synth::VoicePool::kMaxVoices; ++index) {
        pool.noteOn(static_cast<std::uint8_t>(30 + index), 1.0F, patch, live);
    }
    REQUIRE(pool.activeVoiceCount() == drumas::synth::VoicePool::kMaxVoices);
    REQUIRE(pool.stealCount() == 0);

    pool.noteOn(90, 1.0F, patch, live);
    REQUIRE(pool.activeVoiceCount() == drumas::synth::VoicePool::kMaxVoices);
    REQUIRE(pool.stealCount() == 1);
}

TEST_CASE("each voice captures the patch it started with", "[synth][voices][patches]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);

    const drumas::synth::Patch& pad = drumas::synth::kFactoryPatches[0];
    const drumas::synth::Patch& pluck = drumas::synth::kFactoryPatches[1];
    pool.noteOn(60, 1.0F, pad, drumas::synth::liveParametersFrom(pad));
    pool.noteOn(64, 1.0F, pluck, drumas::synth::liveParametersFrom(pluck));

    REQUIRE(pool.voice(0).patch().name == std::string_view("Pad"));
    REQUIRE(pool.voice(1).patch().name == std::string_view("Pluck"));
}

TEST_CASE("note off releases the voices playing that note", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    pool.noteOn(60, 1.0F, patch, live);
    pool.noteOn(64, 1.0F, patch, live);

    pool.noteOff(60);
    REQUIRE_FALSE(pool.voice(0).isHeld());
    REQUIRE(pool.voice(1).isHeld());

    // The released voice keeps its tail until the envelope reaches idle.
    StereoBlock block;
    for (int iteration = 0; iteration < 40; ++iteration) {
        renderBlock(pool, block);
    }
    REQUIRE(pool.voice(0).isFree());
    REQUIRE(pool.activeVoiceCount() == 1);
}

TEST_CASE("oscillator levels are live and ramp on sounding voices", "[synth][voices][parameters]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    drumas::synth::Patch patch = testPatch();
    patch.osc1Level = 1.0F;
    patch.osc2Level = 0.0F;
    patch.subLevel = 0.0F;
    patch.noiseLevel = 0.0F;
    drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    pool.noteOn(60, 1.0F, patch, live);
    StereoBlock block;
    float fullPeak = 0.0F;
    for (int iteration = 0; iteration < 80; ++iteration) {
        renderBlock(pool, block);
        for (const float sample : block.left) {
            fullPeak = std::max(fullPeak, std::fabs(sample));
        }
    }
    REQUIRE(fullPeak > 0.01F);

    live.osc1Level = 0.0F;
    pool.setLiveParameters(live);
    float silentPeak = 0.0F;
    for (int iteration = 0; iteration < 80; ++iteration) {
        renderBlock(pool, block);
        if (iteration >= 40) {
            for (const float sample : block.left) {
                silentPeak = std::max(silentPeak, std::fabs(sample));
            }
        }
    }
    REQUIRE(silentPeak < fullPeak * 0.01F);
}

TEST_CASE("rendering voices produces finite, bounded stereo audio", "[synth][voices]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch(4);
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    pool.noteOn(57, 1.0F, patch, live);
    pool.noteOn(60, 0.8F, patch, live);
    pool.noteOn(64, 0.6F, patch, live);

    StereoBlock block;
    for (int iteration = 0; iteration < 200; ++iteration) {
        renderBlock(pool, block);
        for (int frame = 0; frame < kBlockFrames; ++frame) {
            REQUIRE(std::isfinite(block.left[static_cast<std::size_t>(frame)]));
            REQUIRE(std::isfinite(block.right[static_cast<std::size_t>(frame)]));
            // Three full voices before master gain: a generous bound that still
            // catches runaway resonance or unison summing.
            REQUIRE(std::fabs(block.left[static_cast<std::size_t>(frame)]) <= 3.0F);
            REQUIRE(std::fabs(block.right[static_cast<std::size_t>(frame)]) <= 3.0F);
        }
    }
}

// --- Chord groups (block 1) -------------------------------------------------

TEST_CASE("a chord starts every note before any of them renders", "[synth][voices][chord]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    const std::array<std::uint8_t, 3> notes{60, 64, 67};
    REQUIRE(pool.chordOn(1, notes.data(), 3, 1.0F, patch, live));

    // One command, three voices already held: no partial chord can be observed,
    // and all of them start on the first sample of the block.
    REQUIRE(pool.activeVoiceCount() == 3);
    for (std::uint8_t note : notes) {
        bool found = false;
        for (std::size_t index = 0; index < drumas::synth::VoicePool::kMaxVoices; ++index) {
            const drumas::synth::SynthVoice& voice = pool.voice(index);
            if (voice.isHeld() && voice.note() == note) {
                found = true;
                REQUIRE(voice.groupMask() == (1U << 1));
            }
        }
        REQUIRE(found);
    }

    StereoBlock block;
    block.left.fill(0.0F);
    block.right.fill(0.0F);
    block.lfo.fill(0.0F);
    pool.renderAdd(block.left.data(), block.right.data(), 1, block.lfo.data());
    REQUIRE(std::fabs(block.left[0]) > 0.0F);
}

TEST_CASE("a note shared by two chords is not retriggered and survives one release",
          "[synth][voices][chord]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    const std::array<std::uint8_t, 3> cMajor{60, 64, 67};
    REQUIRE(pool.chordOn(1, cMajor.data(), 3, 1.0F, patch, live));
    const std::size_t voicesAfterC = pool.activeVoiceCount();
    REQUIRE(voicesAfterC == 3);

    StereoBlock block;
    for (int iteration = 0; iteration < 20; ++iteration) {
        renderBlock(pool, block);
    }

    // A minor shares C (60) and E (64): only A (57) takes a new voice.
    const std::array<std::uint8_t, 3> aMinor{57, 60, 64};
    REQUIRE(pool.chordOn(2, aMinor.data(), 3, 1.0F, patch, live));
    REQUIRE(pool.activeVoiceCount() == voicesAfterC + 1);

    // Releasing C major drops its claim on C and E; they stay held by A minor.
    pool.chordOff(1);
    for (std::size_t index = 0; index < drumas::synth::VoicePool::kMaxVoices; ++index) {
        const drumas::synth::SynthVoice& voice = pool.voice(index);
        if (voice.note() == 60 || voice.note() == 64) {
            REQUIRE(voice.isHeld());
            REQUIRE(voice.groupMask() == (1U << 2));
        }
        if (voice.note() == 67) {
            REQUIRE_FALSE(voice.isHeld());
        }
    }

    // Releasing the second chord leaves nothing held.
    pool.chordOff(2);
    for (std::size_t index = 0; index < drumas::synth::VoicePool::kMaxVoices; ++index) {
        REQUIRE_FALSE(pool.voice(index).isHeld());
    }
}

TEST_CASE("chord off only releases the pad that was pressed", "[synth][voices][chord]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    const std::array<std::uint8_t, 3> dMinor{62, 65, 69};
    const std::array<std::uint8_t, 3> fMajor{65, 69, 72};
    REQUIRE(pool.chordOn(4, dMinor.data(), 3, 1.0F, patch, live));
    REQUIRE(pool.chordOn(6, fMajor.data(), 3, 1.0F, patch, live));

    pool.chordOff(4);
    bool dHeld = false;
    bool fHeld = false;
    for (std::size_t index = 0; index < drumas::synth::VoicePool::kMaxVoices; ++index) {
        const drumas::synth::SynthVoice& voice = pool.voice(index);
        if (voice.note() == 62) {
            dHeld = voice.isHeld();
        }
        if (voice.note() == 72) {
            fHeld = voice.isHeld();
        }
    }
    REQUIRE_FALSE(dHeld);
    REQUIRE(fHeld);
}

TEST_CASE("plain notes never steal or release a chord voice", "[synth][voices][chord]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    const std::array<std::uint8_t, 3> cMajor{60, 64, 67};
    REQUIRE(pool.chordOn(1, cMajor.data(), 3, 1.0F, patch, live));

    pool.noteOn(60, 0.5F, patch, live);
    REQUIRE(pool.activeVoiceCount() == 4);
    pool.noteOff(60);
    for (std::size_t index = 0; index < drumas::synth::VoicePool::kMaxVoices; ++index) {
        const drumas::synth::SynthVoice& voice = pool.voice(index);
        if (voice.note() == 60 && voice.groupMask() != 0) {
            REQUIRE(voice.isHeld());
        }
    }
}

TEST_CASE("stealing a busy pool stays finite and click-free", "[synth][voices][chord]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    // Fill the pool with single notes, then press chords: the steal path runs.
    for (std::size_t index = 0; index < drumas::synth::VoicePool::kMaxVoices; ++index) {
        pool.noteOn(static_cast<std::uint8_t>(30 + index), 1.0F, patch, live);
    }
    REQUIRE(pool.stealCount() == 0);

    const std::array<std::uint8_t, 4> chord{60, 64, 67, 71};
    REQUIRE(pool.chordOn(1, chord.data(), 4, 1.0F, patch, live));
    REQUIRE(pool.stealCount() == 4);

    StereoBlock block;
    float peak = 0.0F;
    float largestStep = 0.0F;
    float previous = 0.0F;
    for (int iteration = 0; iteration < 80; ++iteration) {
        renderBlock(pool, block);
        for (const float sample : block.left) {
            REQUIRE(std::isfinite(sample));
            peak = std::max(peak, std::fabs(sample));
            largestStep = std::max(largestStep, std::fabs(sample - previous));
            previous = sample;
        }
    }
    INFO("steal peak " << peak << ", largest step " << largestStep);
    REQUIRE(peak < 3.0F);
    // The new voices fade in over 2 ms, so no steal can produce a full-scale
    // jump even though 32 voices are sounding.
    REQUIRE(largestStep < 0.5F);
}

TEST_CASE("chords are rejected instead of truncated", "[synth][voices][chord]") {
    drumas::synth::VoicePool pool;
    pool.prepare(kSampleRate);
    const drumas::synth::Patch patch = testPatch();
    const drumas::synth::LiveParameters live = drumas::synth::liveParametersFrom(patch);

    const std::array<std::uint8_t, 5> tooMany{60, 62, 64, 65, 67};
    REQUIRE_FALSE(pool.chordOn(1, tooMany.data(), 5, 1.0F, patch, live));
    REQUIRE_FALSE(pool.chordOn(0, tooMany.data(), 3, 1.0F, patch, live));
    REQUIRE(pool.activeVoiceCount() == 0);
}
