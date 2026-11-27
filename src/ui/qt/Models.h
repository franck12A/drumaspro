#pragma once

#include <array>
#include <optional>

#include <QAbstractListModel>
#include <QVariant>

#include "bridge/ChainMirror.h"
#include "bridge/ParameterMirror.h"
#include "fx/ChainDescription.h"
#include "music/Scale.h"
#include "synth/PatchBank.h"

namespace drumas::ui {

class ParameterModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ValueRole,
        MinimumRole,
        MaximumRole,
        UnitRole,
        CurveRole,
    };
    Q_ENUM(Role)

    explicit ParameterModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const noexcept;
    Q_INVOKABLE double valueForId(qulonglong id) const noexcept;
    Q_INVOKABLE double minimumForId(qulonglong id) const noexcept;
    Q_INVOKABLE double maximumForId(qulonglong id) const noexcept;
    Q_INVOKABLE QString unitForId(qulonglong id) const;
    Q_INVOKABLE int curveForId(qulonglong id) const noexcept;

    void setSnapshot(const bridge::ParameterSnapshot& snapshot);
    void setValue(core::ParameterId id, float value);

Q_SIGNALS:
    void countChanged();

private:
    bridge::ParameterSnapshot snapshot_{};
};

class EffectParameterModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        NameRole,
        ValueRole,
        MinimumRole,
        MaximumRole,
        UnitRole,
        CurveRole,
    };
    Q_ENUM(Role)

    explicit EffectParameterModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    [[nodiscard]] int count() const noexcept;
    Q_INVOKABLE double valueForId(qulonglong id) const noexcept;

    /// Control thread. Copies the effect's registry and the values currently in
    /// the slot description (falling back to each parameter's default).
    void setEffect(fx::EffectTypeId type, const fx::EffectSlotDescription& description);
    void setValue(core::ParameterId id, float value);

Q_SIGNALS:
    void countChanged();

private:
    struct Row {
        core::ParameterDescriptor descriptor{};
        float value = 0.0F;
    };

    std::array<Row, fx::kMaxParametersPerEffect> rows_{};
    int count_ = 0;
};

class PatchModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        IndexRole = Qt::UserRole + 1,
        NameRole,
    };
    Q_ENUM(Role)

    explicit PatchModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
};

class ChainModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SlotRole = Qt::UserRole + 1,
        TypeRole,
        TypeNameRole,
        BypassRole,
        MixRole,
    };
    Q_ENUM(Role)

    explicit ChainModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;
    void setDescription(const fx::ChainDescription& description);

private:
    fx::ChainDescription description_ = fx::defaultChainDescription();
};

class ChordModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        DegreeRole = Qt::UserRole + 1,
        NumeralRole,
        RootRole,
        QualityRole,
        MidiNotesRole,
        ManualRole,
    };
    Q_ENUM(Role)

    explicit ChordModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool setScale(int tonicSemitones, bool naturalMinor);
    [[nodiscard]] bool setSevenths(bool sevenths);
    /// Manual override for one pad; `std::nullopt` restores the diatonic chord.
    [[nodiscard]] bool setManualChord(int row, const std::optional<music::Chord>& chord);
    [[nodiscard]] bool chordAt(int row, music::Chord& chord) const noexcept;
    [[nodiscard]] bool isManual(int row) const noexcept;

private:
    void rebuildChords();

    std::array<music::DiatonicChord, 7> chords_{};
    std::array<std::optional<music::Chord>, 7> manualChords_{};
    int count_ = 0;
    int tonic_ = 0;
    bool naturalMinor_ = false;
    bool sevenths_ = false;
};

}  // namespace drumas::ui
