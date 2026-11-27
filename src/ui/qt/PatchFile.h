#pragma once

#include <optional>

#include <QJsonObject>
#include <QString>
#include <QStringList>

#include "synth/Patch.h"

namespace drumas::ui {

inline constexpr int kPatchSchemaVersion = 1;

/// A patch plus the name/category that live outside the trivially-copyable
/// engine struct (whose name is a view into a literal).
struct PatchDocument {
    QString name;
    QString category;
    synth::Patch patch;
};

/// Versioned JSON patches. Parameter keys are the readable registry names
/// ("filter.cutoff"), never hashes; unknown keys are ignored and known values
/// are clamped to the registry range on load.
class PatchFile final {
public:
    /// `%APPDATA%\\Drumaspro\\patches` (or `DRUMAS_PATCHES_PATH`).
    static QString directory();
    static QString fileNameFor(const QString& name);

    static QJsonObject toJson(const PatchDocument& document);
    static std::optional<PatchDocument> fromJson(const QJsonObject& object,
                                                 QString* error = nullptr);

    static bool save(const PatchDocument& document, const QString& filePath,
                     QString* error = nullptr);
    static std::optional<PatchDocument> load(const QString& filePath,
                                             QString* error = nullptr);

    /// Absolute paths of the .json files in `directory`, sorted by name.
    static QStringList listFiles(const QString& directory = {});
};

}  // namespace drumas::ui
