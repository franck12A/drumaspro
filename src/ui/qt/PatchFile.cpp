#include "PatchFile.h"

#include <array>
#include <cmath>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>

#include "synth/ParameterIds.h"
#include "synth/SynthProcessor.h"

namespace drumas::ui {
namespace {

using drumas::core::ParameterId;
namespace ids = drumas::synth;

[[nodiscard]] const drumas::core::ParameterRegistry& synthRegistry() {
    // The registry is deliberately leaked once: it is immutable after setup and
    // the process owns it for its whole lifetime.
    static const drumas::core::ParameterRegistry* registry = [] {
        auto* built = new drumas::core::ParameterRegistry();
        (void)drumas::synth::registerSynthParameters(*built);
        return built;
    }();
    return *registry;
}

struct Entry {
    ParameterId id;
    const char* name;
};

/// One entry per patch field, in the same order as the registry. The names are
/// what a user reads and edits in the JSON file.
constexpr std::array<Entry, 30> kEntries{{
    {ids::kOsc1Waveform, "osc1.waveform"},
    {ids::kOsc1Level, "osc1.level"},
    {ids::kOsc1DetuneCents, "osc1.detune"},
    {ids::kOsc1PulseWidth, "osc1.pulseWidth"},
    {ids::kOsc2Waveform, "osc2.waveform"},
    {ids::kOsc2Level, "osc2.level"},
    {ids::kOsc2DetuneCents, "osc2.detune"},
    {ids::kOsc2PulseWidth, "osc2.pulseWidth"},
    {ids::kSubLevel, "sub.level"},
    {ids::kNoiseLevel, "noise.level"},
    {ids::kUnisonVoices, "unison.voices"},
    {ids::kUnisonDetuneCents, "unison.detune"},
    {ids::kUnisonStereo, "unison.stereo"},
    {ids::kFilterCutoff, "filter.cutoff"},
    {ids::kFilterResonance, "filter.resonance"},
    {ids::kFilterEnvOctaves, "filter.envOctaves"},
    {ids::kFilterAttackMs, "filter.attack"},
    {ids::kFilterDecayMs, "filter.decay"},
    {ids::kFilterSustain, "filter.sustain"},
    {ids::kFilterReleaseMs, "filter.release"},
    {ids::kAmpAttackMs, "amp.attack"},
    {ids::kAmpDecayMs, "amp.decay"},
    {ids::kAmpSustain, "amp.sustain"},
    {ids::kAmpReleaseMs, "amp.release"},
    {ids::kVelocityVolume, "velocity.volume"},
    {ids::kVelocityBrightness, "velocity.brightness"},
    {ids::kLfoShape, "lfo.shape"},
    {ids::kLfoRate, "lfo.rate"},
    {ids::kLfoCutoffDepth, "lfo.cutoffDepth"},
    {ids::kLfoPitchDepthCents, "lfo.pitchDepth"},
}};

[[nodiscard]] float valueOf(const drumas::synth::Patch& patch, ParameterId id,
                            bool& matched) {
    matched = true;
    switch (id) {
        case ids::kOsc1Waveform:
            return static_cast<float>(static_cast<int>(patch.osc1Waveform));
        case ids::kOsc1Level: return patch.osc1Level;
        case ids::kOsc1DetuneCents: return patch.osc1DetuneCents;
        case ids::kOsc1PulseWidth: return patch.osc1PulseWidth;
        case ids::kOsc2Waveform:
            return static_cast<float>(static_cast<int>(patch.osc2Waveform));
        case ids::kOsc2Level: return patch.osc2Level;
        case ids::kOsc2DetuneCents: return patch.osc2DetuneCents;
        case ids::kOsc2PulseWidth: return patch.osc2PulseWidth;
        case ids::kSubLevel: return patch.subLevel;
        case ids::kNoiseLevel: return patch.noiseLevel;
        case ids::kUnisonVoices: return static_cast<float>(patch.unisonVoices);
        case ids::kUnisonDetuneCents: return patch.unisonDetuneCents;
        case ids::kUnisonStereo: return patch.unisonStereo;
        case ids::kFilterCutoff: return patch.cutoffHz;
        case ids::kFilterResonance: return patch.resonance;
        case ids::kFilterEnvOctaves: return patch.filterEnvOctaves;
        case ids::kFilterAttackMs: return patch.filterEnvelope.attackMs;
        case ids::kFilterDecayMs: return patch.filterEnvelope.decayMs;
        case ids::kFilterSustain: return patch.filterEnvelope.sustain;
        case ids::kFilterReleaseMs: return patch.filterEnvelope.releaseMs;
        case ids::kAmpAttackMs: return patch.ampEnvelope.attackMs;
        case ids::kAmpDecayMs: return patch.ampEnvelope.decayMs;
        case ids::kAmpSustain: return patch.ampEnvelope.sustain;
        case ids::kAmpReleaseMs: return patch.ampEnvelope.releaseMs;
        case ids::kVelocityVolume: return patch.velocityVolume;
        case ids::kVelocityBrightness: return patch.velocityBrightness;
        case ids::kLfoShape:
            return static_cast<float>(static_cast<int>(patch.lfoShape));
        case ids::kLfoRate: return patch.lfoRateHz;
        case ids::kLfoCutoffDepth: return patch.lfoCutoffDepth;
        case ids::kLfoPitchDepthCents: return patch.lfoPitchDepthCents;
        default: matched = false; return 0.0F;
    }
}

void setValue(drumas::synth::Patch& patch, ParameterId id, float value,
              bool& matched) {
    matched = true;
    switch (id) {
        case ids::kOsc1Waveform:
            patch.osc1Waveform =
                static_cast<drumas::synth::Waveform>(static_cast<int>(value));
            break;
        case ids::kOsc1Level: patch.osc1Level = value; break;
        case ids::kOsc1DetuneCents: patch.osc1DetuneCents = value; break;
        case ids::kOsc1PulseWidth: patch.osc1PulseWidth = value; break;
        case ids::kOsc2Waveform:
            patch.osc2Waveform =
                static_cast<drumas::synth::Waveform>(static_cast<int>(value));
            break;
        case ids::kOsc2Level: patch.osc2Level = value; break;
        case ids::kOsc2DetuneCents: patch.osc2DetuneCents = value; break;
        case ids::kOsc2PulseWidth: patch.osc2PulseWidth = value; break;
        case ids::kSubLevel: patch.subLevel = value; break;
        case ids::kNoiseLevel: patch.noiseLevel = value; break;
        case ids::kUnisonVoices:
            patch.unisonVoices = static_cast<int>(std::lround(value));
            break;
        case ids::kUnisonDetuneCents: patch.unisonDetuneCents = value; break;
        case ids::kUnisonStereo: patch.unisonStereo = value; break;
        case ids::kFilterCutoff: patch.cutoffHz = value; break;
        case ids::kFilterResonance: patch.resonance = value; break;
        case ids::kFilterEnvOctaves: patch.filterEnvOctaves = value; break;
        case ids::kFilterAttackMs: patch.filterEnvelope.attackMs = value; break;
        case ids::kFilterDecayMs: patch.filterEnvelope.decayMs = value; break;
        case ids::kFilterSustain: patch.filterEnvelope.sustain = value; break;
        case ids::kFilterReleaseMs: patch.filterEnvelope.releaseMs = value; break;
        case ids::kAmpAttackMs: patch.ampEnvelope.attackMs = value; break;
        case ids::kAmpDecayMs: patch.ampEnvelope.decayMs = value; break;
        case ids::kAmpSustain: patch.ampEnvelope.sustain = value; break;
        case ids::kAmpReleaseMs: patch.ampEnvelope.releaseMs = value; break;
        case ids::kVelocityVolume: patch.velocityVolume = value; break;
        case ids::kVelocityBrightness: patch.velocityBrightness = value; break;
        case ids::kLfoShape:
            patch.lfoShape =
                static_cast<drumas::synth::LfoShape>(static_cast<int>(value));
            break;
        case ids::kLfoRate: patch.lfoRateHz = value; break;
        case ids::kLfoCutoffDepth: patch.lfoCutoffDepth = value; break;
        case ids::kLfoPitchDepthCents: patch.lfoPitchDepthCents = value; break;
        default: matched = false; break;
    }
}

[[nodiscard]] QString cleanCategory(const QString& category) {
    const QString upper = category.trimmed().toUpper();
    if (upper.isEmpty()) {
        return QStringLiteral("USER");
    }
    return upper.left(16);
}

}  // namespace

QString PatchFile::directory() {
    const QString override = qEnvironmentVariable("DRUMAS_PATCHES_PATH");
    if (!override.isEmpty()) {
        return override;
    }
    const QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty()) {
        return QDir::cleanPath(appData + QStringLiteral("/Drumaspro/patches"));
    }
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("patches"));
}

QString PatchFile::fileNameFor(const QString& name) {
    QString safe = name.trimmed().toLower();
    safe.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")),
                 QStringLiteral("-"));
    while (safe.startsWith(QLatin1Char('-'))) {
        safe.remove(0, 1);
    }
    while (safe.endsWith(QLatin1Char('-'))) {
        safe.chop(1);
    }
    if (safe.isEmpty()) {
        safe = QStringLiteral("patch");
    }
    return safe + QStringLiteral(".json");
}

QJsonObject PatchFile::toJson(const PatchDocument& document) {
    QJsonObject parameters;
    for (const Entry& entry : kEntries) {
        bool matched = false;
        const float value = valueOf(document.patch, entry.id, matched);
        if (matched) {
            parameters.insert(QString::fromLatin1(entry.name),
                              static_cast<double>(value));
        }
    }
    return QJsonObject{{QStringLiteral("schemaVersion"), kPatchSchemaVersion},
                       {QStringLiteral("name"), document.name},
                       {QStringLiteral("category"), cleanCategory(document.category)},
                       {QStringLiteral("params"), parameters}};
}

std::optional<PatchDocument> PatchFile::fromJson(const QJsonObject& object,
                                                 QString* error) {
    const int version =
        object.value(QStringLiteral("schemaVersion")).toInt(kPatchSchemaVersion);
    if (version > kPatchSchemaVersion) {
        if (error != nullptr) {
            *error = QStringLiteral("patch schemaVersion %1 is newer than %2")
                         .arg(version)
                         .arg(kPatchSchemaVersion);
        }
        return std::nullopt;
    }

    PatchDocument document;
    document.name = object.value(QStringLiteral("name")).toString().left(64);
    document.category =
        cleanCategory(object.value(QStringLiteral("category")).toString());

    const QJsonValue paramsValue = object.value(QStringLiteral("params"));
    if (!paramsValue.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("patch has no params object");
        }
        return std::nullopt;
    }

    int applied = 0;
    const QJsonObject parameters = paramsValue.toObject();
    for (auto iterator = parameters.constBegin(); iterator != parameters.constEnd();
         ++iterator) {
        if (!iterator.value().isDouble()) {
            continue;
        }
        const double number = iterator.value().toDouble();
        if (!std::isfinite(number)) {
            continue;
        }
        const ParameterId id = core::makeParameterId(iterator.key().toStdString());
        const core::ParameterDescriptor* descriptor = synthRegistry().find(id);
        if (descriptor == nullptr) {
            continue;  // unknown names are ignored, not fatal
        }
        float value = static_cast<float>(number);
        (void)synthRegistry().clampToRange(id, value);
        bool matched = false;
        setValue(document.patch, id, value, matched);
        if (matched) {
            ++applied;
        }
    }
    if (applied == 0) {
        if (error != nullptr) {
            *error = QStringLiteral("patch has no known parameters");
        }
        return std::nullopt;
    }
    return document;
}

bool PatchFile::save(const PatchDocument& document, const QString& filePath,
                     QString* error) {
    const QDir parent = QFileInfo(filePath).dir();
    if (!QDir().mkpath(parent.absolutePath())) {
        if (error != nullptr) {
            *error = QStringLiteral("patch directory cannot be created");
        }
        return false;
    }
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("patch file cannot be written");
        }
        return false;
    }
    const QByteArray payload =
        QJsonDocument(toJson(document)).toJson(QJsonDocument::Indented);
    if (file.write(payload) != payload.size() || !file.commit()) {
        if (error != nullptr) {
            *error = QStringLiteral("patch file could not be committed");
        }
        return false;
    }
    return true;
}

std::optional<PatchDocument> PatchFile::load(const QString& filePath,
                                             QString* error) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error != nullptr) {
            *error = QStringLiteral("patch file cannot be opened");
        }
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error != nullptr) {
            *error = QStringLiteral("patch file is not valid JSON");
        }
        return std::nullopt;
    }
    return fromJson(document.object(), error);
}

QStringList PatchFile::listFiles(const QString& directory) {
    const QDir dir = directory.isEmpty() ? QDir(PatchFile::directory())
                                         : QDir(directory);
    const QStringList names =
        dir.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QStringList paths;
    paths.reserve(names.size());
    for (const QString& name : names) {
        paths.append(dir.absoluteFilePath(name));
    }
    return paths;
}

}  // namespace drumas::ui
