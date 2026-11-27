#include "EngineBridge.h"

#include <QMetaObject>

#include <array>
#include <cmath>
#include <memory>
#include <span>

#include "core/AudioTypes.h"
#include "fx/EffectFactory.h"
#include "music/Chord.h"
#include "synth/ParameterIds.h"

namespace drumas::ui {

EngineBridge::EngineBridge(QObject* parent, bridge::CommandSink* commandSink)
    : QObject(parent),
      audioCommandSink_(engine_),
      commandSink_(commandSink != nullptr ? commandSink : &audioCommandSink_),
      commandSinkInjected_(commandSink != nullptr),
      parameterModel_(this),
      patchModel_(this),
      chainModel_(this),
      chordModel_(this),
      effectParameterModel_(this) {
    chainModel_.setDescription(chainMirror_.snapshot());
    refreshEffectParameterModel();

    // Populate the mirror before the audio device starts so the offline UI
    // shows the values the default patch will use instead of zeros.
    core::ParameterRegistry defaults;
    (void)synth::registerSynthParameters(defaults);
    parameterMirror_.initialize(defaults);
    applyPatchToMirror(synth::kDefaultPatchIndex);

    timer_.setInterval(25);
    connect(&timer_, &QTimer::timeout, this, &EngineBridge::refreshStatus);
    timer_.start();
}

EngineBridge::~EngineBridge() { shutdown(); }

bool EngineBridge::start() {
    if (!initialized_) {
        const engine::AudioEngine::Config config;
        if (!engine_.initialize(config, synth_)) {
            setError(QString::fromUtf8(engine_.lastError().data(),
                                       static_cast<qsizetype>(engine_.lastError().size())));
            return false;
        }
        initialized_ = true;
        parameterMirror_.initialize(synth_.parameters());
        // The engine starts on the default patch, not on the registry defaults:
        // the UI mirror has to show the values the next note will actually use.
        applyPatchToMirror(synth::kDefaultPatchIndex);
    }
    if (!engine_.start()) {
        setError(QString::fromUtf8(engine_.lastError().data(),
                                   static_cast<qsizetype>(engine_.lastError().size())));
        return false;
    }
    setError({});
    Q_EMIT statusChanged();
    return true;
}

void EngineBridge::shutdown() {
    if (initialized_ || engine_.isRunning()) {
        engine_.shutdown();
        initialized_ = false;
        // The engine reset its voices: forget which chords were held so a
        // restart does not leave the note reference counts stale.
        chordHeld_.fill(false);
        chordNoteCounts_.fill(0);
        noteRefCounts_.fill(0);
        Q_EMIT statusChanged();
    }
}

bool EngineBridge::setParameter(qulonglong id, double value) {
    if (!initialized_ || !std::isfinite(value)) {
        return false;
    }
    const auto parameterId = static_cast<core::ParameterId>(id);
    const float realValue = static_cast<float>(value);
    if (!commandSink_->push(core::setParam(parameterId, realValue)) ||
        !parameterMirror_.set(parameterId, realValue)) {
        return false;
    }
    float clamped = 0.0F;
    if (parameterMirror_.get(parameterId, clamped)) {
        parameterModel_.setValue(parameterId, clamped);
    }
    return true;
}

bool EngineBridge::selectPatch(int index) {
    if (!initialized_ || index < 0 || index >= synth::kFactoryPatchCount) {
        return false;
    }
    if (!commandSink_->push(core::selectPatch(static_cast<std::uint8_t>(index)))) {
        return false;
    }
    applyPatchToMirror(index);
    return true;
}

void EngineBridge::applyPatchToMirror(int index) {
    if (index < 0 || index >= synth::kFactoryPatchCount) {
        return;
    }
    const synth::Patch& patch = synth::kFactoryPatches[static_cast<std::size_t>(index)];
    const auto set = [this](core::ParameterId id, float value) {
        (void)parameterMirror_.set(id, value);
    };

    set(synth::kOsc1Waveform,
        static_cast<float>(static_cast<int>(patch.osc1Waveform)));
    set(synth::kOsc1Level, patch.osc1Level);
    set(synth::kOsc1DetuneCents, patch.osc1DetuneCents);
    set(synth::kOsc1PulseWidth, patch.osc1PulseWidth);
    set(synth::kOsc2Waveform,
        static_cast<float>(static_cast<int>(patch.osc2Waveform)));
    set(synth::kOsc2Level, patch.osc2Level);
    set(synth::kOsc2DetuneCents, patch.osc2DetuneCents);
    set(synth::kOsc2PulseWidth, patch.osc2PulseWidth);
    set(synth::kSubLevel, patch.subLevel);
    set(synth::kNoiseLevel, patch.noiseLevel);

    set(synth::kUnisonVoices, static_cast<float>(patch.unisonVoices));
    set(synth::kUnisonDetuneCents, patch.unisonDetuneCents);
    set(synth::kUnisonStereo, patch.unisonStereo);

    set(synth::kFilterCutoff, patch.cutoffHz);
    set(synth::kFilterResonance, patch.resonance);
    set(synth::kFilterEnvOctaves, patch.filterEnvOctaves);
    set(synth::kFilterAttackMs, patch.filterEnvelope.attackMs);
    set(synth::kFilterDecayMs, patch.filterEnvelope.decayMs);
    set(synth::kFilterSustain, patch.filterEnvelope.sustain);
    set(synth::kFilterReleaseMs, patch.filterEnvelope.releaseMs);

    set(synth::kAmpAttackMs, patch.ampEnvelope.attackMs);
    set(synth::kAmpDecayMs, patch.ampEnvelope.decayMs);
    set(synth::kAmpSustain, patch.ampEnvelope.sustain);
    set(synth::kAmpReleaseMs, patch.ampEnvelope.releaseMs);

    set(synth::kVelocityVolume, patch.velocityVolume);
    set(synth::kVelocityBrightness, patch.velocityBrightness);

    set(synth::kLfoShape, static_cast<float>(static_cast<int>(patch.lfoShape)));
    set(synth::kLfoRate, patch.lfoRateHz);
    set(synth::kLfoCutoffDepth, patch.lfoCutoffDepth);
    set(synth::kLfoPitchDepthCents, patch.lfoPitchDepthCents);

    parameterModel_.setSnapshot(parameterMirror_.snapshot());
}

bool EngineBridge::setEffectType(int slot, qulonglong type) {
    if (!initialized_ || slot < 0 ||
        slot >= static_cast<int>(fx::kEffectSlotCount)) {
        return false;
    }
    const auto effectType = static_cast<fx::EffectTypeId>(type);
    auto effect = fx::createPreparedEffect(effectType, engine_.graph().spec());
    if (effect == nullptr) {
        setError(QStringLiteral("Unknown or unavailable effect type"));
        return false;
    }

    // Copy the defaults while the effect is still ours: after the swap the
    // audio thread owns it and must not be read from here.
    std::array<core::ParameterDescriptor, fx::kMaxParametersPerEffect> defaults{};
    std::size_t defaultCount = 0;
    const core::ParameterRegistry& registry = effect->parameters();
    for (std::size_t index = 0;
         index < registry.size() && defaultCount < defaults.size(); ++index) {
        defaults[defaultCount++] = registry.at(index);
    }

    const auto command = core::swapEffect(
        static_cast<std::uint8_t>(slot),
        reinterpret_cast<std::uintptr_t>(effect.get()));
    if (!commandSink_->push(command)) {
        return false;
    }
    effect.release();
    (void)chainMirror_.setEffectType(static_cast<std::size_t>(slot), effectType);
    for (std::size_t index = 0; index < defaultCount; ++index) {
        (void)chainMirror_.setParameter(static_cast<std::size_t>(slot), defaults[index].id,
                                        defaults[index].defaultValue);
    }
    chainModel_.setDescription(chainMirror_.snapshot());
    if (slot == selectedEffectSlot_) {
        refreshEffectParameterModel();
        Q_EMIT selectedEffectSlotChanged();
    }
    return true;
}

bool EngineBridge::setEffectBypass(int slot, bool bypassed) {
    if (!initialized_ || slot < 0 ||
        slot >= static_cast<int>(fx::kEffectSlotCount) ||
        !commandSink_->push(core::setEffectBypass(static_cast<std::uint8_t>(slot), bypassed))) {
        return false;
    }
    (void)chainMirror_.setBypass(static_cast<std::size_t>(slot), bypassed);
    chainModel_.setDescription(chainMirror_.snapshot());
    return true;
}

bool EngineBridge::setEffectMix(int slot, double mix) {
    if (!initialized_ || slot < 0 ||
        slot >= static_cast<int>(fx::kEffectSlotCount) || !std::isfinite(mix) ||
        !commandSink_->push(
            core::setEffectMix(static_cast<std::uint8_t>(slot), static_cast<float>(mix)))) {
        return false;
    }
    (void)chainMirror_.setMix(static_cast<std::size_t>(slot), static_cast<float>(mix));
    chainModel_.setDescription(chainMirror_.snapshot());
    return true;
}

bool EngineBridge::setEffectParameter(int slot, qulonglong id, double value) {
    if (!initialized_ || slot < 0 ||
        slot >= static_cast<int>(fx::kEffectSlotCount) || !std::isfinite(value)) {
        return false;
    }
    const auto parameterId = static_cast<core::ParameterId>(id);
    const float realValue = static_cast<float>(value);
    if (!commandSink_->push(core::setEffectParam(static_cast<std::uint8_t>(slot),
                                                parameterId, realValue)) ||
        !chainMirror_.setParameter(static_cast<std::size_t>(slot), parameterId, realValue)) {
        return false;
    }
    chainModel_.setDescription(chainMirror_.snapshot());
    if (slot == selectedEffectSlot_) {
        effectParameterModel_.setValue(parameterId, realValue);
    }
    return true;
}

void EngineBridge::selectEffectSlot(int slot) {
    if (slot < 0 || slot >= static_cast<int>(fx::kEffectSlotCount) ||
        slot == selectedEffectSlot_) {
        return;
    }
    selectedEffectSlot_ = slot;
    refreshEffectParameterModel();
    Q_EMIT selectedEffectSlotChanged();
}

QString EngineBridge::selectedEffectTypeName() const {
    const fx::ChainDescription description = chainMirror_.snapshot();
    const auto index = static_cast<std::size_t>(selectedEffectSlot_);
    if (index >= description.slots.size()) {
        return {};
    }
    const std::string_view name = fx::effectTypeName(description.slots[index].effectType);
    return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
}

void EngineBridge::refreshEffectParameterModel() {
    const fx::ChainDescription description = chainMirror_.snapshot();
    const auto index = static_cast<std::size_t>(selectedEffectSlot_);
    if (index >= description.slots.size()) {
        return;
    }
    effectParameterModel_.setEffect(description.slots[index].effectType,
                                    description.slots[index]);
}

bool EngineBridge::setChordScale(int tonicSemitones, bool naturalMinor) {
    return chordModel_.setScale(tonicSemitones, naturalMinor);
}

bool EngineBridge::playChord(int row, double velocity) {
    if ((!initialized_ && !commandSinkInjected_) || !std::isfinite(velocity) || row < 0 ||
        row >= static_cast<int>(chordHeld_.size())) {
        return false;
    }
    music::Chord chord;
    if (!chordModel_.chordAt(row, chord) || chordHeld_[static_cast<std::size_t>(row)]) {
        return true;
    }
    const auto midiNotes = music::chordToMidiNotes(chord);
    if (!midiNotes.has_value() || midiNotes->count > chordNotes_.front().size()) {
        return false;
    }

    // Only send note-on for notes that no other held chord is already playing.
    // Without this, a shared note would retrigger and its release would cut the
    // chord that still needs it.
    std::array<core::Command, 4> commands{};
    std::size_t commandCount = 0;
    for (std::size_t index = 0; index < midiNotes->count; ++index) {
        const std::uint8_t note = midiNotes->notes[index];
        if (noteRefCounts_[note] == 0) {
            commands[commandCount++] = core::noteOn(note, static_cast<float>(velocity));
        }
    }
    if (commandCount > 0 &&
        !commandSink_->pushBatch(
            std::span<const core::Command>(commands.data(), commandCount))) {
        return false;
    }

    const auto slot = static_cast<std::size_t>(row);
    for (std::size_t index = 0; index < midiNotes->count; ++index) {
        const std::uint8_t note = midiNotes->notes[index];
        ++noteRefCounts_[note];
        chordNotes_[slot][index] = note;
    }
    chordNoteCounts_[slot] = midiNotes->count;
    chordHeld_[slot] = true;
    return true;
}

bool EngineBridge::releaseChord(int row) {
    if ((!initialized_ && !commandSinkInjected_) || row < 0 ||
        row >= static_cast<int>(chordHeld_.size())) {
        return false;
    }
    const auto slot = static_cast<std::size_t>(row);
    if (!chordHeld_[slot]) {
        return true;
    }

    std::array<core::Command, 4> commands{};
    std::size_t commandCount = 0;
    for (std::size_t index = 0; index < chordNoteCounts_[slot]; ++index) {
        const std::uint8_t note = chordNotes_[slot][index];
        if (noteRefCounts_[note] == 0) {
            continue;
        }
        --noteRefCounts_[note];
        if (noteRefCounts_[note] == 0) {
            commands[commandCount++] = core::noteOff(note);
        }
    }
    if (commandCount > 0 &&
        !commandSink_->pushBatch(
            std::span<const core::Command>(commands.data(), commandCount))) {
        // Put the references back so a later release still sends the note-off.
        for (std::size_t index = 0; index < chordNoteCounts_[slot]; ++index) {
            ++noteRefCounts_[chordNotes_[slot][index]];
        }
        return false;
    }
    chordHeld_[slot] = false;
    chordNoteCounts_[slot] = 0;
    return true;
}

qulonglong EngineBridge::parameterId(const QString& name) const {
    const bridge::ParameterSnapshot snapshot = parameterMirror_.snapshot();
    for (std::size_t index = 0; index < snapshot.count; ++index) {
        const auto& descriptor = snapshot.values[index].descriptor;
        const QString candidate =
     QString::fromUtf8(descriptor.name.data(),
                       static_cast<qsizetype>(descriptor.name.size()));
        if (candidate == name) {
            return static_cast<qulonglong>(descriptor.id);
        }
    }
    return core::kInvalidParameterId;
}

qulonglong EngineBridge::effectTypeId(int index) const noexcept {
    constexpr std::array<fx::EffectTypeId, 5> types{
        fx::kReverbType, fx::kDelayType, fx::kChorusType, fx::kDriveType, fx::kFilterType};
    if (index < 0 || index >= static_cast<int>(types.size())) {
        return fx::kNoEffectType;
    }
    return static_cast<qulonglong>(types[static_cast<std::size_t>(index)]);
}

int EngineBridge::activeVoices() const noexcept {
    return static_cast<int>(synth_.activeVoices());
}

void EngineBridge::refreshStatus() {
    if (initialized_) {
        fx::IEffect* retired = nullptr;
        while (synth_.takeRetiredEffect(retired)) {
            delete retired;
        }
    }
    const engine::AudioGraph::Stats stats = engine_.stats();
    xruns_.store(stats.lateBlocks + stats.overdueBlocks, std::memory_order_relaxed);
    outputPeak_ = static_cast<double>(stats.outputPeak);
    Q_EMIT statusChanged();
}

void EngineBridge::setError(QString message) {
    if (error_ == message) {
        return;
    }
    error_ = std::move(message);
    Q_EMIT errorChanged();
}

}  // namespace drumas::ui
