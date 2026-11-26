#include "EngineBridge.h"

#include <QMetaObject>

#include <array>
#include <cmath>
#include <memory>

#include "core/AudioTypes.h"
#include "fx/EffectFactory.h"
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
      chordModel_(this) {
    chainModel_.setDescription(chainMirror_.snapshot());

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
    auto effect = fx::createPreparedEffect(
        effectType, core::AudioSpec{core::kDefaultSampleRate, 2048, 2});
    if (effect == nullptr) {
        setError(QStringLiteral("Unknown or unavailable effect type"));
        return false;
    }

    const auto command = core::swapEffect(
        static_cast<std::uint8_t>(slot),
        reinterpret_cast<std::uintptr_t>(effect.get()));
    if (!commandSink_->push(command)) {
        return false;
    }
    effect.release();
    (void)chainMirror_.setEffectType(static_cast<std::size_t>(slot), effectType);
    chainModel_.setDescription(chainMirror_.snapshot());
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
    return true;
}

bool EngineBridge::setChordScale(int tonicSemitones, bool naturalMinor) {
    return chordModel_.setScale(tonicSemitones, naturalMinor);
}

bool EngineBridge::playChord(int row, double velocity) {
    if ((!initialized_ && !commandSinkInjected_) || !std::isfinite(velocity)) {
        return false;
    }
    music::Chord chord;
    return chordModel_.chordAt(row, chord) &&
           chordExpander_.noteOn(*commandSink_, chord, static_cast<float>(velocity));
}

bool EngineBridge::releaseChord(int row) {
    if (!initialized_ && !commandSinkInjected_) {
        return false;
    }
    music::Chord chord;
    return chordModel_.chordAt(row, chord) && chordExpander_.noteOff(*commandSink_, chord);
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
