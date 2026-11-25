#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "core/AudioTypes.h"
#include "engine/AudioGraph.h"
#include "music/Chord.h"
#include "synth/SynthProcessor.h"

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockFrames = 256;
constexpr std::size_t kFftSize = 16384;

void fft(std::array<std::complex<double>, kFftSize>& values) {
    for (std::size_t index = 1, reversed = 0; index < kFftSize; ++index) {
        std::size_t bit = kFftSize >> 1;
        for (; (reversed & bit) != 0; bit >>= 1) {
            reversed ^= bit;
        }
        reversed ^= bit;
        if (index < reversed) {
            std::swap(values[index], values[reversed]);
        }
    }

    for (std::size_t length = 2; length <= kFftSize; length <<= 1) {
        const double angle = -2.0 * std::numbers::pi / static_cast<double>(length);
        const std::complex<double> step{std::cos(angle), std::sin(angle)};
        for (std::size_t start = 0; start < kFftSize; start += length) {
            std::complex<double> phase{1.0, 0.0};
            const std::size_t half = length / 2;
            for (std::size_t offset = 0; offset < half; ++offset) {
                const auto even = values[start + offset];
                const auto odd = values[start + offset + half] * phase;
                values[start + offset] = even + odd;
                values[start + offset + half] = even - odd;
                phase *= step;
            }
        }
    }
}

[[nodiscard]] double fundamentalMagnitude(
    const std::array<std::complex<double>, kFftSize>& spectrum, std::uint8_t note) {
    const double frequency = drumas::core::noteToFrequency(note);
    const auto center = static_cast<std::size_t>(
        std::llround(frequency * static_cast<double>(kFftSize) / kSampleRate));
    double peak = 0.0;
    for (std::size_t bin = center > 2 ? center - 2 : 1;
         bin <= std::min(center + 2, kFftSize / 2); ++bin) {
        peak = std::max(peak, std::abs(spectrum[bin]) * 2.0 /
                                  static_cast<double>(kFftSize));
    }
    return peak;
}

}  // namespace

TEST_CASE("a music chord renders all expected fundamentals through the real graph",
          "[engine][music][fft]") {
    drumas::synth::SynthProcessor synth;
    drumas::engine::AudioGraph graph;
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    synth.prepare(spec);
    graph.prepare(spec);
    graph.setProcessor(&synth);

    const auto notes = drumas::music::chordToMidiNotes(
        {drumas::music::PitchClass::C, drumas::music::ChordQuality::Major, 0, 4});
    REQUIRE(notes.has_value());
    REQUIRE(notes->count == 3);
    REQUIRE(graph.pushCommand(drumas::core::selectPatch(2)));
    for (std::size_t index = 0; index < notes->count; ++index) {
        REQUIRE(graph.pushCommand(drumas::core::noteOn(notes->notes[index], 0.9F)));
    }

    std::vector<float> output(static_cast<std::size_t>(kBlockFrames) * 2);
    for (int block = 0; block < 32; ++block) {
        graph.render(output.data(), 2, kBlockFrames);
    }

    std::array<std::complex<double>, kFftSize> spectrum{};
    for (std::size_t offset = 0; offset < kFftSize; offset += kBlockFrames) {
        graph.render(output.data(), 2, kBlockFrames);
        for (int frame = 0; frame < kBlockFrames; ++frame) {
            const std::size_t index = offset + static_cast<std::size_t>(frame);
            const double window =
                0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * static_cast<double>(index) /
                                     static_cast<double>(kFftSize - 1));
            spectrum[index] = static_cast<double>(output[static_cast<std::size_t>(frame) * 2]) *
                              window;
        }
    }
    fft(spectrum);

    for (std::size_t index = 0; index < notes->count; ++index) {
        const double magnitude = fundamentalMagnitude(spectrum, notes->notes[index]);
        INFO("MIDI " << static_cast<int>(notes->notes[index]) << " fundamental magnitude "
                     << magnitude);
        REQUIRE(magnitude > 0.002);
    }
}

TEST_CASE("overlapping chord release and attack has no abrupt sample discontinuity",
          "[engine][music][continuity]") {
    drumas::synth::SynthProcessor synth;
    drumas::engine::AudioGraph graph;
    const drumas::core::AudioSpec spec{kSampleRate, kBlockFrames, 2};
    synth.prepare(spec);
    graph.prepare(spec);
    graph.setProcessor(&synth);

    const auto previousChord = drumas::music::chordToMidiNotes(
        {drumas::music::PitchClass::C, drumas::music::ChordQuality::Major, 0, 4});
    const auto nextChord = drumas::music::chordToMidiNotes(
        {drumas::music::PitchClass::D, drumas::music::ChordQuality::Minor, 0, 4});
    REQUIRE(previousChord.has_value());
    REQUIRE(nextChord.has_value());

    std::vector<float> output(static_cast<std::size_t>(kBlockFrames) * 2);
    REQUIRE(graph.pushCommand(drumas::core::selectPatch(2)));
    for (std::size_t index = 0; index < previousChord->count; ++index) {
        REQUIRE(graph.pushCommand(drumas::core::noteOn(previousChord->notes[index], 0.8F)));
    }

    float previousLeft = 0.0F;
    float previousRight = 0.0F;
    float baselineStep = 0.0F;
    for (int block = 0; block < 120; ++block) {
        graph.render(output.data(), 2, kBlockFrames);
        for (int frame = 0; frame < kBlockFrames; ++frame) {
            const float left = output[static_cast<std::size_t>(frame) * 2];
            const float right = output[static_cast<std::size_t>(frame) * 2 + 1];
            if (block >= 116) {
                baselineStep = std::max({baselineStep, std::fabs(left - previousLeft),
                                         std::fabs(right - previousRight)});
            }
            previousLeft = left;
            previousRight = right;
        }
    }

    for (std::size_t index = 0; index < previousChord->count; ++index) {
        REQUIRE(graph.pushCommand(drumas::core::noteOff(previousChord->notes[index])));
    }
    for (std::size_t index = 0; index < nextChord->count; ++index) {
        REQUIRE(graph.pushCommand(drumas::core::noteOn(nextChord->notes[index], 0.8F)));
    }

    float transitionStep = 0.0F;
    graph.render(output.data(), 2, kBlockFrames);
    for (int frame = 0; frame < kBlockFrames; ++frame) {
        const float left = output[static_cast<std::size_t>(frame) * 2];
        const float right = output[static_cast<std::size_t>(frame) * 2 + 1];
        transitionStep = std::max({transitionStep, std::fabs(left - previousLeft),
                                   std::fabs(right - previousRight)});
        previousLeft = left;
        previousRight = right;
    }

    INFO("steady step " << baselineStep << ", transition step " << transitionStep);
    REQUIRE(baselineStep > 0.0F);
    REQUIRE(transitionStep <= baselineStep * 2.0F + 0.02F);
}
