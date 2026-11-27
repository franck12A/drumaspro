#include <QDir>
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

        const auto capture = [window](const QSize& size, bool amber,
                                      const QString& fileName) {
            window->setProperty("amberTheme", amber);
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
            capture(QSize(1280, 720), false, QStringLiteral("drumaspro-default-1280x720.png"));
        if (!defaultLarge) {
            QSKIP("Offscreen grabWindow is unavailable; capture requires human/UI verification");
        }
        QVERIFY(capture(QSize(960, 600), false,
                        QStringLiteral("drumaspro-default-960x600.png")));
        QVERIFY(capture(QSize(1280, 720), true,
                        QStringLiteral("drumaspro-amber-1280x720.png")));
        QVERIFY(capture(QSize(960, 600), true,
                        QStringLiteral("drumaspro-amber-960x600.png")));
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
    UiTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "test_ui.moc"
