// Console prototype front end.
//
// A, S, D, F, G, H, J play C4..B4. F1/F2/F3 select the factory patch.
// Q/W/E/R/T/Y select effect slots 1..6; B toggles bypass; N cycles the type;
// 1/2/3 select an effect parameter, 4 selects its dry/wet mix; Tab toggles
// between synth and effect editing; Up/Down or +/- adjust the selection.
// 0 returns to the synth parameters. Esc exits.
//
// A Windows console has no key-release event, so every note key starts a note
// with a fixed gate (kNoteGate); the note is released automatically.
//
// The console owns only mirrors: the synth and effects live on the audio
// thread. Commands travel through the lock-free queue and retired effects are
// returned through the chain's SPSC retirement queue, which the main loop drains
// and destroys here.
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <conio.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

#include "core/AudioTypes.h"
#include "core/ParameterRegistry.h"
#include "engine/AudioEngine.h"
#include "fx/ChainDescription.h"
#include "fx/EffectFactory.h"
#include "fx/EffectTypes.h"
#include "synth/ParameterIds.h"
#include "synth/PatchBank.h"
#include "synth/SynthProcessor.h"

namespace engine = drumas::engine;
namespace synth = drumas::synth;
namespace core = drumas::core;
namespace fx = drumas::fx;

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kEscapeKey = 27;
constexpr std::uint8_t kNoNote = 0xFF;

constexpr std::array<std::pair<int, std::uint8_t>, 7> kKeyMap = {
    {{'A', 60}, {'S', 62}, {'D', 64}, {'F', 65}, {'G', 67}, {'H', 69}, {'J', 71}}};

constexpr std::array<std::pair<int, std::uint8_t>, 6> kSlotKeys = {
    {{'Q', 0}, {'W', 1}, {'E', 2}, {'R', 3}, {'T', 4}, {'Y', 5}}};

constexpr auto kNoteGate = std::chrono::milliseconds(400);
constexpr auto kPollInterval = std::chrono::milliseconds(5);
constexpr auto kStatusInterval = std::chrono::milliseconds(500);

constexpr float kVelocity = 1.0F;

enum class EditTarget : std::uint8_t { Cutoff = 0, Resonance = 1, Release = 2 };
enum class Focus : std::uint8_t { Synth = 0, Effects = 1 };

constexpr float kCutoffStepRatio = 1.12F;
constexpr float kReleaseStepRatio = 1.20F;
constexpr float kResonanceStep = 0.05F;

struct Options {
    std::uint32_t bufferFrames = 256;
    std::uint32_t sampleRate = 48000;
    bool help = false;
};

struct FxParameterState {
    core::ParameterDescriptor descriptor{};
    float value = 0.0F;
};

struct FxSlotState {
    fx::EffectTypeId type = fx::kNoEffectType;
    bool bypass = true;
    float mix = 0.5F;
    int parameterCount = 0;
    std::array<FxParameterState, fx::kMaxParametersPerEffect> parameters{};
};

void fillSlotState(FxSlotState& slot, fx::EffectTypeId type, bool bypass, float mix) {
    auto effect = fx::createEffect(type);
    slot = FxSlotState{};
    slot.type = type;
    slot.bypass = bypass;
    slot.mix = mix;
    if (effect == nullptr) return;
    const core::ParameterRegistry& registry = effect->parameters();
    for (std::size_t index = 0; index < registry.size() &&
                                slot.parameterCount < static_cast<int>(slot.parameters.size());
         ++index) {
        FxParameterState& parameter = slot.parameters[static_cast<std::size_t>(
            slot.parameterCount++)];
        parameter.descriptor = registry.at(index);
        parameter.value = parameter.descriptor.defaultValue;
    }
}

constexpr auto& kEffectCycle = fx::kEffectTypeCycle;

[[nodiscard]] std::string_view shortEffectName(fx::EffectTypeId type) noexcept {
    if (type == fx::kReverbType) return "Rvb";
    if (type == fx::kDelayType) return "Dly";
    if (type == fx::kChorusType) return "Cho";
    if (type == fx::kDriveType) return "Drv";
    if (type == fx::kFilterType) return "Flt";
    if (type == fx::kFlangerType) return "Flg";
    if (type == fx::kPhaserType) return "Phs";
    if (type == fx::kTremoloType) return "Trm";
    if (type == fx::kVibratoType) return "Vib";
    if (type == fx::kEqType) return "EQ ";
    if (type == fx::kLoFiType) return "LoF";
    return "---";
}

/// Control-side mirror of what the console has sent. Never read from the audio
/// thread; the authoritative state lives in the processor.
struct ConsoleState {
    int patchIndex = synth::kDefaultPatchIndex;
    Focus focus = Focus::Synth;
    EditTarget target = EditTarget::Cutoff;
    float cutoffHz = synth::kFactoryPatches[synth::kDefaultPatchIndex].cutoffHz;
    float resonance = synth::kFactoryPatches[synth::kDefaultPatchIndex].resonance;
    float releaseMs = synth::kFactoryPatches[synth::kDefaultPatchIndex].ampEnvelope.releaseMs;

    std::size_t fxSlot = 0;
    int fxTarget = 0;  // 0..2 parameter, 3 mix
    std::array<FxSlotState, fx::kEffectSlotCount> slots{};
    core::AudioSpec spec{};
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

[[nodiscard]] int slotForKey(int key) {
    for (const auto& [mappedKey, slot] : kSlotKeys) {
        if (mappedKey == key) return slot;
    }
    return -1;
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

void printChainLine(const ConsoleState& state, const synth::SynthProcessor& processor) {
    std::printf("Chain:");
    for (std::size_t index = 0; index < state.slots.size(); ++index) {
        const FxSlotState& slot = state.slots[index];
        std::printf(" %zu:%s%s", index + 1, shortEffectName(slot.type).data(),
                    slot.bypass ? "-" : "*");
    }
    std::printf(" | total latency %.2f ms (inserts + limiter)\n",
                processor.latencyMs());
    std::fflush(stdout);
}

void printFxLine(const ConsoleState& state) {
    const FxSlotState& slot = state.slots[state.fxSlot];
    std::printf("\nFX %zu: %s | bypass %s | mix %.2f | latency is in the chain line\n",
                state.fxSlot + 1, shortEffectName(slot.type).data(),
                slot.bypass ? "ON" : "OFF", static_cast<double>(slot.mix));
    for (int index = 0; index < slot.parameterCount; ++index) {
        const FxParameterState& parameter = slot.parameters[static_cast<std::size_t>(index)];
        std::printf("  %s%d %.*s = %.3f", index == state.fxTarget ? ">" : " ", index + 1,
                    static_cast<int>(parameter.descriptor.name.size()),
                    parameter.descriptor.name.data(),
                    static_cast<double>(parameter.value));
    }
    std::printf("\n");
    std::fflush(stdout);
}

/// Applies one up/down step to the selected synth parameter, sends the command
/// and prints the new value. `direction` is +1 or -1.
void adjustSynthParameter(engine::AudioEngine& audio, ConsoleState& state, int direction) {
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

[[nodiscard]] float normalizedValue(const core::ParameterDescriptor& descriptor,
                                    float value) noexcept {
    const float clamped = std::clamp(value, descriptor.minValue, descriptor.maxValue);
    if (descriptor.curve == core::ParameterCurve::Logarithmic) {
        return std::log(clamped / descriptor.minValue) /
               std::log(descriptor.maxValue / descriptor.minValue);
    }
    return (clamped - descriptor.minValue) / (descriptor.maxValue - descriptor.minValue);
}

[[nodiscard]] float realValue(const core::ParameterDescriptor& descriptor,
                              float normalized) noexcept {
    const float clamped = std::clamp(normalized, 0.0F, 1.0F);
    if (descriptor.curve == core::ParameterCurve::Logarithmic) {
        return descriptor.minValue *
               std::pow(descriptor.maxValue / descriptor.minValue, clamped);
    }
    return descriptor.minValue + clamped * (descriptor.maxValue - descriptor.minValue);
}

void adjustFxParameter(engine::AudioEngine& audio, ConsoleState& state, int direction) {
    FxSlotState& slot = state.slots[state.fxSlot];
    if (state.fxTarget == 3) {
        const float next = std::clamp(slot.mix + static_cast<float>(direction) * 0.05F,
                                      0.0F, 1.0F);
        if (audio.pushCommand(core::setEffectMix(static_cast<std::uint8_t>(state.fxSlot), next))) {
            slot.mix = next;
        }
        std::printf("\nFX %zu mix: %.2f\n", state.fxSlot + 1, static_cast<double>(slot.mix));
        std::fflush(stdout);
        return;
    }
    if (slot.parameterCount == 0) return;
    const int index = std::clamp(state.fxTarget, 0, slot.parameterCount - 1);
    FxParameterState& parameter = slot.parameters[static_cast<std::size_t>(index)];
    const core::ParameterDescriptor& descriptor = parameter.descriptor;

    float next = parameter.value;
    const bool discrete = descriptor.curve == core::ParameterCurve::Linear &&
                          descriptor.unit == core::ParameterUnit::None &&
                          descriptor.maxValue - descriptor.minValue <= 8.0F;
    if (discrete) {
        next = std::clamp(std::round(next) + static_cast<float>(direction),
                          descriptor.minValue, descriptor.maxValue);
    } else {
        const float step = descriptor.curve == core::ParameterCurve::Linear ? 0.03F : 0.02F;
        const float normalized =
            std::clamp(normalizedValue(descriptor, next) +
                           static_cast<float>(direction) * step,
                       0.0F, 1.0F);
        next = realValue(descriptor, normalized);
    }
    if (audio.pushCommand(core::setEffectParam(static_cast<std::uint8_t>(state.fxSlot),
                                               descriptor.id, next))) {
        parameter.value = next;
    }
    std::printf("\nFX %zu %.*s: %.3f\n", state.fxSlot + 1,
                static_cast<int>(descriptor.name.size()), descriptor.name.data(),
                static_cast<double>(parameter.value));
    std::fflush(stdout);
}

void adjustSelected(engine::AudioEngine& audio, ConsoleState& state, int direction) {
    if (state.focus == Focus::Effects) {
        adjustFxParameter(audio, state, direction);
    } else {
        adjustSynthParameter(audio, state, direction);
    }
}

void selectPatch(engine::AudioEngine& audio, ConsoleState& state, int patchIndex) {
    if (patchIndex < 0 || patchIndex >= synth::kFactoryPatchCount) {
        return;
    }
    state.patchIndex = patchIndex;
    const synth::Patch& patch =
        synth::kFactoryPatches[static_cast<std::size_t>(patchIndex)];
    state.cutoffHz = patch.cutoffHz;
    state.resonance = patch.resonance;
    state.releaseMs = patch.ampEnvelope.releaseMs;
    (void)audio.pushCommand(core::selectPatch(static_cast<std::uint8_t>(patchIndex)));
    printPatchLine(state);
}

void selectFxSlot(ConsoleState& state, int slot) {
    if (slot < 0 || slot >= static_cast<int>(state.slots.size())) return;
    state.fxSlot = static_cast<std::size_t>(slot);
    state.fxTarget = 0;
    state.focus = Focus::Effects;
    printFxLine(state);
}

void cycleEffectType(engine::AudioEngine& audio, ConsoleState& state,
                     const synth::SynthProcessor& processor) {
    FxSlotState& current = state.slots[state.fxSlot];
    const auto found = std::find(kEffectCycle.begin(), kEffectCycle.end(), current.type);
    const std::size_t index = found == kEffectCycle.end()
                                  ? 0
                                  : (static_cast<std::size_t>(found - kEffectCycle.begin()) + 1) %
                                        kEffectCycle.size();
    const fx::EffectTypeId nextType = kEffectCycle[index];

    auto effect = fx::createPreparedEffect(nextType, state.spec);
    if (effect == nullptr) return;

    FxSlotState nextState;
    fillSlotState(nextState, nextType, current.bypass, current.mix);
    const auto address = reinterpret_cast<std::uintptr_t>(effect.get());
    if (!audio.pushCommand(core::swapEffect(static_cast<std::uint8_t>(state.fxSlot), address))) {
        std::printf("\nFX queue full; type change dropped\n");
        std::fflush(stdout);
        return;
    }
    (void)effect.release();  // the chain now owns it until it retires the old one
    current = nextState;
    state.fxTarget = 0;
    printFxLine(state);
    printChainLine(state, processor);
}

void collectRetired(synth::SynthProcessor& processor) {
    fx::IEffect* retired = nullptr;
    while (processor.takeRetiredEffect(retired)) {
        delete retired;
    }
}

void printStatus(const engine::AudioEngine& audio, synth::SynthProcessor& synthProcessor,
                 const ConsoleState& state) {
    const engine::AudioGraph::Stats stats = audio.stats();
    const unsigned long long xruns = stats.lateBlocks + stats.overdueBlocks;
    const std::string_view name = patchName(state);
    std::printf("\r%.*s | cutoff %5.0f Hz | res %.2f | rel %5.0f ms | voices %2u | "
                "fx %.2f ms | blocks %llu | xruns %llu | max cb %6.3f ms   ",
                static_cast<int>(name.size()), name.data(),
                static_cast<double>(state.cutoffHz), static_cast<double>(state.resonance),
                static_cast<double>(state.releaseMs), synthProcessor.activeVoices(),
                synthProcessor.latencyMs(), static_cast<unsigned long long>(stats.blocks),
                xruns, stats.maxCallbackMs);
    std::fflush(stdout);
}

void printSummary(const engine::AudioEngine& audio, const synth::SynthProcessor& processor) {
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
    std::printf("Instrument latency: %.2f ms (%d samples)\n", processor.latencyMs(),
                processor.latencySamples());
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

    std::printf("drumaspro_console -- engine diagnostics\n\n");

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
    consoleState.spec = audio.graph().spec();
    const fx::ChainDescription defaults = fx::defaultChainDescription();
    for (std::size_t index = 0; index < consoleState.slots.size(); ++index) {
        fillSlotState(consoleState.slots[index], defaults.slots[index].effectType,
                      defaults.slots[index].bypass, defaults.slots[index].mix);
    }

    std::printf("\nA S D F G H J = C D E F G A B.  F1/F2/F3 = patch.  "
                "1/2/3 = cutoff/resonance/release.  Up/Down or +/- = adjust.  Esc = quit.\n");
    std::printf("Effects: Q/W/E/R/T/Y = slot, B = bypass, N = next type, "
                "1/2/3 = parameter, 4 = mix, Tab = synth/effects focus.\n");
    printPatchLine(consoleState);
    printChainLine(consoleState, synthProcessor);
    std::printf("\n");

    std::array<Clock::time_point, core::kMidiNoteMax + 1> gateUntil{};
    std::array<bool, core::kMidiNoteMax + 1> gatePending{};

    bool running = true;
    Clock::time_point lastStatus = Clock::now();

    while (running) {
        collectRetired(synthProcessor);

        while (_kbhit() != 0) {
            int key = _getch();
            if (key == 0 || key == 0xE0) {
                // Extended key: the second code selects the key.
                const int scan = _getch();
                if (key == 0 && scan >= 0x3B && scan <= 0x3D) {
                    selectPatch(audio, consoleState, scan - 0x3B);
                } else if (key == 0xE0 && (scan == 0x48 || scan == 0x50)) {
                    adjustSelected(audio, consoleState, scan == 0x48 ? 1 : -1);
                }
                continue;
            }
            if (key == kEscapeKey) {
                running = false;
                break;
            }
            if (key == 9) {  // Tab
                consoleState.focus = consoleState.focus == Focus::Synth ? Focus::Effects
                                                                        : Focus::Synth;
                std::printf("\nFocus: %s\n",
                            consoleState.focus == Focus::Synth ? "synth" : "effects");
                if (consoleState.focus == Focus::Effects) printFxLine(consoleState);
                std::fflush(stdout);
                continue;
            }
            if (key >= 'a' && key <= 'z') {
                key -= 'a' - 'A';
            }
            if (key == '0') {
                consoleState.focus = Focus::Synth;
                std::printf("\nFocus: synth\n");
                std::fflush(stdout);
                continue;
            }
            if (key == 'B') {
                consoleState.focus = Focus::Effects;
                FxSlotState& slot = consoleState.slots[consoleState.fxSlot];
                if (audio.pushCommand(core::setEffectBypass(
                        static_cast<std::uint8_t>(consoleState.fxSlot), !slot.bypass))) {
                    slot.bypass = !slot.bypass;
                }
                printFxLine(consoleState);
                printChainLine(consoleState, synthProcessor);
                continue;
            }
            if (key == 'N') {
                consoleState.focus = Focus::Effects;
                cycleEffectType(audio, consoleState, synthProcessor);
                continue;
            }
            const int slot = slotForKey(key);
            if (slot >= 0) {
                selectFxSlot(consoleState, slot);
                continue;
            }
            if (key >= '1' && key <= '4') {
                if (consoleState.focus == Focus::Effects) {
                    consoleState.fxTarget = key - '1';
                    printFxLine(consoleState);
                } else if (key <= '3') {
                    consoleState.target = static_cast<EditTarget>(key - '1');
                    const char* names[] = {"cutoff", "resonance", "release"};
                    std::printf("\nEditing %s (Up/Down or +/-)\n",
                                names[static_cast<int>(consoleState.target)]);
                    std::fflush(stdout);
                }
                continue;
            }
            if (key == '+' || key == '=') {
                adjustSelected(audio, consoleState, 1);
                continue;
            }
            if (key == '-' || key == '_') {
                adjustSelected(audio, consoleState, -1);
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
    collectRetired(synthProcessor);
    printSummary(audio, synthProcessor);
    audio.shutdown();
    return 0;
}
