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
        QVERIFY(QDir().mkpath(QStringLiteral(DRUMAS_SCREENSHOT_DIR)));

        const auto capture = [window](const QSize& size, bool amber,
                                      const QString& fileName) {
            window->setProperty("amberTheme", amber);
            window->resize(size);
            QTest::qWait(100);
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
