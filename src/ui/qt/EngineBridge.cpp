#include "EngineBridge.h"

#include <QMetaObject>

#include <array>
#include <cmath>
#include <memory>

#include "core/AudioTypes.h"
#include "fx/EffectFactory.h"

namespace drumas::ui {

EngineBridge::EngineBridge(QObject* parent)
    : QObject(parent),
      commandSink_(engine_),
      parameterModel_(this),
      patchModel_(this),
      chainModel_(this),
      chordModel_(this) {
    chainModel_.setDescription(chainMirror_.snapshot());
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
        parameterModel_.setSnapshot(parameterMirror_.snapshot());
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

bool EngineBridge::setParameter(quint32 id, double value) {
    if (!initialized_ || !std::isfinite(value)) {
        return false;
    }
    const auto parameterId = static_cast<core::ParameterId>(id);
    const float realValue = static_cast<float>(value);
    if (!commandSink_.push(core::setParam(parameterId, realValue)) ||
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
    return commandSink_.push(core::selectPatch(static_cast<std::uint8_t>(index)));
}

bool EngineBridge::setEffectType(int slot, quint32 type) {
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
    if (!commandSink_.push(command)) {
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
        !commandSink_.push(core::setEffectBypass(static_cast<std::uint8_t>(slot), bypassed))) {
        return false;
    }
    (void)chainMirror_.setBypass(static_cast<std::size_t>(slot), bypassed);
    chainModel_.setDescription(chainMirror_.snapshot());
    return true;
}

bool EngineBridge::setEffectMix(int slot, double mix) {
    if (!initialized_ || slot < 0 ||
        slot >= static_cast<int>(fx::kEffectSlotCount) || !std::isfinite(mix) ||
        !commandSink_.push(
            core::setEffectMix(static_cast<std::uint8_t>(slot), static_cast<float>(mix)))) {
        return false;
    }
    (void)chainMirror_.setMix(static_cast<std::size_t>(slot), static_cast<float>(mix));
    chainModel_.setDescription(chainMirror_.snapshot());
    return true;
}

bool EngineBridge::setEffectParameter(int slot, quint32 id, double value) {
    if (!initialized_ || slot < 0 ||
        slot >= static_cast<int>(fx::kEffectSlotCount) || !std::isfinite(value)) {
        return false;
    }
    const auto parameterId = static_cast<core::ParameterId>(id);
    const float realValue = static_cast<float>(value);
    if (!commandSink_.push(core::setEffectParam(static_cast<std::uint8_t>(slot),
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
    if (!initialized_ || !std::isfinite(velocity)) {
        return false;
    }
    music::Chord chord;
    return chordModel_.chordAt(row, chord) &&
           chordExpander_.noteOn(commandSink_, chord, static_cast<float>(velocity));
}

bool EngineBridge::releaseChord(int row) {
    if (!initialized_) {
        return false;
    }
    music::Chord chord;
    return chordModel_.chordAt(row, chord) && chordExpander_.noteOff(commandSink_, chord);
}

quint32 EngineBridge::parameterId(const QString& name) const {
    const bridge::ParameterSnapshot snapshot = parameterMirror_.snapshot();
    for (std::size_t index = 0; index < snapshot.count; ++index) {
        const auto& descriptor = snapshot.values[index].descriptor;
        const QString candidate =
     QString::fromUtf8(descriptor.name.data(),
                       static_cast<qsizetype>(descriptor.name.size()));
        if (candidate == name) {
     return static_cast<quint32>(descriptor.id);
        }
    }
    return core::kInvalidParameterId;
}

quint32 EngineBridge::effectTypeId(int index) const noexcept {
    constexpr std::array<fx::EffectTypeId, 5> types{
        fx::kReverbType, fx::kDelayType, fx::kChorusType, fx::kDriveType, fx::kFilterType};
    if (index < 0 || index >= static_cast<int>(types.size())) {
        return fx::kNoEffectType;
    }
    return types[static_cast<std::size_t>(index)];
}

int EngineBridge::activeVoices() const noexcept {
    return static_cast<int>(synth_.activeVoices());
}

void EngineBridge::refreshStatus() {
    const engine::AudioGraph::Stats stats = engine_.stats();
    xruns_.store(stats.lateBlocks + stats.overdueBlocks, std::memory_order_relaxed);
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
