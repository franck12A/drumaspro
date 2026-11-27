#pragma once

#include <array>
#include <optional>

#include <QJsonObject>
#include <QString>

namespace drumas::ui {

inline constexpr int kSettingsSchemaVersion = 1;
inline constexpr int kChordSlotCount = 8;
inline constexpr int kChordQualityCount = 10;
inline constexpr int kAccentPresetCount = 6;
inline constexpr int kVelocityCurveCount = 3;

struct ChordSlotSetting {
    bool manual = false;
    int root = 0;       // 0..11 semitone
    int quality = 0;    // index into music::ChordQuality
    int inversion = 0;  // 0..3
    int octave = 4;     // 0..8
};

struct AppSettings {
    int schemaVersion = kSettingsSchemaVersion;
    QString theme = QStringLiteral("default");
    int accentPreset = 0;
    int uiScale = 100;
    int knobSensitivity = 100;
    int velocityCurve = 0;
    int defaultOctave = 4;
    int lastKey = 0;
    int lastScale = 0;
    int lastPatch = 0;
    bool sevenths = false;
    bool latch = false;
    std::array<ChordSlotSetting, kChordSlotCount> chordSlots{};
};

/// Thin, testable wrapper over QJsonDocument. The rest of the UI never touches
/// JSON directly.
class SettingsStore final {
public:
    /// `%APPDATA%\\Drumaspro`, or the platform data location as a fallback.
    static QString directory();
    static QString path();

    /// Missing or corrupt files yield defaults; `error` is set when the file
    /// existed but could not be used. Future schema versions are rejected the
    /// same way so a newer build's file is never half-applied.
    static AppSettings load(const QString& filePath, QString* error = nullptr);
    /// Atomic: writes a temporary file and commits it over the target.
    static bool save(const AppSettings& settings, const QString& filePath,
                     QString* error = nullptr);

    static QJsonObject toJson(const AppSettings& settings);
    static AppSettings fromJson(const QJsonObject& object, QString* error = nullptr);
};

}  // namespace drumas::ui
