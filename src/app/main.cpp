// Console prototype front end.
//
// A, S, D, F, G, H, J play C4 D4 E4 F4 G4 A4 B4 as sine notes. Esc exits.
// A Windows console has no key-release event, so every key press starts a note
// with a fixed gate (kNoteGate); the note is released automatically.
//
// Threading: this file is the control thread. It only ever talks to the engine
// through pushCommand() and the atomic statistics, never through the processor
// object itself.
#include <array>
#include <chrono>
#include <conio.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <utility>

#include "core/AudioTypes.h"
#include "engine/AudioEngine.h"
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

struct Options {
    std::uint32_t bufferFrames = 256;
    std::uint32_t sampleRate = 48000;
    bool help = false;
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

void printStatus(const engine::AudioEngine& audio, synth::SynthProcessor& synth) {
    const engine::AudioGraph::Stats stats = audio.stats();
    const unsigned long long xruns = stats.lateBlocks + stats.overdueBlocks;
    std::printf("\rblock %u frames (%u..%u) | latency ~%5.2f ms | voices %2u | blocks %llu | "
                "xruns %llu (glitches %llu, overdue %llu) | max callback %6.3f ms   ",
                stats.lastBlockFrames, stats.minBlockFrames, stats.maxBlockFrames,
                audio.deviceInfo().estimatedLatencyMs, synth.activeVoices(),
                static_cast<unsigned long long>(stats.blocks), xruns,
                static_cast<unsigned long long>(stats.lateBlocks),
                static_cast<unsigned long long>(stats.overdueBlocks), stats.maxCallbackMs);
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

    synth::SynthProcessor synth;
    engine::AudioEngine audio;

    engine::AudioEngine::Config config;
    config.sampleRate = options.sampleRate;
    config.bufferFrames = options.bufferFrames;

    if (!audio.initialize(config, synth)) {
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

    std::printf("\nA S D F G H J = C D E F G A B.  Esc = quit.\n\n");

    std::array<Clock::time_point, core::kMidiNoteMax + 1> gateUntil{};
    std::array<bool, core::kMidiNoteMax + 1> gatePending{};

    bool running = true;
    Clock::time_point lastStatus = Clock::now();

    while (running) {
        while (_kbhit() != 0) {
            int key = _getch();
            if (key == 0 || key == 0xE0) {
                // Extended key: swallow the scan code so it is not interpreted
                // as a character.
                (void)_getch();
                continue;
            }
            if (key == kEscapeKey) {
                running = false;
                break;
            }
            if (key >= 'a' && key <= 'z') {
                key -= 'a' - 'A';
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
            printStatus(audio, synth);
            lastStatus = now;
        }

        std::this_thread::sleep_for(kPollInterval);
    }

    audio.stop();
    printSummary(audio);
    audio.shutdown();
    return 0;
}
