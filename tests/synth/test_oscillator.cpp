#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>
#include <vector>

#include <dspark/Core/FFT.h>

#include "synth/BandLimitedOscillator.h"

namespace {

using drumas::synth::BandLimitedOscillator;
using drumas::synth::Waveform;

constexpr double kSampleRate = 48000.0;
constexpr std::size_t kFftSize = 65536;

/// Frames rendered per measurement. One full FFT window; the FFT is the
/// measurement instrument, not part of the synth path.
constexpr int kRenderFrames = static_cast<int>(kFftSize);

struct Spectrum {
    std::vector<double> power;  // N/2 + 1 bins
    double binHz = 0.0;
};

[[nodiscard]] Spectrum analyze(const std::vector<float>& signal) {
    REQUIRE(signal.size() == kFftSize);

    // 4-term Blackman-Harris: sidelobes about -92 dB, so leakage from a strong
    // harmonic cannot masquerade as aliasing 20-30 dB down.
    constexpr double a0 = 0.35875;
    constexpr double a1 = 0.48829;
    constexpr double a2 = 0.14128;
    constexpr double a3 = 0.01168;
    constexpr double twoPi = 2.0 * std::numbers::pi;
    const double denominator = static_cast<double>(kFftSize - 1);

    std::vector<float> windowed(kFftSize);
    for (std::size_t index = 0; index < kFftSize; ++index) {
        const double phase = twoPi * static_cast<double>(index) / denominator;
        const double window = a0 - a1 * std::cos(phase) + a2 * std::cos(2.0 * phase) -
                              a3 * std::cos(3.0 * phase);
        windowed[index] = static_cast<float>(static_cast<double>(signal[index]) * window);
    }

    dspark::FFTReal<float> fft(kFftSize);
    std::vector<float> frequencyDomain(fft.getFrequencyDomainSize());
    std::vector<float> magnitudes(fft.getNumBins());
    fft.forward(windowed.data(), frequencyDomain.data());
    fft.computeMagnitudes(frequencyDomain.data(), magnitudes.data());

    Spectrum spectrum;
    spectrum.binHz = kSampleRate / static_cast<double>(kFftSize);
    spectrum.power.resize(magnitudes.size());
    for (std::size_t bin = 0; bin < magnitudes.size(); ++bin) {
        const double magnitude = static_cast<double>(magnitudes[bin]);
        spectrum.power[bin] = magnitude * magnitude;
    }
    return spectrum;
}

/// Parabolic interpolation of the peak of a magnitude spectrum, in Hz.
[[nodiscard]] double peakFrequency(const Spectrum& spectrum) {
    std::size_t peak = 1;
    for (std::size_t bin = 2; bin + 1 < spectrum.power.size(); ++bin) {
        if (spectrum.power[bin] > spectrum.power[peak]) {
            peak = bin;
        }
    }

    const double left = std::log(spectrum.power[peak - 1] + 1e-30);
    const double center = std::log(spectrum.power[peak] + 1e-30);
    const double right = std::log(spectrum.power[peak + 1] + 1e-30);
    const double curvature = left - 2.0 * center + right;
    const double offset = curvature != 0.0 ? 0.5 * (left - right) / curvature : 0.0;
    return (static_cast<double>(peak) + offset) * spectrum.binHz;
}

[[nodiscard]] double centsBetween(double measured, double expected) {
    return 1200.0 * std::log2(measured / expected);
}

/// Total power outside every harmonic zone. That energy is what aliasing
/// produces: a band-limited periodic signal has no other components.
///
/// Only the audible band counts (up to 20 kHz). A harmonic sitting a few hertz
/// above Nyquist folds back to a few hertz below it: it is a real fold, but it
/// lands where nobody can hear it and no finite band-limiting window removes
/// it completely. Counted everywhere, that edge case would dominate the score
/// of an otherwise clean oscillator.
[[nodiscard]] double aliasPower(const Spectrum& spectrum, double fundamental, int halfWidth) {
    constexpr double kMaxAudibleFrequency = 20000.0;
    double total = 0.0;
    for (std::size_t bin = 4; bin < spectrum.power.size(); ++bin) {
        const double frequency = static_cast<double>(bin) * spectrum.binHz;
        if (frequency >= kMaxAudibleFrequency) {
            continue;
        }
        const double nearestHarmonic = std::round(frequency / fundamental);
        if (nearestHarmonic < 1.0) {
            total += spectrum.power[bin];
            continue;
        }
        const double distance =
            std::fabs(frequency - nearestHarmonic * fundamental) / spectrum.binHz;
        if (distance > static_cast<double>(halfWidth)) {
            total += spectrum.power[bin];
        }
    }
    return total;
}

/// Power around the fundamental only, for the dBc reference.
[[nodiscard]] double fundamentalPower(const Spectrum& spectrum, double fundamental,
                                      int halfWidth) {
    const long center =
        static_cast<long>(std::llround(fundamental / spectrum.binHz));
    double total = 0.0;
    for (long bin = std::max<long>(1, center - halfWidth);
         bin <= center + halfWidth && bin < static_cast<long>(spectrum.power.size()); ++bin) {
        total += spectrum.power[static_cast<std::size_t>(bin)];
    }
    return total;
}

[[nodiscard]] double aliasDb(const Spectrum& spectrum, double fundamental) {
    constexpr int kHalfWidth = 8;  // Blackman-Harris main lobe is ~4 bins per side
    const double alias = aliasPower(spectrum, fundamental, kHalfWidth);
    const double reference = fundamentalPower(spectrum, fundamental, kHalfWidth);
    return 10.0 * std::log10((alias + 1e-30) / (reference + 1e-30));
}

/// Renders a BandLimitedOscillator with a given waveform and frequency.
[[nodiscard]] std::vector<float> renderOscillator(Waveform waveform, float frequency,
                                                  float pulseWidth = 0.5F) {
    BandLimitedOscillator oscillator;
    oscillator.prepare(kSampleRate);
    oscillator.setWaveform(waveform);
    oscillator.setPulseWidth(pulseWidth);
    oscillator.setFrequency(frequency);
    oscillator.reset();

    std::vector<float> samples(kFftSize);
    for (float& sample : samples) {
        sample = oscillator.nextSample();
    }
    return samples;
}

/// Same signal without any band-limiting, as the "before" reference.
[[nodiscard]] std::vector<float> renderNaiveSaw(double frequency) {
    std::vector<float> samples(kFftSize);
    double phase = 0.0;
    const double increment = frequency / kSampleRate;
    for (float& sample : samples) {
        sample = static_cast<float>(2.0 * phase - 1.0);
        phase += increment;
        if (phase >= 1.0) {
            phase -= 1.0;
        }
    }
    return samples;
}

/// Frequencies that fall exactly on FFT bins, so the harmonic zones are exact.
[[nodiscard]] double binFrequency(std::size_t bin) {
    return static_cast<double>(bin) * kSampleRate / static_cast<double>(kFftSize);
}

}  // namespace

TEST_CASE("band-limited oscillator tunes to the requested frequency", "[synth][oscillator]") {
    const Waveform waveforms[] = {Waveform::Sine, Waveform::Triangle, Waveform::Saw,
                                  Waveform::Square};
    const float frequencies[] = {110.0F, 440.0F, 1000.0F, 3520.0F, 7040.0F};

    for (const Waveform waveform : waveforms) {
        for (const float frequency : frequencies) {
            const std::vector<float> samples = renderOscillator(waveform, frequency);
            const Spectrum spectrum = analyze(samples);
            const double measured = peakFrequency(spectrum);
            const double cents = centsBetween(measured, static_cast<double>(frequency));
            INFO("waveform " << static_cast<int>(waveform) << " at " << frequency
                             << " Hz: measured " << measured << " Hz, " << cents << " cents");
            // A cent is the resolution a musician hears; 5 cents leaves room
            // for the FFT peak interpolation without hiding a real mistune.
            REQUIRE(std::fabs(cents) < 5.0);
        }
    }
}

TEST_CASE("band-limited saw keeps aliasing far below a naive saw", "[synth][oscillator][alias]") {
    // 1 kHz, 1.5 kHz and 3 kHz: at each of them some fold-downs land inside
    // the audible band, so the metric cannot be passed by accident.
    const double frequencies[] = {binFrequency(1365), binFrequency(2047), binFrequency(4093)};

    for (const double frequency : frequencies) {
        const Spectrum bandLimited =
            analyze(renderOscillator(Waveform::Saw, static_cast<float>(frequency)));
        const Spectrum naive = analyze(renderNaiveSaw(frequency));

        const double bandLimitedDb = aliasDb(bandLimited, frequency);
        const double naiveDb = aliasDb(naive, frequency);
        const double improvement = naiveDb - bandLimitedDb;

        INFO("saw at " << frequency << " Hz: PolyBLEP " << bandLimitedDb << " dBc, naive "
                       << naiveDb << " dBc, improvement " << improvement << " dB");

        // Measured floor is -86 dBc or better across these frequencies; -75
        // leaves margin without hiding a regression. The naive saw is the
        // control that proves the measurement is not vacuous.
        REQUIRE(bandLimitedDb < -75.0);
        REQUIRE(naiveDb > bandLimitedDb + 50.0);
    }
}

TEST_CASE("band-limited pulse keeps aliasing low and no DC offset", "[synth][oscillator][alias]") {
    const double frequencies[] = {binFrequency(1365), binFrequency(4093)};

    for (const double frequency : frequencies) {
        for (const float width : {0.5F, 0.25F, 0.1F}) {
            const std::vector<float> samples =
                renderOscillator(Waveform::Square, static_cast<float>(frequency), width);
            const Spectrum spectrum = analyze(samples);
            const double db = aliasDb(spectrum, frequency);
            INFO("pulse width " << width << " at " << frequency << " Hz: " << db << " dBc");

            double mean = 0.0;
            for (const float sample : samples) {
                mean += sample;
            }
            mean /= static_cast<double>(samples.size());
            REQUIRE(std::fabs(mean) < 1e-3);

            // A narrow pulse has less fundamental energy for the same alias
            // floor, so its dBc figure is legitimately worse.
            REQUIRE(db < (width == 0.1F ? -75.0 : -80.0));
        }
    }
}

TEST_CASE("band-limited oscillator stays finite at the extremes", "[synth][oscillator]") {
    BandLimitedOscillator oscillator;
    oscillator.prepare(kSampleRate);

    const Waveform waveforms[] = {Waveform::Sine, Waveform::Triangle, Waveform::Saw,
                                  Waveform::Square};
    for (const Waveform waveform : waveforms) {
        oscillator.setWaveform(waveform);
        for (const float frequency : {0.0F, 20.0F, 24000.0F, 30000.0F}) {
            oscillator.setFrequency(frequency);
            oscillator.reset();
            for (int frame = 0; frame < 2048; ++frame) {
                const float value = oscillator.nextSample();
                REQUIRE(std::isfinite(value));
            }
        }
    }

    oscillator.setFrequency(std::numeric_limits<float>::quiet_NaN());
    oscillator.setPulseWidth(std::numeric_limits<float>::quiet_NaN());
    oscillator.setPhase(std::numeric_limits<float>::infinity());
    oscillator.setFrequency(440.0F);
    oscillator.setWaveform(Waveform::Square);
    oscillator.setPulseWidth(0.0F);
    REQUIRE(oscillator.pulseWidth() == Catch::Approx(BandLimitedOscillator::kMinPulseWidth));
    oscillator.setPulseWidth(1.0F);
    REQUIRE(oscillator.pulseWidth() == Catch::Approx(BandLimitedOscillator::kMaxPulseWidth));
}

TEST_CASE("resetting the oscillator restarts the phase", "[synth][oscillator]") {
    BandLimitedOscillator oscillator;
    oscillator.prepare(kSampleRate);
    oscillator.setFrequency(440.0F);
    oscillator.setWaveform(Waveform::Saw);
    oscillator.reset();

    const float first = oscillator.nextSample();
    for (int frame = 0; frame < 100; ++frame) {
        (void)oscillator.nextSample();
    }
    oscillator.reset();
    REQUIRE(oscillator.nextSample() == Catch::Approx(first));
    REQUIRE(oscillator.phase() == Catch::Approx(440.0F / 48000.0F));
}


