// Console prototype front end.
//
// A, S, D, F, G, H, J play C4..B4. F1/F2/F3 select the factory patch;
// 1/2/3 choose cutoff, resonance or release; Up/Down (or +/-) adjust it.
// Esc exits.
//
// A Windows console has no key-release event, so every note key starts a note
// with a fixed gate (kNoteGate); the note is released automatically.
//
// The LFO and the patch bank live in the audio thread; this file only ever
// talks to the engine through pushCommand() and the atomic statistics, never
// through the processor object itself. The values printed here are the ones
// the console last sent, which is the correct model for a UI that owns its
// controls.
#include <algorithm>
#include <array>
#include <chrono>
#include <conio.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "core/AudioTypes.h"
#include "engine/AudioEngine.h"
#include "synth/ParameterIds.h"
#include "synth/PatchBank.h"
#include "synth/SynthProcessor.h"

namespace engine = drumas::engine;
namespace synth = drumas::synth;
namespace core = drumas::core;

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kEscapeKey = 27;
constexpr std::uint8_t kNoNote = 0xFF;

constexpr std::array<std::pair<int, std::uint8_t>, 7> kKeyMap = {
    {{'A', 60}, {'S', 62}, {'D', 64}, {'F', 65}, {'G', 67}, {'H', 69}, {'J', 71}}};

constexpr auto kNoteGate = std::chrono::milliseconds(400);
constexpr auto kPollInterval = std::chrono::milliseconds(5);
constexpr auto kStatusInterval = std::chrono::milliseconds(500);

constexpr float kVelocity = 1.0F;

enum class EditTarget : std::uint8_t { Cutoff = 0, Resonance = 1, Release = 2 };

constexpr float kCutoffStepRatio = 1.12F;
constexpr float kReleaseStepRatio = 1.20F;
constexpr float kResonanceStep = 0.05F;

struct Options {
    std::uint32_t bufferFrames = 256;
    std::uint32_t sampleRate = 48000;
    bool help = false;
};

/// Control-side mirror of what the console has sent. Never read from the audio
/// thread; the authoritative state lives in the processor.
struct ConsoleState {
    int patchIndex = synth::kDefaultPatchIndex;
    EditTarget target = EditTarget::Cutoff;
    float cutoffHz = synth::kFactoryPatches[synth::kDefaultPatchIndex].cutoffHz;
    float resonance = synth::kFactoryPatches[synth::kDefaultPatchIndex].resonance;
    float releaseMs = synth::kFactoryPatches[synth::kDefaultPatchIndex].ampEnvelope.releaseMs;
};

void printUsage(const char* executable) {
    std::printf("Usage: %s [--buffer=<frames>] [--rate=<hz>]\n"
                "\n"
                "  --buffer=<frames>  Requested device period (default 256)\n"
                "  --rate=<hz>        Sample rate (default 48000)\n"
                "  --help             Show this message\n",
                executable);
}

[[nodiscard]] bool parseUnsigned(const std::string& text, std::uint32_t& value) {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const unsigned long parsed = std::strtoul(text.c_str(), &end, 10);
    if (end == nullptr || *end != '\0' || parsed == 0) {
        return false;
    }
    value = static_cast<std::uint32_t>(parsed);
    return true;
}

[[nodiscard]] bool parseOptions(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--help" || argument == "-h") {
            options.help = true;
            continue;
        }

        constexpr const char* kBufferPrefix = "--buffer=";
        constexpr const char* kRatePrefix = "--rate=";
        if (argument.rfind(kBufferPrefix, 0) == 0) {
            if (!parseUnsigned(argument.substr(std::strlen(kBufferPrefix)), options.bufferFrames)) {
                std::printf("Invalid --buffer value: %s\n", argument.c_str());
                return false;
            }
            continue;
        }
        if (argument.rfind(kRatePrefix, 0) == 0) {
            if (!parseUnsigned(argument.substr(std::strlen(kRatePrefix)), options.sampleRate)) {
                std::printf("Invalid --rate value: %s\n", argument.c_str());
                return false;
            }
            continue;
        }

        std::printf("Unknown argument: %s\n", argument.c_str());
        return false;
    }
    return true;
}

[[nodiscard]] std::uint8_t noteForKey(int key) {
    for (const auto& [mappedKey, note] : kKeyMap) {
        if (mappedKey == key) {
            return note;
        }
    }
    return kNoNote;
}

[[nodiscard]] std::string_view patchName(const ConsoleState& state) {
    return synth::kFactoryPatches[static_cast<std::size_t>(state.patchIndex)].name;
}

void printPatchLine(const ConsoleState& state) {
    const std::string_view name = patchName(state);
    std::printf("\nPatch: %.*s | cutoff %.0f Hz | resonance %.2f | release %.0f ms\n",
                static_cast<int>(name.size()), name.data(),
                static_cast<double>(state.cutoffHz), static_cast<double>(state.resonance),
                static_cast<double>(state.releaseMs));
    std::fflush(stdout);
}

/// Applies one up/down step to the selected parameter, sends the command and
/// prints the new value. `direction` is +1 or -1.
void adjustParameter(engine::AudioEngine& audio, ConsoleState& state, int direction) {
    const float step = direction > 0 ? 1.0F : -1.0F;
    switch (state.target) {
        case EditTarget::Cutoff: {
            const float scale = direction > 0 ? kCutoffStepRatio : 1.0F / kCutoffStepRatio;
            state.cutoffHz = std::clamp(state.cutoffHz * scale, 20.0F, 20000.0F);
            (void)audio.pushCommand(core::setParam(synth::kFilterCutoff, state.cutoffHz));
            std::printf("\ncutoff: %.0f Hz\n", static_cast<double>(state.cutoffHz));
            break;
        }
        case EditTarget::Resonance: {
            state.resonance = std::clamp(state.resonance + step * kResonanceStep, 0.0F, 1.0F);
            (void)audio.pushCommand(core::setParam(synth::kFilterResonance, state.resonance));
            std::printf("\nresonance: %.2f\n", static_cast<double>(state.resonance));
            break;
        }
        case EditTarget::Release: {
            const float scale = direction > 0 ? kReleaseStepRatio : 1.0F / kReleaseStepRatio;
            state.releaseMs = std::clamp(state.releaseMs * scale, 0.5F, 20000.0F);
            (void)audio.pushCommand(core::setParam(synth::kAmpReleaseMs, state.releaseMs));
            std::printf("\nrelease: %.0f ms\n", static_cast<double>(state.releaseMs));
            break;
        }
    }
    std::fflush(stdout);
}

void selectPatch(engine::AudioEngine& audio, ConsoleState& state, int patchIndex) {
    if (patchIndex < 0 || patchIndex >= synth::kFactoryPatchCount) {
        return;
    }
    state.patchIndex = patchIndex;
    const synth::Patch& patch =
        synth::kFactoryPatches[static_cast<std::size_t>(patchIndex)];
    // The processor resets its live overrides to the patch when the command
    // arrives; the console mirrors the same values so the display is truthful.
    state.cutoffHz = patch.cutoffHz;
    state.resonance = patch.resonance;
    state.releaseMs = patch.ampEnvelope.releaseMs;
    (void)audio.pushCommand(core::selectPatch(static_cast<std::uint8_t>(patchIndex)));
    printPatchLine(state);
}

void printStatus(const engine::AudioEngine& audio, synth::SynthProcessor& synthProcessor,
                 const ConsoleState& state) {
    const engine::AudioGraph::Stats stats = audio.stats();
    const unsigned long long xruns = stats.lateBlocks + stats.overdueBlocks;
    const std::string_view name = patchName(state);
    std::printf("\r%.*s | cutoff %5.0f Hz | res %.2f | rel %5.0f ms | voices %2u | "
                "blocks %llu | xruns %llu | max cb %6.3f ms   ",
                static_cast<int>(name.size()), name.data(),
                static_cast<double>(state.cutoffHz), static_cast<double>(state.resonance),
                static_cast<double>(state.releaseMs), synthProcessor.activeVoices(),
                static_cast<unsigned long long>(stats.blocks), xruns, stats.maxCallbackMs);
    std::fflush(stdout);
}

void printSummary(const engine::AudioEngine& audio) {
    const engine::AudioGraph::Stats stats = audio.stats();
    const engine::AudioEngine::DeviceInfo info = audio.deviceInfo();
    std::printf("\n\nStopped.\n");
    std::printf("Backend           : %s\n", info.backend.c_str());
    std::printf("Device            : %s\n", info.deviceName.c_str());
    std::printf("Sample rate       : %.0f Hz\n", info.sampleRate);
    std::printf("Channels          : %d\n", info.channels);
    std::printf("Reported period   : %d frames\n", info.reportedBufferFrames);
    std::printf("Block frames      : %u..%u (last %u)\n", stats.minBlockFrames,
                stats.maxBlockFrames, stats.lastBlockFrames);
    std::printf("Frames rendered   : %llu\n", static_cast<unsigned long long>(stats.frames));
    std::printf("Callbacks         : %llu\n", static_cast<unsigned long long>(stats.blocks));
    std::printf("Stream glitches   : %llu\n", static_cast<unsigned long long>(stats.lateBlocks));
    std::printf("Overdue callbacks : %llu\n", static_cast<unsigned long long>(stats.overdueBlocks));
    std::printf("Max callback time : %.3f ms\n", stats.maxCallbackMs);
    std::printf("Commands applied  : %llu\n",
                static_cast<unsigned long long>(stats.commandsApplied));
    std::printf("Commands dropped  : %llu\n",
                static_cast<unsigned long long>(stats.commandsDropped));
    std::printf("Device events     : %llu\n", static_cast<unsigned long long>(info.deviceEvents));
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseOptions(argc, argv, options)) {
        printUsage(argv[0]);
        return 2;
    }
    if (options.help) {
        printUsage(argv[0]);
        return 0;
    }

    std::printf("drumaspro -- engine prototype\n\n");

    synth::SynthProcessor synthProcessor;
    engine::AudioEngine audio;

    engine::AudioEngine::Config config;
    config.sampleRate = options.sampleRate;
    config.bufferFrames = options.bufferFrames;

    if (!audio.initialize(config, synthProcessor)) {
        std::printf("ERROR: %s\n", std::string(audio.lastError()).c_str());
        return 1;
    }

    const engine::AudioEngine::DeviceInfo info = audio.deviceInfo();
    std::printf("Backend        : %s\n", info.backend.c_str());
    std::printf("Device         : %s\n", info.deviceName.c_str());
    std::printf("Sample rate    : %.0f Hz\n", info.sampleRate);
    std::printf("Channels       : %d\n", info.channels);
    std::printf("Requested      : %u frames\n", options.bufferFrames);
    std::printf("Device period  : %d frames (reported by the driver)\n", info.reportedBufferFrames);
    std::printf("Latency (est.) : %.2f ms (buffer only)\n", info.estimatedLatencyMs);

    if (!audio.start()) {
        std::printf("ERROR: %s\n", std::string(audio.lastError()).c_str());
        return 1;
    }

    ConsoleState consoleState;
    std::printf("\nA S D F G H J = C D E F G A B.  F1/F2/F3 = patch.  "
                "1/2/3 = cutoff/resonance/release.  Up/Down or +/- = adjust.  Esc = quit.\n");
    printPatchLine(consoleState);
    std::printf("\n");

    std::array<Clock::time_point, core::kMidiNoteMax + 1> gateUntil{};
    std::array<bool, core::kMidiNoteMax + 1> gatePending{};

    bool running = true;
    Clock::time_point lastStatus = Clock::now();

    while (running) {
        while (_kbhit() != 0) {
            int key = _getch();
            if (key == 0 || key == 0xE0) {
                // Extended key: the second code selects the key.
                const int scan = _getch();
                if (key == 0 && scan >= 0x3B && scan <= 0x3D) {
                    selectPatch(audio, consoleState, scan - 0x3B);
                } else if (key == 0xE0 && (scan == 0x48 || scan == 0x50)) {
                    adjustParameter(audio, consoleState, scan == 0x48 ? 1 : -1);
                }
                continue;
            }
            if (key == kEscapeKey) {
                running = false;
                break;
            }
            if (key >= 'a' && key <= 'z') {
                key -= 'a' - 'A';
            }
            if (key >= '1' && key <= '3') {
                consoleState.target = static_cast<EditTarget>(key - '1');
                const char* names[] = {"cutoff", "resonance", "release"};
                std::printf("\nEditing %s (Up/Down or +/-)\n",
                            names[static_cast<int>(consoleState.target)]);
                std::fflush(stdout);
                continue;
            }
            if (key == '+' || key == '=') {
                adjustParameter(audio, consoleState, 1);
                continue;
            }
            if (key == '-' || key == '_') {
                adjustParameter(audio, consoleState, -1);
                continue;
            }
            const std::uint8_t note = noteForKey(key);
            if (note == kNoNote) {
                continue;
            }
            (void)audio.pushCommand(core::noteOn(note, kVelocity));
            gateUntil[note] = Clock::now() + kNoteGate;
            gatePending[note] = true;
        }
        if (!running) {
            break;
        }

        const Clock::time_point now = Clock::now();
        for (std::size_t note = 0; note <= core::kMidiNoteMax; ++note) {
            if (gatePending[note] && now >= gateUntil[note]) {
                (void)audio.pushCommand(core::noteOff(static_cast<std::uint8_t>(note)));
                gatePending[note] = false;
            }
        }

        if (now - lastStatus >= kStatusInterval) {
            printStatus(audio, synthProcessor, consoleState);
            lastStatus = now;
        }

        std::this_thread::sleep_for(kPollInterval);
    }

    audio.stop();
    printSummary(audio);
    audio.shutdown();
    return 0;
}
