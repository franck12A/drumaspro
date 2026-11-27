#include "EngineBridge.h"

#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QtCore/qnamespace.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <span>

#include "ValueFormat.h"
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
    QString settingsError;
    settings_ = SettingsStore::load(SettingsStore::path(), &settingsError);
    if (!settingsError.isEmpty()) {
        setError(QStringLiteral("Settings: ") + settingsError);
    }
    applyChordSettings();
    pianoOctave_ = settings_.defaultOctave;

    chainModel_.setDescription(chainMirror_.snapshot());
    refreshEffectParameterModel();

    // Populate the mirror before the audio device starts so the offline UI
    // shows the values the default patch will use instead of zeros.
    core::ParameterRegistry defaults;
    (void)synth::registerSynthParameters(defaults);
    parameterMirror_.initialize(defaults);
    applyPatchToMirror(synth::kDefaultPatchIndex);
    editingPatch_ = synth::kFactoryPatches[synth::kDefaultPatchIndex];
    editingName_ = QString::fromUtf8(editingPatch_.name.data(),
                                     static_cast<qsizetype>(editingPatch_.name.size()));
    editingCategory_ = QString::fromUtf8(
        editingPatch_.category.data(), static_cast<qsizetype>(editingPatch_.category.size()));

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
        // Restore the last patch if it is still a factory patch; otherwise the
        // engine keeps its own default. The mirror must show what will sound.
        int initialPatch = synth::kDefaultPatchIndex;
        if (settings_.lastPatch >= 0 && settings_.lastPatch < synth::kFactoryPatchCount) {
            initialPatch = settings_.lastPatch;
            (void)commandSink_->push(
                core::selectPatch(static_cast<std::uint8_t>(initialPatch)));
        }
        applyPatchToMirror(initialPatch);
        editingPatch_ = synth::kFactoryPatches[static_cast<std::size_t>(initialPatch)];
        editingName_ = QString::fromUtf8(editingPatch_.name.data(),
                                         static_cast<qsizetype>(editingPatch_.name.size()));
        editingCategory_ = QString::fromUtf8(
            editingPatch_.category.data(),
            static_cast<qsizetype>(editingPatch_.category.size()));
        userPatchActive_ = false;
        Q_EMIT patchChanged();
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
        noteRefCounts_.fill(0);
        if (latchedRow_ >= 0) {
            latchedRow_ = -1;
            Q_EMIT latchChanged();
        }
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
    editingPatch_ = synth::kFactoryPatches[static_cast<std::size_t>(index)];
    editingName_ = QString::fromUtf8(editingPatch_.name.data(),
                                     static_cast<qsizetype>(editingPatch_.name.size()));
    editingCategory_ = QString::fromUtf8(
        editingPatch_.category.data(), static_cast<qsizetype>(editingPatch_.category.size()));
    userPatchActive_ = false;
    Q_EMIT patchChanged();
    settings_.lastPatch = index;
    persistSettings();
    return true;
}

int EngineBridge::patchCount() const { return synth::kFactoryPatchCount; }

QVariantMap EngineBridge::patchInfo(int index) const {
    if (index < 0 || index >= synth::kFactoryPatchCount) {
        return {};
    }
    const synth::Patch& patch = synth::kFactoryPatches[static_cast<std::size_t>(index)];
    return {{QStringLiteral("index"), index},
            {QStringLiteral("name"),
             QString::fromUtf8(patch.name.data(), static_cast<qsizetype>(patch.name.size()))},
            {QStringLiteral("category"),
             QString::fromUtf8(patch.category.data(),
                               static_cast<qsizetype>(patch.category.size()))}};
}

bool EngineBridge::loadUserPatch(const QString& filePath) {
    if (!initialized_) {
        return false;
    }
    QString error;
    const std::optional<PatchDocument> document = PatchFile::load(filePath, &error);
    if (!document.has_value()) {
        setError(error);
        return false;
    }
    auto* copy = new synth::Patch(document->patch);
    if (!commandSink_->push(core::selectPatchData(
            reinterpret_cast<std::uintptr_t>(copy)))) {
        delete copy;
        setError(QStringLiteral("patch command queue is full"));
        return false;
    }
    applyPatchToMirror(document->patch);
    editingPatch_ = document->patch;
    editingName_ = document->name;
    editingCategory_ = document->category;
    userPatchActive_ = true;
    setError({});
    Q_EMIT patchChanged();
    return true;
}

bool EngineBridge::saveCurrentPatchToFile(const QString& filePath) {
    QString path = filePath;
    if (path.isEmpty()) {
        path = QDir(userPatchDirectory()).filePath(patchFileName(editingName_));
    }
    QString error;
    const PatchDocument document{editingName_, editingCategory_, editingPatch_};
    if (!PatchFile::save(document, path, &error)) {
        setError(error);
        return false;
    }
    setError({});
    return true;
}

QString EngineBridge::userPatchDirectory() const { return PatchFile::directory(); }

QStringList EngineBridge::userPatchFiles() const { return PatchFile::listFiles(); }

bool EngineBridge::deleteUserPatch(const QString& filePath) {
    if (filePath.isEmpty() || !QFile::remove(filePath)) {
        setError(QStringLiteral("patch file cannot be deleted"));
        return false;
    }
    setError({});
    return true;
}

bool EngineBridge::duplicateUserPatch(const QString& filePath) {
    QString error;
    const std::optional<PatchDocument> document = PatchFile::load(filePath, &error);
    if (!document.has_value()) {
        setError(error);
        return false;
    }
    PatchDocument copy = *document;
    copy.name = copy.name + QStringLiteral(" Copy");
    const QString path = QDir(userPatchDirectory()).filePath(patchFileName(copy.name));
    if (!PatchFile::save(copy, path, &error)) {
        setError(error);
        return false;
    }
    setError({});
    return true;
}

QString EngineBridge::patchFileName(const QString& name) const {
    return PatchFile::fileNameFor(name);
}

void EngineBridge::applyPatchToMirror(int index) {
    if (index < 0 || index >= synth::kFactoryPatchCount) {
        return;
    }
    applyPatchToMirror(synth::kFactoryPatches[static_cast<std::size_t>(index)]);
}

void EngineBridge::applyPatchToMirror(const synth::Patch& patch) {
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

bool EngineBridge::noteOn(int note, double velocity) {
    if ((!initialized_ && !commandSinkInjected_) || !std::isfinite(velocity) || note < 0 ||
        note > 127) {
        return false;
    }
    const auto midiNote = static_cast<std::uint8_t>(note);
    const float clampedVelocity =
        std::clamp(static_cast<float>(velocity), 0.0F, 1.0F);
    if (noteRefCounts_[midiNote] == 0 &&
        !commandSink_->push(core::noteOn(midiNote, clampedVelocity))) {
        return false;
    }
    ++noteRefCounts_[midiNote];
    return true;
}

bool EngineBridge::noteOff(int note) {
    if ((!initialized_ && !commandSinkInjected_) || note < 0 || note > 127) {
        return false;
    }
    const auto midiNote = static_cast<std::uint8_t>(note);
    if (noteRefCounts_[midiNote] == 0) {
        return true;
    }
    --noteRefCounts_[midiNote];
    if (noteRefCounts_[midiNote] == 0 && !commandSink_->push(core::noteOff(midiNote))) {
        ++noteRefCounts_[midiNote];
        return false;
    }
    return true;
}

void EngineBridge::setPianoOctave(int octave) {
    const int clamped = std::clamp(octave, 0, 8);
    if (clamped == pianoOctave_) {
        return;
    }
    pianoOctave_ = clamped;
    Q_EMIT pianoOctaveChanged();
}

bool EngineBridge::shiftPianoOctave(int delta) {
    const int next = std::clamp(pianoOctave_ + delta, 0, 8);
    if (next == pianoOctave_) {
        return false;
    }
    pianoOctave_ = next;
    Q_EMIT pianoOctaveChanged();
    return true;
}

int EngineBridge::semitoneForKey(int key) const {
    switch (key) {
        case Qt::Key_A: return 0;
        case Qt::Key_W: return 1;
        case Qt::Key_S: return 2;
        case Qt::Key_E: return 3;
        case Qt::Key_D: return 4;
        case Qt::Key_F: return 5;
        case Qt::Key_T: return 6;
        case Qt::Key_G: return 7;
        case Qt::Key_Y: return 8;
        case Qt::Key_H: return 9;
        case Qt::Key_U: return 10;
        case Qt::Key_J: return 11;
        case Qt::Key_K: return 12;
        default: return -1;
    }
}

int EngineBridge::midiNoteForSemitone(int semitone) const {
    const int note = (pianoOctave_ + 1) * 12 + std::clamp(semitone, 0, 12);
    return std::clamp(note, 0, 127);
}

double EngineBridge::applyVelocityCurve(double velocity) const {
    const double clamped = std::clamp(velocity, 0.0, 1.0);
    switch (settings_.velocityCurve) {
        case 1: return std::sqrt(clamped);
        case 2: return clamped * clamped;
        default: return clamped;
    }
}

QStringList EngineBridge::chordQualityNames() const {
    QStringList names;
    for (int quality = 0; quality < kChordQualityCount; ++quality) {
        const std::string_view label =
            music::intervals(static_cast<music::ChordQuality>(quality)).label;
        names.append(QString::fromUtf8(label.data(), static_cast<qsizetype>(label.size())));
    }
    return names;
}

QStringList EngineBridge::noteNames() const {
    QStringList names;
    constexpr std::array<music::PitchClass, 12> pitchClasses{
        music::PitchClass::C, music::PitchClass::CSharp, music::PitchClass::D,
        music::PitchClass::DSharp, music::PitchClass::E, music::PitchClass::F,
        music::PitchClass::FSharp, music::PitchClass::G, music::PitchClass::GSharp,
        music::PitchClass::A, music::PitchClass::ASharp, music::PitchClass::B};
    for (const music::PitchClass pitchClass : pitchClasses) {
        const std::string_view pitchName = music::name(pitchClass);
        names.append(
            QString::fromUtf8(pitchName.data(), static_cast<qsizetype>(pitchName.size())));
    }
    return names;
}

QVariantMap EngineBridge::chordSlot(int row) const {
    QVariantMap map;
    if (row < 0 || row >= kChordSlotCount) {
        return map;
    }
    const ChordSlotSetting& slot = settings_.chordSlots[static_cast<std::size_t>(row)];
    map.insert(QStringLiteral("manual"), slot.manual);
    map.insert(QStringLiteral("root"), slot.root);
    map.insert(QStringLiteral("quality"), slot.quality);
    map.insert(QStringLiteral("inversion"), slot.inversion);
    map.insert(QStringLiteral("octave"), slot.octave);
    return map;
}

bool EngineBridge::setChordSlot(int row, bool manual, int root, int quality,
                                int inversion, int octave) {
    if (row < 0 || row >= kChordSlotCount) {
        return false;
    }
    root = std::clamp(root, 0, 11);
    quality = std::clamp(quality, 0, kChordQualityCount - 1);
    inversion = std::clamp(inversion, 0, 3);
    octave = std::clamp(octave, 0, 8);

    constexpr std::array<music::PitchClass, 12> pitchClasses{
        music::PitchClass::C, music::PitchClass::CSharp, music::PitchClass::D,
        music::PitchClass::DSharp, music::PitchClass::E, music::PitchClass::F,
        music::PitchClass::FSharp, music::PitchClass::G, music::PitchClass::GSharp,
        music::PitchClass::A, music::PitchClass::ASharp, music::PitchClass::B};
    const music::Chord chord{pitchClasses[static_cast<std::size_t>(root)],
                             static_cast<music::ChordQuality>(quality),
                             static_cast<std::uint8_t>(inversion), octave};

    if (manual && !chordModel_.setManualChord(row, chord)) {
        setError(QStringLiteral("Chord voicing is outside MIDI 0..127"));
        return false;
    }
    if (!manual && !chordModel_.setManualChord(row, std::nullopt)) {
        return false;
    }

    ChordSlotSetting& slot = settings_.chordSlots[static_cast<std::size_t>(row)];
    slot.manual = manual;
    slot.root = root;
    slot.quality = quality;
    slot.inversion = inversion;
    slot.octave = octave;
    setError({});
    persistSettings();
    Q_EMIT settingsChanged();
    return true;
}

bool EngineBridge::resetChordSlot(int row) {
    if (row < 0 || row >= kChordSlotCount) {
        return false;
    }
    if (!chordModel_.setManualChord(row, std::nullopt)) {
        return false;
    }
    settings_.chordSlots[static_cast<std::size_t>(row)].manual = false;
    persistSettings();
    Q_EMIT settingsChanged();
    return true;
}

bool EngineBridge::saveSettings() {
    QString error;
    if (!SettingsStore::save(settings_, SettingsStore::path(), &error)) {
        setError(QStringLiteral("Settings: ") + error);
        return false;
    }
    setError({});
    return true;
}

QString EngineBridge::settingsPath() const { return SettingsStore::path(); }

void EngineBridge::applyChordSettings() {
    (void)chordModel_.setScale(settings_.lastKey, settings_.lastScale == 1);
    (void)chordModel_.setSevenths(settings_.sevenths);
    constexpr std::array<music::PitchClass, 12> pitchClasses{
        music::PitchClass::C, music::PitchClass::CSharp, music::PitchClass::D,
        music::PitchClass::DSharp, music::PitchClass::E, music::PitchClass::F,
        music::PitchClass::FSharp, music::PitchClass::G, music::PitchClass::GSharp,
        music::PitchClass::A, music::PitchClass::ASharp, music::PitchClass::B};
    for (int row = 0; row < 7; ++row) {
        const ChordSlotSetting& slot = settings_.chordSlots[static_cast<std::size_t>(row)];
        if (!slot.manual) {
            (void)chordModel_.setManualChord(row, std::nullopt);
            continue;
        }
        const music::Chord chord{pitchClasses[static_cast<std::size_t>(slot.root)],
                                 static_cast<music::ChordQuality>(slot.quality),
                                 static_cast<std::uint8_t>(slot.inversion), slot.octave};
        if (!chordModel_.setManualChord(row, chord)) {
            (void)chordModel_.setManualChord(row, std::nullopt);
            settings_.chordSlots[static_cast<std::size_t>(row)].manual = false;
        }
    }
}

void EngineBridge::persistSettings() { (void)saveSettings(); }

void EngineBridge::setThemeName(const QString& theme) {
    if (theme != QLatin1String("default") && theme != QLatin1String("amber") &&
        theme != QLatin1String("night")) {
        return;
    }
    if (settings_.theme == theme) {
        return;
    }
    settings_.theme = theme;
    persistSettings();
    Q_EMIT settingsChanged();
}

void EngineBridge::setAccentPreset(int preset) {
    const int clamped = std::clamp(preset, 0, kAccentPresetCount - 1);
    if (clamped == settings_.accentPreset) {
        return;
    }
    settings_.accentPreset = clamped;
    persistSettings();
    Q_EMIT settingsChanged();
}

void EngineBridge::setUiScale(int scale) {
    if (scale != 90 && scale != 100 && scale != 110 && scale != 125) {
        return;
    }
    if (settings_.uiScale == scale) {
        return;
    }
    settings_.uiScale = scale;
    persistSettings();
    Q_EMIT settingsChanged();
}

void EngineBridge::setKnobSensitivity(int sensitivity) {
    const int clamped = std::clamp(sensitivity, 50, 150);
    if (clamped == settings_.knobSensitivity) {
        return;
    }
    settings_.knobSensitivity = clamped;
    persistSettings();
    Q_EMIT settingsChanged();
}

void EngineBridge::setVelocityCurve(int curve) {
    const int clamped = std::clamp(curve, 0, kVelocityCurveCount - 1);
    if (clamped == settings_.velocityCurve) {
        return;
    }
    settings_.velocityCurve = clamped;
    persistSettings();
    Q_EMIT settingsChanged();
}

void EngineBridge::setDefaultOctave(int octave) {
    const int clamped = std::clamp(octave, 0, 8);
    if (clamped == settings_.defaultOctave) {
        return;
    }
    settings_.defaultOctave = clamped;
    setPianoOctave(clamped);
    persistSettings();
    Q_EMIT settingsChanged();
}

void EngineBridge::setSevenths(bool sevenths) {
    if (settings_.sevenths == sevenths) {
        return;
    }
    settings_.sevenths = sevenths;
    (void)chordModel_.setSevenths(sevenths);
    persistSettings();
    Q_EMIT settingsChanged();
}

void EngineBridge::setLatchEnabled(bool enabled) {
    if (settings_.latch == enabled) {
        return;
    }
    settings_.latch = enabled;
    if (!enabled && latchedRow_ >= 0) {
        const int previous = latchedRow_;
        latchedRow_ = -1;
        (void)releaseChordInternal(previous);
        Q_EMIT latchChanged();
    }
    persistSettings();
    Q_EMIT settingsChanged();
}

QString EngineBridge::formatParameterValue(qulonglong id, double value) const {
    const bridge::ParameterSnapshot snapshot = parameterMirror_.snapshot();
    for (std::size_t index = 0; index < snapshot.count; ++index) {
        const core::ParameterDescriptor& descriptor = snapshot.values[index].descriptor;
        if (static_cast<qulonglong>(descriptor.id) == id) {
            return formatParameter(descriptor.unit, value);
        }
    }
    return formatParameter(core::ParameterUnit::None, value);
}

QString EngineBridge::formatUnitValue(const QString& unit, double value) const {
    return formatParameterByUnitName(unit, value);
}

bool EngineBridge::setChordScale(int tonicSemitones, bool naturalMinor) {
    if (!chordModel_.setScale(tonicSemitones, naturalMinor)) {
        return false;
    }
    settings_.lastKey = tonicSemitones;
    settings_.lastScale = naturalMinor ? 1 : 0;
    persistSettings();
    return true;
}

bool EngineBridge::playChord(int row, double velocity) {
    if ((!initialized_ && !commandSinkInjected_) || !std::isfinite(velocity) || row < 0 ||
        row >= static_cast<int>(chordHeld_.size())) {
        return false;
    }
    // Latch: a new chord replaces the held one before its notes are sent, so
    // the audio thread always sees at most one latched chord.
    if (settings_.latch) {
        if (latchedRow_ == row) {
            return true;
        }
        if (latchedRow_ >= 0) {
            const int previous = latchedRow_;
            latchedRow_ = -1;
            (void)releaseChordInternal(previous);
            Q_EMIT latchChanged();
        }
    }

    music::Chord chord;
    if (!chordModel_.chordAt(row, chord) || chordHeld_[static_cast<std::size_t>(row)]) {
        return true;
    }

    // One ChordOn: the engine allocates every voice before starting any note,
    // and a note already held by another chord is joined, not retriggered.
    const auto group = static_cast<std::uint8_t>(row + 1);
    if (!chordExpander_.noteOn(*commandSink_, chord, static_cast<float>(velocity), group)) {
        return false;
    }

    const auto slot = static_cast<std::size_t>(row);
    chordHeld_[slot] = true;
    if (settings_.latch) {
        latchedRow_ = row;
        Q_EMIT latchChanged();
    }
    return true;
}

bool EngineBridge::releaseChord(int row) {
    if (settings_.latch && row == latchedRow_) {
        // The pad keeps sounding until another chord is played or latch is off.
        return true;
    }
    const bool released = releaseChordInternal(row);
    if (released && row == latchedRow_) {
        latchedRow_ = -1;
        Q_EMIT latchChanged();
    }
    return released;
}

bool EngineBridge::releaseChordInternal(int row) {
    if ((!initialized_ && !commandSinkInjected_) || row < 0 ||
        row >= static_cast<int>(chordHeld_.size())) {
        return false;
    }
    const auto slot = static_cast<std::size_t>(row);
    if (!chordHeld_[slot]) {
        return true;
    }

    // ChordOff only drops this pad's claim: notes still held by another chord
    // keep sounding, and the audio thread decides which voices release.
    if (!chordExpander_.noteOff(*commandSink_, static_cast<std::uint8_t>(row + 1))) {
        return false;
    }
    chordHeld_[slot] = false;
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
    if (index < 0 || index >= static_cast<int>(fx::kEffectTypeCycle.size())) {
        return fx::kNoEffectType;
    }
    return static_cast<qulonglong>(fx::kEffectTypeCycle[static_cast<std::size_t>(index)]);
}

QStringList EngineBridge::effectTypeNames() const {
    QStringList names;
    names.reserve(static_cast<qsizetype>(fx::kEffectTypeCycle.size()));
    for (const fx::EffectTypeId type : fx::kEffectTypeCycle) {
        const std::string_view name = fx::effectTypeName(type);
        names.append(QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
    }
    return names;
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
        const synth::Patch* retiredPatch = nullptr;
        while (synth_.takeRetiredPatch(retiredPatch)) {
            delete retiredPatch;
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
