#pragma once

#include <array>

#include <QAbstractListModel>
#include <QVariant>

#include "bridge/ChainMirror.h"
#include "bridge/ParameterMirror.h"
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
    };
    Q_ENUM(Role)

    explicit ChordModel(QObject* parent = nullptr);
    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] bool setScale(int tonicSemitones, bool naturalMinor);
    [[nodiscard]] bool chordAt(int row, music::Chord& chord) const noexcept;

private:
    std::array<music::DiatonicChord, 7> chords_{};
    int count_ = 0;
};

}  // namespace drumas::ui
