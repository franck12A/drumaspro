#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

#include <QObject>
#include <QString>
#include <QTimer>

#include "bridge/ChainMirror.h"
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
    Q_INVOKABLE bool setChordScale(int tonicSemitones, bool naturalMinor);
    Q_INVOKABLE bool playChord(int row, double velocity = 0.8);
    Q_INVOKABLE bool releaseChord(int row);
    Q_INVOKABLE qulonglong parameterId(const QString& name) const;
    Q_INVOKABLE qulonglong effectTypeId(int index) const noexcept;

Q_SIGNALS:
    void statusChanged();
    void errorChanged();
    void selectedEffectSlotChanged();

private Q_SLOTS:
    void refreshStatus();

private:
    void setError(QString message);
    void applyPatchToMirror(int index);
    void refreshEffectParameterModel();

    synth::SynthProcessor synth_;
    engine::AudioEngine engine_;
    bridge::AudioEngineCommandSink audioCommandSink_;
    bridge::CommandSink* commandSink_ = nullptr;
    bool commandSinkInjected_ = false;
    bridge::ParameterMirror parameterMirror_;
    bridge::ChainMirror chainMirror_;
    ParameterModel parameterModel_;
    PatchModel patchModel_;
    ChainModel chainModel_;
    ChordModel chordModel_;
    EffectParameterModel effectParameterModel_;
    QTimer timer_;
    QString error_;
    std::atomic<std::uint64_t> xruns_{0};
    double outputPeak_ = 0.0;
    int selectedEffectSlot_ = 0;
    std::array<std::array<std::uint8_t, 4>, 7> chordNotes_{};
    std::array<std::uint8_t, 7> chordNoteCounts_{};
    std::array<bool, 7> chordHeld_{};
    std::array<std::uint8_t, 128> noteRefCounts_{};
    bool initialized_ = false;
};

}  // namespace drumas::ui
