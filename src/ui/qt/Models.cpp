#include "Models.h"

#include <QString>

#include <algorithm>
#include <cstddef>
#include <memory>

#include "fx/EffectFactory.h"

namespace drumas::ui {
namespace {

[[nodiscard]] QString fromUtf8(std::string_view value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

[[nodiscard]] QString unitName(core::ParameterUnit unit) {
    switch (unit) {
        case core::ParameterUnit::Hertz: return QStringLiteral("Hz");
        case core::ParameterUnit::Milliseconds: return QStringLiteral("ms");
        case core::ParameterUnit::Percent: return QStringLiteral("%");
        case core::ParameterUnit::Decibels: return QStringLiteral("dB");
        case core::ParameterUnit::Semitones: return QStringLiteral("st");
        case core::ParameterUnit::Cents: return QStringLiteral("ct");
        case core::ParameterUnit::Octaves: return QStringLiteral("oct");
        case core::ParameterUnit::Ratio: return QStringLiteral("ratio");
        case core::ParameterUnit::None: return {};
    }
    return {};
}

constexpr std::array<music::PitchClass, 12> kPitchClasses{
    music::PitchClass::C, music::PitchClass::CSharp, music::PitchClass::D,
    music::PitchClass::DSharp, music::PitchClass::E, music::PitchClass::F,
    music::PitchClass::FSharp, music::PitchClass::G, music::PitchClass::GSharp,
    music::PitchClass::A, music::PitchClass::ASharp, music::PitchClass::B};

}  // namespace

ParameterModel::ParameterModel(QObject* parent) : QAbstractListModel(parent) {}

int ParameterModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(snapshot_.count);
}

QVariant ParameterModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const bridge::ParameterValue& parameter =
        snapshot_.values[static_cast<std::size_t>(index.row())];
    switch (role) {
        case IdRole: return static_cast<qulonglong>(parameter.descriptor.id);
        case NameRole: return fromUtf8(parameter.descriptor.name);
        case ValueRole: return parameter.value;
        case MinimumRole: return parameter.descriptor.minValue;
        case MaximumRole: return parameter.descriptor.maxValue;
        case UnitRole: return unitName(parameter.descriptor.unit);
        case CurveRole: return static_cast<int>(parameter.descriptor.curve);
        default: return {};
    }
}

QHash<int, QByteArray> ParameterModel::roleNames() const {
    return {{IdRole, "parameterId"}, {NameRole, "name"}, {ValueRole, "value"},
            {MinimumRole, "minimum"}, {MaximumRole, "maximum"}, {UnitRole, "unit"},
            {CurveRole, "curve"}};
}

int ParameterModel::count() const noexcept { return static_cast<int>(snapshot_.count); }

double ParameterModel::valueForId(qulonglong id) const noexcept {
    for (std::size_t row = 0; row < snapshot_.count; ++row) {
        if (snapshot_.values[row].descriptor.id == id) {
            return snapshot_.values[row].value;
        }
    }
    return 0.0;
}

double ParameterModel::minimumForId(qulonglong id) const noexcept {
    for (std::size_t row = 0; row < snapshot_.count; ++row) {
        if (snapshot_.values[row].descriptor.id == id) {
            return snapshot_.values[row].descriptor.minValue;
        }
    }
    return 0.0;
}

double ParameterModel::maximumForId(qulonglong id) const noexcept {
    for (std::size_t row = 0; row < snapshot_.count; ++row) {
        if (snapshot_.values[row].descriptor.id == id) {
            return snapshot_.values[row].descriptor.maxValue;
        }
    }
    return 1.0;
}

QString ParameterModel::unitForId(qulonglong id) const {
    for (std::size_t row = 0; row < snapshot_.count; ++row) {
        if (snapshot_.values[row].descriptor.id == id) {
            return unitName(snapshot_.values[row].descriptor.unit);
        }
    }
    return {};
}

int ParameterModel::curveForId(qulonglong id) const noexcept {
    for (std::size_t row = 0; row < snapshot_.count; ++row) {
        if (snapshot_.values[row].descriptor.id == id) {
            return static_cast<int>(snapshot_.values[row].descriptor.curve);
        }
    }
    return 0;
}

void ParameterModel::setSnapshot(const bridge::ParameterSnapshot& snapshot) {
    beginResetModel();
    snapshot_ = snapshot;
    endResetModel();
    Q_EMIT countChanged();
}

void ParameterModel::setValue(core::ParameterId id, float value) {
    for (std::size_t row = 0; row < snapshot_.count; ++row) {
        if (snapshot_.values[row].descriptor.id == id) {
            snapshot_.values[row].value = value;
            Q_EMIT dataChanged(index(static_cast<int>(row)), index(static_cast<int>(row)),
                               {ValueRole});
            return;
        }
    }
}

EffectParameterModel::EffectParameterModel(QObject* parent) : QAbstractListModel(parent) {}

int EffectParameterModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : count_;
}

QVariant EffectParameterModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count_) {
        return {};
    }
    const Row& row = rows_[static_cast<std::size_t>(index.row())];
    switch (role) {
        case IdRole: return static_cast<qulonglong>(row.descriptor.id);
        case NameRole: return fromUtf8(row.descriptor.name);
        case ValueRole: return row.value;
        case MinimumRole: return row.descriptor.minValue;
        case MaximumRole: return row.descriptor.maxValue;
        case UnitRole: return unitName(row.descriptor.unit);
        case CurveRole: return static_cast<int>(row.descriptor.curve);
        default: return {};
    }
}

QHash<int, QByteArray> EffectParameterModel::roleNames() const {
    return {{IdRole, "parameterId"}, {NameRole, "name"}, {ValueRole, "value"},
            {MinimumRole, "minimum"}, {MaximumRole, "maximum"}, {UnitRole, "unit"},
            {CurveRole, "curve"}};
}

int EffectParameterModel::count() const noexcept { return count_; }

double EffectParameterModel::valueForId(qulonglong id) const noexcept {
    for (int row = 0; row < count_; ++row) {
        if (rows_[static_cast<std::size_t>(row)].descriptor.id == id) {
            return rows_[static_cast<std::size_t>(row)].value;
        }
    }
    return 0.0;
}

void EffectParameterModel::setEffect(fx::EffectTypeId type,
                                     const fx::EffectSlotDescription& description) {
    beginResetModel();
    count_ = 0;
    auto effect = fx::createEffect(type);
    if (effect != nullptr) {
        const core::ParameterRegistry& registry = effect->parameters();
        for (std::size_t index = 0;
             index < registry.size() && count_ < static_cast<int>(fx::kMaxParametersPerEffect);
             ++index) {
            const core::ParameterDescriptor& descriptor = registry.at(index);
            float value = descriptor.defaultValue;
            if (const fx::EffectParameterValue* stored =
                    fx::findParameter(description, descriptor.id)) {
                value = stored->value;
            }
            rows_[static_cast<std::size_t>(count_)] = Row{descriptor, value};
            ++count_;
        }
    }
    endResetModel();
    Q_EMIT countChanged();
}

void EffectParameterModel::setValue(core::ParameterId id, float value) {
    for (int row = 0; row < count_; ++row) {
        Row& candidate = rows_[static_cast<std::size_t>(row)];
        if (candidate.descriptor.id == id) {
            candidate.value = std::clamp(value, candidate.descriptor.minValue,
                                         candidate.descriptor.maxValue);
            Q_EMIT dataChanged(index(row), index(row), {ValueRole});
            return;
        }
    }
}

PatchModel::PatchModel(QObject* parent) : QAbstractListModel(parent) {}

int PatchModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : synth::kFactoryPatchCount;
}

QVariant PatchModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= synth::kFactoryPatchCount) {
        return {};
    }
    if (role == IndexRole) {
        return index.row();
    }
    if (role == NameRole) {
        return fromUtf8(synth::kFactoryPatches[static_cast<std::size_t>(index.row())].name);
    }
    return {};
}

QHash<int, QByteArray> PatchModel::roleNames() const {
    return {{IndexRole, "patchIndex"}, {NameRole, "name"}};
}

ChainModel::ChainModel(QObject* parent) : QAbstractListModel(parent) {}

int ChainModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(description_.slots.size());
}

QVariant ChainModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const fx::EffectSlotDescription& slot =
        description_.slots[static_cast<std::size_t>(index.row())];
    switch (role) {
        case SlotRole: return index.row();
        case TypeRole: return slot.effectType;
        case TypeNameRole: return fromUtf8(fx::effectTypeName(slot.effectType));
        case BypassRole: return slot.bypass;
        case MixRole: return slot.mix;
        default: return {};
    }
}

QHash<int, QByteArray> ChainModel::roleNames() const {
    return {{SlotRole, "slot"}, {TypeRole, "effectType"}, {TypeNameRole, "effectName"},
            {BypassRole, "isBypassed"}, {MixRole, "effectMix"}};
}

void ChainModel::setDescription(const fx::ChainDescription& description) {
    beginResetModel();
    description_ = description;
    endResetModel();
}

ChordModel::ChordModel(QObject* parent) : QAbstractListModel(parent) {
    (void)setScale(0, false);
}

void ChordModel::rebuildChords() {
    const music::Scale scale{
        kPitchClasses[static_cast<std::size_t>(tonic_)],
        naturalMinor_ ? music::ScaleType::NaturalMinor : music::ScaleType::Major};
    const auto result = music::diatonicChords(
        scale, sevenths_ ? music::DiatonicChordType::Seventh
                         : music::DiatonicChordType::Triad);
    if (!result.has_value()) {
        return;
    }
    beginResetModel();
    chords_ = *result;
    count_ = static_cast<int>(chords_.size());
    endResetModel();
}

int ChordModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : count_;
}

QVariant ChordModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= count_) {
        return {};
    }
    const std::size_t row = static_cast<std::size_t>(index.row());
    const music::DiatonicChord& diatonic = chords_[row];
    const music::Chord chord = manualChords_[row].value_or(diatonic.chord);
    const auto chordName = music::chordName(chord);
    switch (role) {
        case DegreeRole: return diatonic.degree;
        case NumeralRole: return fromUtf8(diatonic.numeral);
        case RootRole:
            return chordName.has_value() ? fromUtf8(chordName->root) : QString{};
        case QualityRole:
            return chordName.has_value() ? fromUtf8(chordName->quality) : QString{};
        case MidiNotesRole: {
            QVariantList notes;
            const auto midi = music::chordToMidiNotes(chord);
            if (midi.has_value()) {
                for (std::size_t note = 0; note < midi->count; ++note) {
                    notes.append(midi->notes[note]);
                }
            }
            return notes;
        }
        case ManualRole: return manualChords_[row].has_value();
        default: return {};
    }
}

QHash<int, QByteArray> ChordModel::roleNames() const {
    return {{DegreeRole, "degree"}, {NumeralRole, "degreeNumeral"},
            {RootRole, "chordRoot"}, {QualityRole, "chordQuality"},
            {MidiNotesRole, "chordMidiNotes"}, {ManualRole, "chordManual"}};
}

bool ChordModel::setScale(int tonicSemitones, bool naturalMinor) {
    if (tonicSemitones < 0 || tonicSemitones >= static_cast<int>(kPitchClasses.size())) {
        return false;
    }
    tonic_ = tonicSemitones;
    naturalMinor_ = naturalMinor;
    rebuildChords();
    return true;
}

bool ChordModel::setSevenths(bool sevenths) {
    if (sevenths_ == sevenths) {
        return true;
    }
    sevenths_ = sevenths;
    rebuildChords();
    return true;
}

bool ChordModel::setManualChord(int row, const std::optional<music::Chord>& chord) {
    if (row < 0 || row >= count_) {
        return false;
    }
    if (chord.has_value() && !music::chordToMidiNotes(*chord).has_value()) {
        return false;
    }
    manualChords_[static_cast<std::size_t>(row)] = chord;
    const QModelIndex changed = index(row);
    Q_EMIT dataChanged(changed, changed,
                       {RootRole, QualityRole, MidiNotesRole, ManualRole});
    return true;
}

bool ChordModel::isManual(int row) const noexcept {
    return row >= 0 && row < count_ && manualChords_[static_cast<std::size_t>(row)].has_value();
}

bool ChordModel::chordAt(int row, music::Chord& chord) const noexcept {
    if (row < 0 || row >= count_) {
        return false;
    }
    const std::size_t slot = static_cast<std::size_t>(row);
    chord = manualChords_[slot].value_or(chords_[slot].chord);
    return true;
}

}  // namespace drumas::ui
