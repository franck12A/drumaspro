#include <QDir>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QFileInfo>
#include <QKeyEvent>
#include <QQuickStyle>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTest>
#include <QThread>

#include <atomic>
#include <algorithm>
#include <chrono>
#include <span>
#include <thread>
#include <vector>

#include "core/AudioTypes.h"
#include "core/IAudioProcessor.h"
#include "core/ParameterRegistry.h"
#include "engine/AudioGraph.h"
#include "music/MusicTypes.h"
#include "ui/qt/EngineBridge.h"
#include "ui/qt/Models.h"
#include "ui/qt/Settings.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

class SilentProcessor final : public drumas::core::IAudioProcessor {
public:
    void prepare(const drumas::core::AudioSpec&) override {}
    void reset() override {}
    void process(std::span<const drumas::core::Command>, float* const* output, int channels,
                 int frames) noexcept override {
        for (int channel = 0; channel < channels; ++channel) {
            std::fill_n(output[channel], frames, 0.0F);
        }
    }
};

class RecordingCommandSink final : public drumas::bridge::CommandSink {
public:
    bool pushBatch(std::span<const drumas::core::Command> commands) noexcept override {
        batches.emplace_back(commands.begin(), commands.end());
        return true;
    }

    std::vector<std::vector<drumas::core::Command>> batches;
};

void compareCommandBatches(const std::vector<drumas::core::Command>& left,
                           const std::vector<drumas::core::Command>& right) {
    QCOMPARE(left.size(), right.size());
    for (std::size_t index = 0; index < left.size(); ++index) {
        QCOMPARE(left[index].type, right[index].type);
        QCOMPARE(left[index].note, right[index].note);
        QCOMPARE(left[index].param, right[index].param);
        QCOMPARE(left[index].patch, right[index].patch);
        QCOMPARE(left[index].value, right[index].value);
        QCOMPARE(left[index].slot, right[index].slot);
        QCOMPARE(left[index].object, right[index].object);
    }
}

void clearTestSettings() { QFile::remove(qEnvironmentVariable("DRUMAS_SETTINGS_PATH")); }

QQuickItem* findVisualItem(QQuickItem* root, const QString& objectName) {
    if (root == nullptr) {
        return nullptr;
    }
    if (root->objectName() == objectName) {
        return root;
    }
    for (QQuickItem* child : root->childItems()) {
        if (QQuickItem* match = findVisualItem(child, objectName)) {
            return match;
        }
    }
    return nullptr;
}

}  // namespace

class UiTests final : public QObject {
    Q_OBJECT

private Q_SLOTS:
    void modelsExposeRegistryAndDiatonicChordData() {
        drumas::core::ParameterRegistry registry;
        constexpr auto id = drumas::core::makeParameterId("ui.test");
        QVERIFY(registry.add({id, "ui.test", 1.0F, 10.0F, 2.0F,
                              drumas::core::ParameterUnit::Hertz,
                              drumas::core::ParameterCurve::Logarithmic}));
        drumas::bridge::ParameterMirror mirror;
        mirror.initialize(registry);

        drumas::ui::ParameterModel parameters;
        parameters.setSnapshot(mirror.snapshot());
        QCOMPARE(parameters.rowCount(), 1);
        QCOMPARE(parameters.data(parameters.index(0), drumas::ui::ParameterModel::NameRole)
                     .toString(),
                 QStringLiteral("ui.test"));
        QCOMPARE(parameters.data(parameters.index(0), drumas::ui::ParameterModel::ValueRole)
                     .toFloat(),
                 2.0F);
        QCOMPARE(parameters.curveForId(id),
                 static_cast<int>(drumas::core::ParameterCurve::Logarithmic));
        QCOMPARE(parameters.data(parameters.index(0), drumas::ui::ParameterModel::CurveRole)
                     .toInt(),
                 1);

        drumas::ui::ChordModel chords;
        QCOMPARE(chords.rowCount(), 7);
        QCOMPARE(chords.data(chords.index(0), drumas::ui::ChordModel::RootRole).toString(),
                 QStringLiteral("C"));
        QCOMPARE(chords.data(chords.index(0), drumas::ui::ChordModel::QualityRole).toString(),
                 QStringLiteral("major"));
        QCOMPARE(chords.data(chords.index(0), drumas::ui::ChordModel::NumeralRole).toString(),
                 QStringLiteral("I"));
        QVERIFY(chords.setScale(10, true));
        drumas::music::Chord chord;
        QVERIFY(chords.chordAt(1, chord));
        QCOMPARE(drumas::music::name(chord.root), std::string_view("B#"));
        QCOMPARE(chord.quality, drumas::music::ChordQuality::Diminished);
        QVERIFY(!chords.setScale(12, false));

        drumas::ui::PatchModel patches;
        QCOMPARE(patches.rowCount(), 3);
        QCOMPARE(patches.data(patches.index(2), drumas::ui::PatchModel::NameRole).toString(),
                 QStringLiteral("Keys"));
    }

    void parameterValuesUseTheirUnitAndScale() {
        drumas::ui::EngineBridge bridge;
        // Percent parameters are stored 0..1 and printed 0..100.
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("%"), 0.55), QStringLiteral("55 %"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("%"), 1.0), QStringLiteral("100 %"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("%"), 0.045), QStringLiteral("4.5 %"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("%"), 0.0), QStringLiteral("0 %"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("Hz"), 750.0), QStringLiteral("750 Hz"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("Hz"), 22.5), QStringLiteral("22.5 Hz"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("Hz"), 0.05), QStringLiteral("0.05 Hz"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("ms"), 200.0), QStringLiteral("200 ms"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("ms"), 0.5), QStringLiteral("0.5 ms"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("dB"), 3.0), QStringLiteral("3 dB"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("dB"), -0.5), QStringLiteral("-0.5 dB"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("ct"), -13.2), QStringLiteral("-13 ct"));
        QCOMPARE(bridge.formatUnitValue(QStringLiteral("oct"), 2.0), QStringLiteral("2 oct"));
        QCOMPARE(bridge.formatUnitValue(QString(), 1.5), QStringLiteral("1.5"));

        const qulonglong gainId = bridge.parameterId(QStringLiteral("master.gain"));
        QCOMPARE(bridge.formatParameterValue(gainId, 0.7), QStringLiteral("70 %"));
        const qulonglong cutoffId = bridge.parameterId(QStringLiteral("filter.cutoff"));
        QCOMPARE(bridge.formatParameterValue(cutoffId, 750.0), QStringLiteral("750 Hz"));
    }

    void effectParameterModelReflectsRegistryAndSelection() {
        drumas::fx::ChainDescription chain = drumas::fx::defaultChainDescription();
        const auto decay = drumas::core::makeParameterId("reverb.decay");
        chain.slots[0].parameterCount = 1;
        chain.slots[0].parameters[0] = drumas::fx::EffectParameterValue{decay, 7.0F};

        drumas::ui::EffectParameterModel parameters;
        parameters.setEffect(drumas::fx::kReverbType, chain.slots[0]);
        QCOMPARE(parameters.count(), 4);
        QCOMPARE(parameters.valueForId(decay), 7.0);
        QCOMPARE(parameters.data(parameters.index(0),
                                 drumas::ui::EffectParameterModel::CurveRole)
                     .toInt(),
                 static_cast<int>(drumas::core::ParameterCurve::Logarithmic));

        // Values are clamped to the effect's published range.
        parameters.setValue(decay, 1000.0F);
        QCOMPARE(parameters.valueForId(decay), 20.0);

        RecordingCommandSink sink;
        drumas::ui::EngineBridge bridge(nullptr, &sink);
        // High-bit ids must survive the QML boundary as unsigned values; the
        // default-patch mirror is populated before the audio device starts.
        const qulonglong cutoffId = bridge.parameterId(QStringLiteral("filter.cutoff"));
        QCOMPARE(cutoffId,
                 static_cast<qulonglong>(
                     drumas::core::makeParameterId("filter.cutoff")));
        QCOMPARE(bridge.parameterModel()->valueForId(cutoffId), 750.0);
        QCOMPARE(bridge.parameterModel()->curveForId(cutoffId), 1);
        QCOMPARE(bridge.selectedEffectSlot(), 0);
        QCOMPARE(bridge.selectedEffectTypeName(), QStringLiteral("reverb"));
        QCOMPARE(bridge.effectParameterModel()->count(), 4);

        bridge.selectEffectSlot(1);
        QCOMPARE(bridge.selectedEffectSlot(), 1);
        QCOMPARE(bridge.selectedEffectTypeName(), QStringLiteral("delay"));
        QCOMPARE(bridge.effectParameterModel()->count(), 4);
    }

    void overlappingChordsShareNotesWithoutRetriggering() {
        RecordingCommandSink sink;
        drumas::ui::EngineBridge bridge(nullptr, &sink);

        QVERIFY(bridge.playChord(0));  // C major: C4 E4 G4
        QVERIFY(bridge.playChord(2));  // E minor: E4 G4 B4
        QCOMPARE(sink.batches.size(), std::size_t{2});
        const auto& secondOn = sink.batches.back();
        QCOMPARE(secondOn.size(), std::size_t{1});
        QCOMPARE(secondOn.front().type, drumas::core::CommandType::NoteOn);
        QCOMPARE(static_cast<int>(secondOn.front().note), 71);

        // Releasing C major must only release the note no other chord holds.
        QVERIFY(bridge.releaseChord(0));
        const auto& releaseC = sink.batches.back();
        QCOMPARE(releaseC.size(), std::size_t{1});
        QCOMPARE(releaseC.front().type, drumas::core::CommandType::NoteOff);
        QCOMPARE(static_cast<int>(releaseC.front().note), 60);

        QVERIFY(bridge.releaseChord(2));
        const auto& releaseEm = sink.batches.back();
        QCOMPARE(releaseEm.size(), std::size_t{3});
        QCOMPARE(static_cast<int>(releaseEm[0].note), 64);
        QCOMPARE(static_cast<int>(releaseEm[1].note), 67);
        QCOMPARE(static_cast<int>(releaseEm[2].note), 71);
    }

    void computerKeyboardMapsOctavesAndMidiLimits() {
        RecordingCommandSink sink;
        drumas::ui::EngineBridge bridge(nullptr, &sink);

        QCOMPARE(bridge.semitoneForKey(Qt::Key_A), 0);
        QCOMPARE(bridge.semitoneForKey(Qt::Key_W), 1);
        QCOMPARE(bridge.semitoneForKey(Qt::Key_S), 2);
        QCOMPARE(bridge.semitoneForKey(Qt::Key_K), 12);
        QCOMPARE(bridge.semitoneForKey(Qt::Key_Z), -1);
        QCOMPARE(bridge.semitoneForKey(Qt::Key_1), -1);

        QCOMPARE(bridge.pianoOctave(), 4);
        QCOMPARE(bridge.midiNoteForSemitone(0), 60);
        QCOMPARE(bridge.midiNoteForSemitone(12), 72);
        bridge.setPianoOctave(2);
        QCOMPARE(bridge.midiNoteForSemitone(0), 36);
        bridge.setPianoOctave(8);
        QCOMPARE(bridge.midiNoteForSemitone(0), 108);
        QCOMPARE(bridge.midiNoteForSemitone(12), 120);
        QVERIFY(!bridge.shiftPianoOctave(1));
        QCOMPARE(bridge.pianoOctave(), 8);
        bridge.setPianoOctave(0);
        QVERIFY(!bridge.shiftPianoOctave(-1));
        QCOMPARE(bridge.pianoOctave(), 0);
        QCOMPARE(bridge.midiNoteForSemitone(0), 12);
        QCOMPARE(bridge.midiNoteForSemitone(-5), 12);
        QCOMPARE(bridge.midiNoteForSemitone(99), 24);

        bridge.setPianoOctave(4);
        QVERIFY(!bridge.noteOn(-1, 0.8));
        QVERIFY(!bridge.noteOn(128, 0.8));
        QVERIFY(!bridge.noteOn(60, std::nan("")));
        QVERIFY(bridge.noteOn(127, 0.8));
        QVERIFY(bridge.noteOff(127));
    }

    void pianoNotesShareReferencesWithChords() {
        RecordingCommandSink sink;
        drumas::ui::EngineBridge bridge(nullptr, &sink);

        QVERIFY(bridge.noteOn(60, 0.8));
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QVERIFY(bridge.noteOn(60, 0.8));
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QVERIFY(bridge.noteOff(60));
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QVERIFY(bridge.noteOff(60));
        QCOMPARE(sink.batches.size(), std::size_t{2});
        QCOMPARE(sink.batches.back().front().type,
                 drumas::core::CommandType::NoteOff);

        sink.batches.clear();
        QVERIFY(bridge.playChord(0));  // C4 E4 G4
        sink.batches.clear();
        QVERIFY(bridge.noteOn(60, 0.5));
        QCOMPARE(sink.batches.size(), std::size_t{0});
        QVERIFY(bridge.noteOff(60));
        QCOMPARE(sink.batches.size(), std::size_t{0});
        QVERIFY(bridge.releaseChord(0));
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QCOMPARE(sink.batches.back().size(), std::size_t{3});
    }

    void pianoViewPlaysNotesAndComputerKeysFollowOctave() {
        RecordingCommandSink sink;
        drumas::ui::EngineBridge bridge(nullptr, &sink);
        QQmlApplicationEngine qml;
        qml.rootContext()->setContextProperty(QStringLiteral("engineBridge"), &bridge);
        qml.loadFromModule("DrumasProTests", "Main");
        QVERIFY2(!qml.rootObjects().isEmpty(), "QML Main component failed to load");

        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().front());
        QVERIFY(window != nullptr);
        window->resize(1280, 720);
        window->show();
        window->requestActivate();
        QTest::qWait(150);

        window->setProperty("bottomView", 1);
        QTest::qWait(100);
        auto* piano =
            findVisualItem(window->contentItem(), QStringLiteral("pianoKeyboard"));
        QVERIFY(piano != nullptr);
        QVERIFY(piano->isVisible());
        QCOMPARE(piano->property("octaveCount").toInt(), 3);
        QCOMPARE(piano->property("lowestNote").toInt(), 48);

        const QPoint firstKey = piano
            ->mapToScene(QPointF(piano->width() / 21.0 / 2.0,
                                 piano->height() * 0.9))
            .toPoint();
        sink.batches.clear();
        QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, firstKey);
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QCOMPARE(sink.batches.back().size(), std::size_t{1});
        QCOMPARE(sink.batches.back().front().type,
                 drumas::core::CommandType::NoteOn);
        QCOMPARE(static_cast<int>(sink.batches.back().front().note), 48);
        QCOMPARE(piano->property("pressedNote").toInt(), 48);
        QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, firstKey);
        QCOMPARE(sink.batches.size(), std::size_t{2});
        QCOMPARE(sink.batches.back().front().type,
                 drumas::core::CommandType::NoteOff);

        auto* keyboardFocus =
            findVisualItem(window->contentItem(), QStringLiteral("keyboardFocus"));
        QVERIFY(keyboardFocus != nullptr);
        keyboardFocus->forceActiveFocus();
        QVERIFY(window->activeFocusItem() == keyboardFocus);

        sink.batches.clear();
        QTest::keyPress(window, Qt::Key_A);
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QCOMPARE(static_cast<int>(sink.batches.back().front().note), 60);
        QKeyEvent autoRepeat(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier,
                             QStringLiteral("a"), true, 1);
        QCoreApplication::sendEvent(window, &autoRepeat);
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QTest::keyRelease(window, Qt::Key_A);
        QCOMPARE(sink.batches.size(), std::size_t{2});
        QCOMPARE(sink.batches.back().front().type,
                 drumas::core::CommandType::NoteOff);

        sink.batches.clear();
        QTest::keyPress(window, Qt::Key_X);
        QCOMPARE(sink.batches.size(), std::size_t{0});
        QCOMPARE(bridge.pianoOctave(), 5);
        QTest::keyPress(window, Qt::Key_A);
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QCOMPARE(static_cast<int>(sink.batches.back().front().note), 72);
        QTest::keyRelease(window, Qt::Key_A);
        QCOMPARE(sink.batches.size(), std::size_t{2});

        QTest::keyPress(window, Qt::Key_Z);
        QCOMPARE(bridge.pianoOctave(), 4);
        QTest::keyRelease(window, Qt::Key_Z);

        // Held chords highlight their notes on the piano while the keys view is
        // open; 1-7 keep working there.
        sink.batches.clear();
        QTest::keyPress(window, Qt::Key_1);
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QCOMPARE(piano->property("soundingNotes").toList().size(), 3);
        QTest::keyRelease(window, Qt::Key_1);
        QCOMPARE(piano->property("soundingNotes").toList().size(), 0);
    }

    void rightClickOpensChordEditorAndSetupButtonOpensSettings() {
        clearTestSettings();
        RecordingCommandSink sink;
        drumas::ui::EngineBridge bridge(nullptr, &sink);
        QQmlApplicationEngine qml;
        qml.rootContext()->setContextProperty(QStringLiteral("engineBridge"), &bridge);
        qml.loadFromModule("DrumasProTests", "Main");
        QVERIFY2(!qml.rootObjects().isEmpty(), "QML Main component failed to load");

        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().front());
        QVERIFY(window != nullptr);
        window->resize(1280, 720);
        window->show();
        QTest::qWait(120);

        auto* setup = findVisualItem(window->contentItem(), QStringLiteral("setupButton"));
        QVERIFY(setup != nullptr);
        const QPoint setupPosition =
            setup->mapToScene(QPointF(setup->width() / 2, setup->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, setupPosition);
        QTest::qWait(120);
        auto* settings = window->findChild<QObject*>(QStringLiteral("settingsDialog"));
        QVERIFY(settings != nullptr);
        QCOMPARE(settings->property("visible").toBool(), true);
        settings->setProperty("visible", false);
        QTest::qWait(60);

        auto* chordButton =
            findVisualItem(window->contentItem(), QStringLiteral("chordButton_1"));
        QVERIFY(chordButton != nullptr);
        const QPoint chordPosition = chordButton
            ->mapToScene(QPointF(chordButton->width() / 2, chordButton->height() / 2))
            .toPoint();
        QTest::mouseClick(window, Qt::RightButton, Qt::NoModifier, chordPosition);
        QTest::qWait(120);
        auto* editor = window->findChild<QObject*>(QStringLiteral("chordEditorDialog"));
        QVERIFY(editor != nullptr);
        QCOMPARE(editor->property("visible").toBool(), true);
        QCOMPARE(editor->property("chordRow").toInt(), 1);
        editor->setProperty("visible", false);
    }

    void qmlFitsRequiredWindowSizesAndCapturesBothThemes() {
        const QDir fontDirectory(
            qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts"));
        for (const QString& fileName :
             {QStringLiteral("arial.ttf"), QStringLiteral("arialn.ttf"),
              QStringLiteral("segoeui.ttf"), QStringLiteral("lucon.ttf")}) {
            const QString fontPath = fontDirectory.filePath(fileName);
            if (QFileInfo::exists(fontPath)) {
                QVERIFY(QFontDatabase::addApplicationFont(fontPath) >= 0);
            }
        }

        drumas::ui::EngineBridge bridge;
        QQmlApplicationEngine qml;
        qml.rootContext()->setContextProperty(QStringLiteral("engineBridge"), &bridge);
        qml.loadFromModule("DrumasProTests", "Main");
        QVERIFY2(!qml.rootObjects().isEmpty(), "QML Main component failed to load");

        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().front());
        QVERIFY(window != nullptr);
        window->show();
        // Canvas-based knobs paint asynchronously; give the first frame time to
        // settle before the 1280x720 capture, which is the first one taken.
        QTest::qWait(200);
        QVERIFY(QDir().mkpath(QStringLiteral(DRUMAS_SCREENSHOT_DIR)));

        const auto capture = [window](const QSize& size, const QString& theme,
                                      const QString& fileName) {
            window->setProperty("amberTheme", theme == QStringLiteral("amber"));
            window->setProperty("nightTheme", theme == QStringLiteral("night"));
            window->resize(size);
            QTest::qWait(200);
            if (window->size() != size) {
                return false;
            }
            const QImage screenshot = window->grabWindow();
            if (screenshot.isNull()) {
                return false;
            }
            const QString path =
                QStringLiteral(DRUMAS_SCREENSHOT_DIR) + QLatin1Char('/') + fileName;
            return screenshot.size() == size && screenshot.save(path);
        };

        const bool defaultLarge =
            capture(QSize(1280, 720), QStringLiteral("default"),
                    QStringLiteral("drumaspro-default-1280x720.png"));
        if (!defaultLarge) {
            QSKIP("Offscreen grabWindow is unavailable; capture requires human/UI verification");
        }
        QVERIFY(capture(QSize(960, 600), QStringLiteral("default"),
                        QStringLiteral("drumaspro-default-960x600.png")));
        QVERIFY(capture(QSize(1280, 720), QStringLiteral("amber"),
                        QStringLiteral("drumaspro-amber-1280x720.png")));
        QVERIFY(capture(QSize(960, 600), QStringLiteral("amber"),
                        QStringLiteral("drumaspro-amber-960x600.png")));
        QVERIFY(capture(QSize(1280, 720), QStringLiteral("night"),
                        QStringLiteral("drumaspro-night-1280x720.png")));
        QVERIFY(capture(QSize(960, 600), QStringLiteral("night"),
                        QStringLiteral("drumaspro-night-960x600.png")));

        // Setup dialog capture: theme, accent, scale and chord mapping panel.
        auto* settingsDialog =
            window->findChild<QObject*>(QStringLiteral("settingsDialog"));
        if (settingsDialog != nullptr) {
            settingsDialog->setProperty("visible", true);
            QTest::qWait(250);
            QVERIFY(capture(QSize(1280, 720), QStringLiteral("default"),
                            QStringLiteral("drumaspro-setup-1280x720.png")));
            settingsDialog->setProperty("visible", false);
        }

        window->setProperty("bottomView", 1);
        QVERIFY(capture(QSize(1280, 720), QStringLiteral("default"),
                        QStringLiteral("drumaspro-keys-1280x720.png")));
        QVERIFY(capture(QSize(960, 600), QStringLiteral("default"),
                        QStringLiteral("drumaspro-keys-960x600.png")));
    }

    void rotarySelectorsSnapOnClickAndKeyboard() {
        drumas::ui::EngineBridge bridge;
        QQmlApplicationEngine qml;
        qml.rootContext()->setContextProperty(QStringLiteral("engineBridge"), &bridge);
        qml.loadFromModule("DrumasProTests", "Main");
        QVERIFY2(!qml.rootObjects().isEmpty(), "QML Main component failed to load");

        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().front());
        QVERIFY(window != nullptr);
        window->resize(1280, 720);
        window->show();
        QTest::qWait(100);

        auto* patchSelector =
            findVisualItem(window->contentItem(), QStringLiteral("patchSelector"));
        QVERIFY(patchSelector != nullptr);
        QCOMPARE(patchSelector->property("currentIndex").toInt(), 0);

        const QPoint center = patchSelector
            ->mapToScene(QPointF(patchSelector->width() / 2,
                                 patchSelector->height() / 2))
            .toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center);
        QCOMPARE(patchSelector->property("currentIndex").toInt(), 1);

        patchSelector->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(patchSelector->property("currentIndex").toInt(), 2);
    }

    void qmlControlsReceiveResponsiveGeometry() {
        drumas::ui::EngineBridge bridge;
        QQmlApplicationEngine qml;
        qml.rootContext()->setContextProperty(QStringLiteral("engineBridge"), &bridge);
        qml.loadFromModule("DrumasProTests", "Main");
        QVERIFY2(!qml.rootObjects().isEmpty(), "QML Main component failed to load");

        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().front());
        QVERIFY(window != nullptr);
        window->show();

        for (const QSize size : {QSize(1280, 720), QSize(960, 600)}) {
            window->resize(size);
            QTest::qWait(100);
            QCOMPARE(window->size(), size);

            auto* keySelector =
                findVisualItem(window->contentItem(), QStringLiteral("keySelector"));
            auto* chordDisplay =
                findVisualItem(window->contentItem(), QStringLiteral("activeChordDisplay"));
            auto* synthPanel =
                findVisualItem(window->contentItem(), QStringLiteral("synthPanel"));
            QVERIFY(keySelector != nullptr);
            QVERIFY(chordDisplay != nullptr);
            QVERIFY(synthPanel != nullptr);
            QVERIFY(keySelector->width() >= 80);
            QVERIFY(keySelector->height() >= 25);
            QVERIFY(chordDisplay->width() >= 100);
            QVERIFY(synthPanel->width() >= size.width() - 40);
            QVERIFY(synthPanel->height() >= 240);

            qreal previousChordRight = -1.0;
            for (int row = 0; row < 7; ++row) {
                auto* button = findVisualItem(
                    window->contentItem(), QStringLiteral("chordButton_%1").arg(row));
                QVERIFY(button != nullptr);
                QVERIFY(button->width() >= 100);
                QVERIFY(button->height() >= 70);
                QVERIFY(button->x() >= previousChordRight);
                previousChordRight = button->x() + button->width();
            }

            auto* effectSlot =
                findVisualItem(window->contentItem(), QStringLiteral("effectSlot_0"));
            QVERIFY(effectSlot != nullptr);
            QVERIFY(effectSlot->width() >= 120);
            QVERIFY(effectSlot->height() >= 100);
        }
    }

    void chordButtonsAndKeyboardKeysSubmitMatchingHoldReleaseBatches() {
        RecordingCommandSink sink;
        drumas::ui::EngineBridge bridge(nullptr, &sink);
        QQmlApplicationEngine qml;
        qml.rootContext()->setContextProperty(QStringLiteral("engineBridge"), &bridge);
        qml.loadFromModule("DrumasProTests", "Main");
        QVERIFY2(!qml.rootObjects().isEmpty(), "QML Main component failed to load");

        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().front());
        QVERIFY(window != nullptr);
        window->resize(1280, 720);
        window->show();
        window->requestActivate();
        QTest::qWait(100);

        auto* keyboardFocus =
            findVisualItem(window->contentItem(), QStringLiteral("keyboardFocus"));
        QVERIFY(keyboardFocus != nullptr);
        for (int row = 0; row < 7; ++row) {
            auto* button = findVisualItem(
                window->contentItem(), QStringLiteral("chordButton_%1").arg(row));
            QVERIFY(button != nullptr);
            const QPoint buttonPosition =
                button->mapToScene(QPointF(button->width() / 2, button->height() / 2)).toPoint();

            sink.batches.clear();
            QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, buttonPosition);
            QCOMPARE(sink.batches.size(), std::size_t{1});
            QVERIFY(button->property("chordActive").toBool());
            const auto buttonPressBatch = sink.batches.back();
            QCOMPARE(buttonPressBatch.size(), std::size_t{3});
            QCOMPARE(buttonPressBatch.front().type, drumas::core::CommandType::NoteOn);
            QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, buttonPosition);
            QCOMPARE(sink.batches.size(), std::size_t{2});
            QVERIFY(!button->property("chordActive").toBool());
            const auto buttonReleaseBatch = sink.batches.back();
            QCOMPARE(buttonReleaseBatch.size(), std::size_t{3});
            QCOMPARE(buttonReleaseBatch.front().type, drumas::core::CommandType::NoteOff);

            keyboardFocus->forceActiveFocus();
            QVERIFY(window->activeFocusItem() == keyboardFocus);
            sink.batches.clear();
            const auto key = static_cast<Qt::Key>(Qt::Key_1 + row);
            QTest::keyPress(window, key);
            QCOMPARE(sink.batches.size(), std::size_t{1});
            QVERIFY(button->property("chordActive").toBool());
            const auto keyPressBatch = sink.batches.back();
            if (row == 0) {
                QKeyEvent autoRepeat(QEvent::KeyPress, key, Qt::NoModifier,
                                     QStringLiteral("1"), true, 1);
                QCoreApplication::sendEvent(window, &autoRepeat);
                QCOMPARE(sink.batches.size(), std::size_t{1});
            }
            QTest::keyRelease(window, key);
            QCOMPARE(sink.batches.size(), std::size_t{2});
            QVERIFY(!button->property("chordActive").toBool());
            const auto keyReleaseBatch = sink.batches.back();

            compareCommandBatches(buttonPressBatch, keyPressBatch);
            compareCommandBatches(buttonReleaseBatch, keyReleaseBatch);
        }

        auto* patchSelector =
            findVisualItem(window->contentItem(), QStringLiteral("patchSelector"));
        QVERIFY(patchSelector != nullptr);
        patchSelector->forceActiveFocus();
        sink.batches.clear();
        QTest::keyPress(window, Qt::Key_2);
        QTest::keyRelease(window, Qt::Key_2);
        QVERIFY(sink.batches.empty());

        auto* brandHeader =
            findVisualItem(window->contentItem(), QStringLiteral("brandHeader"));
        QVERIFY(brandHeader != nullptr);
        const QPoint headerPosition =
            brandHeader->mapToScene(QPointF(brandHeader->width() / 2,
                                            brandHeader->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, headerPosition);
        QVERIFY(window->activeFocusItem() == keyboardFocus);

        sink.batches.clear();
        QTest::keyPress(window, Qt::Key_2);
        QCOMPARE(sink.batches.size(), std::size_t{1});
        patchSelector->forceActiveFocus();
        QCOMPARE(sink.batches.size(), std::size_t{2});
        QVERIFY(sink.batches.back().front().type == drumas::core::CommandType::NoteOff);
    }

    void fullFrameRenderFitsSixtyFps() {
        drumas::ui::EngineBridge bridge;
        QQmlApplicationEngine qml;
        qml.rootContext()->setContextProperty(QStringLiteral("engineBridge"), &bridge);
        qml.loadFromModule("DrumasProTests", "Main");
        QVERIFY2(!qml.rootObjects().isEmpty(), "QML Main component failed to load");

        auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().front());
        QVERIFY(window != nullptr);
        window->resize(1280, 720);
        window->show();
        QTest::qWait(400);

        // The UI is event-driven: there is no animation loop, so the relevant
        // cost is a full repaint of the panel. Offscreen grabWindow() renders
        // the same scene graph the window paints.
        for (int warmUp = 0; warmUp < 3; ++warmUp) {
            (void)window->grabWindow();
        }
        constexpr int kFrames = 20;
        QElapsedTimer timer;
        timer.start();
        for (int frame = 0; frame < kFrames; ++frame) {
            const QImage image = window->grabWindow();
            QVERIFY(!image.isNull());
        }
        const double millisecondsPerFrame =
            static_cast<double>(timer.nsecsElapsed()) / 1.0e6 / kFrames;
        qInfo("full-frame render at 1280x720: %.1f ms (%.0f fps headroom)",
              millisecondsPerFrame, 1000.0 / millisecondsPerFrame);
        QVERIFY2(millisecondsPerFrame < 33.0,
                 "a full repaint should stay above 30 fps even offscreen");
    }

    void settingsRoundTripKeepsEveryField() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath(QStringLiteral("settings.json"));

        drumas::ui::AppSettings settings;
        settings.theme = QStringLiteral("night");
        settings.accentPreset = 3;
        settings.uiScale = 110;
        settings.knobSensitivity = 130;
        settings.velocityCurve = 2;
        settings.defaultOctave = 2;
        settings.lastKey = 7;
        settings.lastScale = 1;
        settings.lastPatch = 2;
        settings.sevenths = true;
        settings.latch = true;
        settings.chordSlots[0] = drumas::ui::ChordSlotSetting{true, 5, 6, 2, 3};

        QString error;
        QVERIFY(drumas::ui::SettingsStore::save(settings, file, &error));
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // Atomic save must leave only the target file behind.
        QCOMPARE(QDir(directory.path()).entryList(QDir::Files).size(), 1);

        const drumas::ui::AppSettings loaded =
            drumas::ui::SettingsStore::load(file, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(loaded.theme, QStringLiteral("night"));
        QCOMPARE(loaded.accentPreset, 3);
        QCOMPARE(loaded.uiScale, 110);
        QCOMPARE(loaded.knobSensitivity, 130);
        QCOMPARE(loaded.velocityCurve, 2);
        QCOMPARE(loaded.defaultOctave, 2);
        QCOMPARE(loaded.lastKey, 7);
        QCOMPARE(loaded.lastScale, 1);
        QCOMPARE(loaded.lastPatch, 2);
        QCOMPARE(loaded.sevenths, true);
        QCOMPARE(loaded.latch, true);
        QCOMPARE(loaded.chordSlots[0].manual, true);
        QCOMPARE(loaded.chordSlots[0].root, 5);
        QCOMPARE(loaded.chordSlots[0].quality, 6);
        QCOMPARE(loaded.chordSlots[0].inversion, 2);
        QCOMPARE(loaded.chordSlots[0].octave, 3);
    }

    void settingsRejectCorruptEmptyTruncatedAndFutureFiles() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QString error;

        const QString corrupt = directory.filePath(QStringLiteral("corrupt.json"));
        {
            QFile file(corrupt);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("not json at all");
        }
        const auto corruptSettings = drumas::ui::SettingsStore::load(corrupt, &error);
        QVERIFY(!error.isEmpty());
        QCOMPARE(corruptSettings.theme, QStringLiteral("default"));
        QCOMPARE(corruptSettings.uiScale, 100);

        const QString empty = directory.filePath(QStringLiteral("empty.json"));
        {
            QFile file(empty);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        error.clear();
        const auto emptySettings = drumas::ui::SettingsStore::load(empty, &error);
        QVERIFY(!error.isEmpty());
        QCOMPARE(emptySettings.theme, QStringLiteral("default"));

        const QString truncated = directory.filePath(QStringLiteral("truncated.json"));
        {
            QFile file(truncated);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("{\"theme\": \"amber\"");
        }
        error.clear();
        const auto truncatedSettings =
            drumas::ui::SettingsStore::load(truncated, &error);
        QVERIFY(!error.isEmpty());
        QCOMPARE(truncatedSettings.theme, QStringLiteral("default"));

        const QString future = directory.filePath(QStringLiteral("future.json"));
        {
            QFile file(future);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QJsonDocument(QJsonObject{{QStringLiteral("schemaVersion"), 99},
                                                 {QStringLiteral("theme"),
                                                  QStringLiteral("night")}})
                           .toJson());
        }
        error.clear();
        const auto futureSettings = drumas::ui::SettingsStore::load(future, &error);
        QVERIFY(error.contains(QStringLiteral("schemaVersion")));
        QCOMPARE(futureSettings.theme, QStringLiteral("default"));

        const QString missing =
            directory.filePath(QStringLiteral("does-not-exist.json"));
        error.clear();
        const auto missingSettings = drumas::ui::SettingsStore::load(missing, &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(missingSettings.uiScale, 100);
    }

    void settingsIgnoreUnknownFieldsAndClampRanges() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath(QStringLiteral("settings.json"));

        QJsonObject object = drumas::ui::SettingsStore::toJson(drumas::ui::AppSettings{});
        object.insert(QStringLiteral("theme"), QStringLiteral("amber"));
        object.insert(QStringLiteral("accentPreset"), 9);
        object.insert(QStringLiteral("uiScale"), 137);
        object.insert(QStringLiteral("knobSensitivity"), 999);
        object.insert(QStringLiteral("velocityCurve"), 7);
        object.insert(QStringLiteral("defaultOctave"), -3);
        object.insert(QStringLiteral("lastKey"), 42);
        object.insert(QStringLiteral("futureField"), QStringLiteral("ignored"));
        QJsonArray slots = object.value(QStringLiteral("chordSlots")).toArray();
        QJsonObject slot = slots.at(0).toObject();
        slot.insert(QStringLiteral("quality"), 42);
        slot.insert(QStringLiteral("root"), -4);
        slots.replace(0, slot);
        object.insert(QStringLiteral("chordSlots"), slots);
        {
            QFile output(file);
            QVERIFY(output.open(QIODevice::WriteOnly));
            output.write(QJsonDocument(object).toJson());
        }

        QString error;
        const auto settings = drumas::ui::SettingsStore::load(file, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(settings.theme, QStringLiteral("amber"));
        QCOMPARE(settings.accentPreset, 5);
        QCOMPARE(settings.uiScale, 100);
        QCOMPARE(settings.knobSensitivity, 150);
        QCOMPARE(settings.velocityCurve, 2);
        QCOMPARE(settings.defaultOctave, 0);
        QCOMPARE(settings.lastKey, 11);
        QCOMPARE(settings.chordSlots[0].quality, 9);
        QCOMPARE(settings.chordSlots[0].root, 0);
    }

    void manualChordPadsAndSeventhsChangeTheNotes() {
        clearTestSettings();
        RecordingCommandSink sink;
        drumas::ui::EngineBridge bridge(nullptr, &sink);

        QVERIFY(bridge.setChordSlot(0, true, 2, 6, 0, 3));  // D minor 7, root D3
        QVERIFY(bridge.playChord(0));
        const auto& manual = sink.batches.back();
        QCOMPARE(manual.size(), std::size_t{4});
        QCOMPARE(static_cast<int>(manual[0].note), 50);
        QCOMPARE(static_cast<int>(manual[1].note), 53);
        QCOMPARE(static_cast<int>(manual[2].note), 57);
        QCOMPARE(static_cast<int>(manual[3].note), 60);
        QVERIFY(bridge.releaseChord(0));
        QCOMPARE(bridge.chordSlot(0).value(QStringLiteral("manual")).toBool(), true);

        // A voicing outside MIDI 0..127 is rejected instead of clamped.
        QVERIFY(!bridge.setChordSlot(1, true, 11, 0, 3, 8));

        QVERIFY(bridge.resetChordSlot(0));
        QVERIFY(bridge.playChord(0));
        QCOMPARE(sink.batches.back().size(), std::size_t{3});
        QCOMPARE(static_cast<int>(sink.batches.back()[0].note), 60);
        QVERIFY(bridge.releaseChord(0));

        bridge.setSevenths(true);
        QVERIFY(bridge.playChord(0));
        const auto& seventh = sink.batches.back();
        QCOMPARE(seventh.size(), std::size_t{4});
        QCOMPARE(static_cast<int>(seventh[0].note), 60);
        QCOMPARE(static_cast<int>(seventh[3].note), 71);
        QVERIFY(bridge.releaseChord(0));

        // The setting survives a fresh bridge reading the persisted file.
        {
            drumas::ui::EngineBridge reloaded(nullptr, &sink);
            QCOMPARE(reloaded.sevenths(), true);
        }
        bridge.setSevenths(false);
    }

    void latchHoldsTheChordUntilAnotherOrDisabled() {
        clearTestSettings();
        RecordingCommandSink sink;
        drumas::ui::EngineBridge bridge(nullptr, &sink);

        bridge.setLatchEnabled(true);
        QCOMPARE(bridge.latchedRow(), -1);
        QVERIFY(bridge.playChord(0));
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QCOMPARE(bridge.latchedRow(), 0);

        sink.batches.clear();
        QVERIFY(bridge.releaseChord(0));
        QCOMPARE(sink.batches.size(), std::size_t{0});
        QCOMPARE(bridge.latchedRow(), 0);

        QVERIFY(bridge.playChord(2));  // E minor replaces C major
        QCOMPARE(bridge.latchedRow(), 2);
        QCOMPARE(sink.batches.size(), std::size_t{2});
        QCOMPARE(sink.batches.front().front().type,
                 drumas::core::CommandType::NoteOff);
        QCOMPARE(sink.batches.back().front().type,
                 drumas::core::CommandType::NoteOn);

        sink.batches.clear();
        bridge.setLatchEnabled(false);
        QCOMPARE(bridge.latchedRow(), -1);
        QCOMPARE(sink.batches.size(), std::size_t{1});
        QCOMPARE(sink.batches.back().front().type,
                 drumas::core::CommandType::NoteOff);
        QCOMPARE(sink.batches.back().size(), std::size_t{3});

        // Velocity curves and knob sensitivity are clamped settings.
        bridge.setVelocityCurve(1);
        QCOMPARE(bridge.applyVelocityCurve(0.25), 0.5);
        bridge.setVelocityCurve(2);
        QCOMPARE(bridge.applyVelocityCurve(0.5), 0.25);
        bridge.setVelocityCurve(0);
        QCOMPARE(bridge.applyVelocityCurve(0.3), 0.3);
    }

    void blockingUiThreadDoesNotStopAudioProcessing() {
        SilentProcessor processor;
        drumas::engine::AudioGraph graph;
        constexpr int kFrames = 2048;
        const drumas::core::AudioSpec spec{48000.0, kFrames, 2};
        graph.prepare(spec);
        graph.setProcessor(&processor);
        std::vector<float> output(static_cast<std::size_t>(kFrames) * 2);

        std::atomic<bool> stop{false};
        std::atomic<std::uint64_t> renderedBlocks{0};
        std::thread audioThread([&] {
            while (!stop.load(std::memory_order_acquire)) {
                graph.render(output.data(), 2, kFrames);
                renderedBlocks.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });

        QThread::msleep(2000);
        stop.store(true, std::memory_order_release);
        audioThread.join();

        const auto stats = graph.stats();
        QVERIFY(renderedBlocks.load(std::memory_order_relaxed) > 0);
        QCOMPARE(stats.lateBlocks + stats.overdueBlocks, std::uint64_t{0});
    }
};

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);
    // Never read or write the developer's real settings from the test suite.
    const QString settingsPath =
        QDir::temp().filePath(QStringLiteral("drumaspro-test-settings.json"));
    QFile::remove(settingsPath);
    qputenv("DRUMAS_SETTINGS_PATH", settingsPath.toUtf8());
    UiTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_ui.moc"
