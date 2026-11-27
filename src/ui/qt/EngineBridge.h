#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <optional>
#include <string>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>

#include "PatchFile.h"
#include "Settings.h"
#include "bridge/ChainMirror.h"
#include "bridge/ChordExpander.h"
#include "bridge/CommandSink.h"
#include "bridge/ParameterMirror.h"
#include "engine/AudioEngine.h"
#include "synth/SynthProcessor.h"
#include "ui/qt/Models.h"

namespace drumas::ui {

class EngineBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(ParameterModel* parameterModel READ parameterModel CONSTANT)
    Q_PROPERTY(PatchModel* patchModel READ patchModel CONSTANT)
    Q_PROPERTY(ChainModel* chainModel READ chainModel CONSTANT)
    Q_PROPERTY(ChordModel* chordModel READ chordModel CONSTANT)
    Q_PROPERTY(EffectParameterModel* effectParameterModel READ effectParameterModel CONSTANT)
    Q_PROPERTY(int selectedEffectSlot READ selectedEffectSlot NOTIFY selectedEffectSlotChanged)
    Q_PROPERTY(QString selectedEffectTypeName READ selectedEffectTypeName NOTIFY
                   selectedEffectSlotChanged)
    Q_PROPERTY(int pianoOctave READ pianoOctave NOTIFY pianoOctaveChanged)
    Q_PROPERTY(QString themeName READ themeName WRITE setThemeName NOTIFY settingsChanged)
    Q_PROPERTY(int accentPreset READ accentPreset WRITE setAccentPreset NOTIFY settingsChanged)
    Q_PROPERTY(int uiScale READ uiScale WRITE setUiScale NOTIFY settingsChanged)
    Q_PROPERTY(int knobSensitivity READ knobSensitivity WRITE setKnobSensitivity NOTIFY
                   settingsChanged)
    Q_PROPERTY(int velocityCurve READ velocityCurve WRITE setVelocityCurve NOTIFY settingsChanged)
    Q_PROPERTY(int defaultOctave READ defaultOctave WRITE setDefaultOctave NOTIFY settingsChanged)
    Q_PROPERTY(bool sevenths READ sevenths WRITE setSevenths NOTIFY settingsChanged)
    Q_PROPERTY(bool latchEnabled READ latchEnabled WRITE setLatchEnabled NOTIFY settingsChanged)
    Q_PROPERTY(int latchedRow READ latchedRow NOTIFY latchChanged)
    Q_PROPERTY(int initialKey READ initialKey CONSTANT)
    Q_PROPERTY(int initialScale READ initialScale CONSTANT)
    Q_PROPERTY(int initialPatch READ initialPatch CONSTANT)
    Q_PROPERTY(QString currentPatchName READ currentPatchName NOTIFY patchChanged)
    Q_PROPERTY(QString currentPatchCategory READ currentPatchCategory NOTIFY patchChanged)
    Q_PROPERTY(bool userPatchActive READ userPatchActive NOTIFY patchChanged)
    Q_PROPERTY(bool running READ running NOTIFY statusChanged)
    Q_PROPERTY(int activeVoices READ activeVoices NOTIFY statusChanged)
    Q_PROPERTY(double outputPeak READ outputPeak NOTIFY statusChanged)
    Q_PROPERTY(qulonglong xruns READ xruns NOTIFY statusChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit EngineBridge(QObject* parent = nullptr,
                          bridge::CommandSink* commandSink = nullptr);
    ~EngineBridge() override;

    [[nodiscard]] ParameterModel* parameterModel() noexcept { return &parameterModel_; }
    [[nodiscard]] PatchModel* patchModel() noexcept { return &patchModel_; }
    [[nodiscard]] ChainModel* chainModel() noexcept { return &chainModel_; }
    [[nodiscard]] ChordModel* chordModel() noexcept { return &chordModel_; }
    [[nodiscard]] EffectParameterModel* effectParameterModel() noexcept {
        return &effectParameterModel_;
    }
    [[nodiscard]] int selectedEffectSlot() const noexcept { return selectedEffectSlot_; }
    [[nodiscard]] QString selectedEffectTypeName() const;
    [[nodiscard]] int pianoOctave() const noexcept { return pianoOctave_; }
    [[nodiscard]] QString themeName() const { return settings_.theme; }
    [[nodiscard]] int accentPreset() const noexcept { return settings_.accentPreset; }
    [[nodiscard]] int uiScale() const noexcept { return settings_.uiScale; }
    [[nodiscard]] int knobSensitivity() const noexcept { return settings_.knobSensitivity; }
    [[nodiscard]] int velocityCurve() const noexcept { return settings_.velocityCurve; }
    [[nodiscard]] int defaultOctave() const noexcept { return settings_.defaultOctave; }
    [[nodiscard]] bool sevenths() const noexcept { return settings_.sevenths; }
    [[nodiscard]] bool latchEnabled() const noexcept { return settings_.latch; }
    [[nodiscard]] int latchedRow() const noexcept { return latchedRow_; }
    [[nodiscard]] int initialKey() const noexcept { return settings_.lastKey; }
    [[nodiscard]] int initialScale() const noexcept { return settings_.lastScale; }
    [[nodiscard]] int initialPatch() const noexcept { return settings_.lastPatch; }
    [[nodiscard]] QString currentPatchName() const { return editingName_; }
    [[nodiscard]] QString currentPatchCategory() const { return editingCategory_; }
    [[nodiscard]] bool userPatchActive() const noexcept { return userPatchActive_; }
    void setThemeName(const QString& theme);
    void setAccentPreset(int preset);
    void setUiScale(int scale);
    void setKnobSensitivity(int sensitivity);
    void setVelocityCurve(int curve);
    void setDefaultOctave(int octave);
    void setSevenths(bool sevenths);
    void setLatchEnabled(bool enabled);
    [[nodiscard]] bool running() const noexcept { return engine_.isRunning(); }
    [[nodiscard]] int activeVoices() const noexcept;
    [[nodiscard]] double outputPeak() const noexcept { return outputPeak_; }
    [[nodiscard]] qulonglong xruns() const noexcept {
        return static_cast<qulonglong>(xruns_.load(std::memory_order_relaxed));
    }
    [[nodiscard]] QString error() const { return error_; }

    Q_INVOKABLE bool start();
    Q_INVOKABLE void shutdown();
    Q_INVOKABLE bool setParameter(qulonglong id, double value);
    Q_INVOKABLE bool selectPatch(int index);
    Q_INVOKABLE bool setEffectType(int slot, qulonglong type);
    Q_INVOKABLE bool setEffectBypass(int slot, bool bypassed);
    Q_INVOKABLE bool setEffectMix(int slot, double mix);
    Q_INVOKABLE bool setEffectParameter(int slot, qulonglong id, double value);
    Q_INVOKABLE void selectEffectSlot(int slot);
    /// Piano notes are ungrouped voices: they reference-count repeated presses
    /// among themselves and never retrigger or release a held chord voice.
    Q_INVOKABLE bool noteOn(int note, double velocity = 0.8);
    Q_INVOKABLE bool noteOff(int note);
    Q_INVOKABLE void setPianoOctave(int octave);
    Q_INVOKABLE bool shiftPianoOctave(int delta);
    /// Semitone offset (0 = C) for a Qt key code, or -1 when the key is not part
    /// of the one-octave computer keyboard (A W S E D F T G Y H U J K).
    Q_INVOKABLE int semitoneForKey(int key) const;
    /// MIDI note for a semitone offset in the current octave, clamped to 0..127.
    Q_INVOKABLE int midiNoteForSemitone(int semitone) const;
    Q_INVOKABLE double applyVelocityCurve(double velocity) const;
    Q_INVOKABLE QStringList chordQualityNames() const;
    Q_INVOKABLE QStringList noteNames() const;
    Q_INVOKABLE QVariantMap chordSlot(int row) const;
    Q_INVOKABLE bool setChordSlot(int row, bool manual, int root, int quality,
                                  int inversion, int octave);
    Q_INVOKABLE bool resetChordSlot(int row);
    Q_INVOKABLE bool saveSettings();
    Q_INVOKABLE QString settingsPath() const;
    Q_INVOKABLE int patchCount() const;
    Q_INVOKABLE QVariantMap patchInfo(int index) const;
    Q_INVOKABLE bool loadUserPatch(const QString& filePath);
    Q_INVOKABLE bool saveCurrentPatchToFile(const QString& filePath);
    Q_INVOKABLE QString userPatchDirectory() const;
    Q_INVOKABLE QStringList userPatchFiles() const;
    Q_INVOKABLE bool deleteUserPatch(const QString& filePath);
    Q_INVOKABLE bool duplicateUserPatch(const QString& filePath);
    Q_INVOKABLE QString patchFileName(const QString& name) const;
    Q_INVOKABLE QString formatParameterValue(qulonglong id, double value) const;
    Q_INVOKABLE QString formatUnitValue(const QString& unit, double value) const;
    Q_INVOKABLE bool setChordScale(int tonicSemitones, bool naturalMinor);
    Q_INVOKABLE bool playChord(int row, double velocity = 0.8);
    Q_INVOKABLE bool releaseChord(int row);
    Q_INVOKABLE qulonglong parameterId(const QString& name) const;
    Q_INVOKABLE qulonglong effectTypeId(int index) const noexcept;
    /// Names in kEffectTypeCycle order: the selector and the console both read
    /// this list instead of keeping their own copy.
    Q_INVOKABLE QStringList effectTypeNames() const;

Q_SIGNALS:
    void statusChanged();
    void errorChanged();
    void selectedEffectSlotChanged();
    void pianoOctaveChanged();
    void settingsChanged();
    void latchChanged();
    void patchChanged();

private Q_SLOTS:
    void refreshStatus();

private:
    void setError(QString message);
    void applyPatchToMirror(int index);
    void applyPatchToMirror(const synth::Patch& patch);
    void applyUserPatch(const synth::Patch& patch);
    void refreshEffectParameterModel();
    void applyChordSettings();
    bool releaseChordInternal(int row);
    void persistSettings();

    synth::SynthProcessor synth_;
    engine::AudioEngine engine_;
    bridge::AudioEngineCommandSink audioCommandSink_;
    bridge::CommandSink* commandSink_ = nullptr;
    bool commandSinkInjected_ = false;
    bridge::ParameterMirror parameterMirror_;
    bridge::ChainMirror chainMirror_;
    bridge::ChordExpander chordExpander_;
    ParameterModel parameterModel_;
    PatchModel patchModel_;
    ChainModel chainModel_;
    ChordModel chordModel_;
    EffectParameterModel effectParameterModel_;
    QTimer timer_;
    QString error_;
    AppSettings settings_{};
    synth::Patch editingPatch_{};
    QString editingName_;
    QString editingCategory_;
    bool userPatchActive_ = false;
    int latchedRow_ = -1;
    std::atomic<std::uint64_t> xruns_{0};
    double outputPeak_ = 0.0;
    int selectedEffectSlot_ = 0;
    int pianoOctave_ = 4;
    std::array<bool, 7> chordHeld_{};
    std::array<std::uint8_t, 128> noteRefCounts_{};
    bool initialized_ = false;
};

}  // namespace drumas::ui
