#pragma once

#include <atomic>
#include <cstdint>
#include <string>

#include <QObject>
#include <QString>
#include <QTimer>

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
    Q_PROPERTY(bool running READ running NOTIFY statusChanged)
    Q_PROPERTY(int activeVoices READ activeVoices NOTIFY statusChanged)
    Q_PROPERTY(qulonglong xruns READ xruns NOTIFY statusChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    explicit EngineBridge(QObject* parent = nullptr);
    ~EngineBridge() override;

    [[nodiscard]] ParameterModel* parameterModel() noexcept { return &parameterModel_; }
    [[nodiscard]] PatchModel* patchModel() noexcept { return &patchModel_; }
    [[nodiscard]] ChainModel* chainModel() noexcept { return &chainModel_; }
    [[nodiscard]] ChordModel* chordModel() noexcept { return &chordModel_; }
    [[nodiscard]] bool running() const noexcept { return engine_.isRunning(); }
    [[nodiscard]] int activeVoices() const noexcept;
    [[nodiscard]] qulonglong xruns() const noexcept {
        return static_cast<qulonglong>(xruns_.load(std::memory_order_relaxed));
    }
    [[nodiscard]] QString error() const { return error_; }

    Q_INVOKABLE bool start();
    Q_INVOKABLE void shutdown();
    Q_INVOKABLE bool setParameter(quint32 id, double value);
    Q_INVOKABLE bool selectPatch(int index);
    Q_INVOKABLE bool setEffectType(int slot, quint32 type);
    Q_INVOKABLE bool setEffectBypass(int slot, bool bypassed);
    Q_INVOKABLE bool setEffectMix(int slot, double mix);
    Q_INVOKABLE bool setEffectParameter(int slot, quint32 id, double value);
    Q_INVOKABLE bool setChordScale(int tonicSemitones, bool naturalMinor);
    Q_INVOKABLE bool playChord(int row, double velocity = 0.8);
    Q_INVOKABLE bool releaseChord(int row);
    Q_INVOKABLE quint32 parameterId(const QString& name) const;
    Q_INVOKABLE quint32 effectTypeId(int index) const noexcept;

Q_SIGNALS:
    void statusChanged();
    void errorChanged();

private Q_SLOTS:
    void refreshStatus();

private:
    void setError(QString message);

    synth::SynthProcessor synth_;
    engine::AudioEngine engine_;
    bridge::AudioEngineCommandSink commandSink_;
    bridge::ParameterMirror parameterMirror_;
    bridge::ChainMirror chainMirror_;
    bridge::ChordExpander chordExpander_;
    ParameterModel parameterModel_;
    PatchModel patchModel_;
    ChainModel chainModel_;
    ChordModel chordModel_;
    QTimer timer_;
    QString error_;
    std::atomic<std::uint64_t> xruns_{0};
    bool initialized_ = false;
};

}  // namespace drumas::ui
