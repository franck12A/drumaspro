#include "Settings.h"

#include <algorithm>
#include <array>
#include <cmath>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QSaveFile>
#include <QStandardPaths>

namespace drumas::ui {
namespace {

[[nodiscard]] int clampedInt(const QJsonObject& object, const QString& key, int fallback,
                             int minimum, int maximum) {
    if (!object.contains(key)) {
        return fallback;
    }
    const QJsonValue value = object.value(key);
    if (!value.isDouble()) {
        return fallback;
    }
    const double number = value.toDouble();
    if (!std::isfinite(number)) {
        return fallback;
    }
    return std::clamp(static_cast<int>(number), minimum, maximum);
}

[[nodiscard]] bool readBool(const QJsonObject& object, const QString& key, bool fallback) {
    if (!object.contains(key)) {
        return fallback;
    }
    const QJsonValue value = object.value(key);
    return value.isBool() ? value.toBool() : fallback;
}

[[nodiscard]] int readUiScale(const QJsonObject& object, int fallback) {
    const int scale = clampedInt(object, QStringLiteral("uiScale"), fallback, 50, 200);
    return (scale == 90 || scale == 100 || scale == 110 || scale == 125) ? scale : fallback;
}

[[nodiscard]] QString readTheme(const QJsonObject& object, const QString& fallback) {
    const QString theme = object.value(QStringLiteral("theme")).toString(fallback);
    if (theme == QLatin1String("default") || theme == QLatin1String("amber") ||
        theme == QLatin1String("night")) {
        return theme;
    }
    return fallback;
}

[[nodiscard]] ChordSlotSetting readChordSlot(const QJsonValue& value,
                                             const ChordSlotSetting& fallback) {
    if (!value.isObject()) {
        return fallback;
    }
    const QJsonObject object = value.toObject();
    ChordSlotSetting slot;
    slot.manual = readBool(object, QStringLiteral("manual"), fallback.manual);
    slot.root = clampedInt(object, QStringLiteral("root"), fallback.root, 0, 11);
    slot.quality = clampedInt(object, QStringLiteral("quality"), fallback.quality, 0,
                              kChordQualityCount - 1);
    slot.inversion = clampedInt(object, QStringLiteral("inversion"), fallback.inversion, 0, 3);
    slot.octave = clampedInt(object, QStringLiteral("octave"), fallback.octave, 0, 8);
    return slot;
}

[[nodiscard]] QJsonObject chordSlotToJson(const ChordSlotSetting& slot) {
    return QJsonObject{{QStringLiteral("manual"), slot.manual},
                       {QStringLiteral("root"), slot.root},
                       {QStringLiteral("quality"), slot.quality},
                       {QStringLiteral("inversion"), slot.inversion},
                       {QStringLiteral("octave"), slot.octave}};
}

}  // namespace

QString SettingsStore::directory() {
    const QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty()) {
        return QDir::cleanPath(appData + QStringLiteral("/Drumaspro"));
    }
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString SettingsStore::path() {
    // Tests and portable runs point this at a scratch file so a developer's
    // real settings never leak into a test run.
    const QString override = qEnvironmentVariable("DRUMAS_SETTINGS_PATH");
    if (!override.isEmpty()) {
        return override;
    }
    return QDir(directory()).filePath(QStringLiteral("settings.json"));
}

AppSettings SettingsStore::fromJson(const QJsonObject& object, QString* error) {
    AppSettings settings;
    if (object.isEmpty()) {
        return settings;
    }

    const int version = object.value(QStringLiteral("schemaVersion"))
                            .toInt(kSettingsSchemaVersion);
    if (version > kSettingsSchemaVersion) {
        if (error != nullptr) {
            *error = QStringLiteral("settings schemaVersion %1 is newer than %2")
                         .arg(version)
                         .arg(kSettingsSchemaVersion);
        }
        return settings;
    }

    settings.schemaVersion = kSettingsSchemaVersion;
    settings.theme = readTheme(object, settings.theme);
    settings.accentPreset =
        clampedInt(object, QStringLiteral("accentPreset"), settings.accentPreset, 0,
                   kAccentPresetCount - 1);
    settings.uiScale = readUiScale(object, settings.uiScale);
    settings.knobSensitivity =
        clampedInt(object, QStringLiteral("knobSensitivity"), settings.knobSensitivity, 50, 150);
    settings.velocityCurve =
        clampedInt(object, QStringLiteral("velocityCurve"), settings.velocityCurve, 0,
                   kVelocityCurveCount - 1);
    settings.defaultOctave =
        clampedInt(object, QStringLiteral("defaultOctave"), settings.defaultOctave, 0, 8);
    settings.lastKey =
        clampedInt(object, QStringLiteral("lastKey"), settings.lastKey, 0, 11);
    settings.lastScale =
        clampedInt(object, QStringLiteral("lastScale"), settings.lastScale, 0, 1);
    settings.lastPatch =
        clampedInt(object, QStringLiteral("lastPatch"), settings.lastPatch, 0, 255);
    settings.sevenths = readBool(object, QStringLiteral("sevenths"), settings.sevenths);
    settings.latch = readBool(object, QStringLiteral("latch"), settings.latch);

    const QJsonValue slots = object.value(QStringLiteral("chordSlots"));
    if (slots.isArray()) {
        const QJsonArray array = slots.toArray();
        for (int row = 0; row < kChordSlotCount; ++row) {
            if (row < array.size()) {
                settings.chordSlots[static_cast<std::size_t>(row)] =
                    readChordSlot(array.at(row),
                                  settings.chordSlots[static_cast<std::size_t>(row)]);
            }
        }
    }
    return settings;
}

QJsonObject SettingsStore::toJson(const AppSettings& settings) {
    QJsonArray slots;
    for (const ChordSlotSetting& slot : settings.chordSlots) {
        slots.append(chordSlotToJson(slot));
    }
    return QJsonObject{
        {QStringLiteral("schemaVersion"), kSettingsSchemaVersion},
        {QStringLiteral("theme"), settings.theme},
        {QStringLiteral("accentPreset"), settings.accentPreset},
        {QStringLiteral("uiScale"), settings.uiScale},
        {QStringLiteral("knobSensitivity"), settings.knobSensitivity},
        {QStringLiteral("velocityCurve"), settings.velocityCurve},
        {QStringLiteral("defaultOctave"), settings.defaultOctave},
        {QStringLiteral("lastKey"), settings.lastKey},
        {QStringLiteral("lastScale"), settings.lastScale},
        {QStringLiteral("lastPatch"), settings.lastPatch},
        {QStringLiteral("sevenths"), settings.sevenths},
        {QStringLiteral("latch"), settings.latch},
        {QStringLiteral("chordSlots"), slots},
    };
}

AppSettings SettingsStore::load(const QString& filePath, QString* error) {
    AppSettings defaults;
    QFile file(filePath);
    if (!file.exists()) {
        return defaults;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("settings file cannot be opened");
        }
        return defaults;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("settings file is not valid JSON");
        }
        return defaults;
    }
    return fromJson(document.object(), error);
}

bool SettingsStore::save(const AppSettings& settings, const QString& filePath,
                         QString* error) {
    const QDir directory = QFileInfo(filePath).dir();
    if (!QDir().mkpath(directory.absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("settings directory cannot be created");
        }
        return false;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("settings file cannot be written");
        }
        return false;
    }
    const QByteArray payload = QJsonDocument(toJson(settings)).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("settings file could not be committed");
        }
        return false;
    }
    return true;
}

}  // namespace drumas::ui
