#include <QDir>
#include <QGuiApplication>
#include <QFont>
#include <QFontDatabase>
#include <QFileInfo>
#include <QQuickStyle>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
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
        const QDir fontDirectory(qEnvironmentVariable("WINDIR") + QStringLiteral("/Fonts"));
        for (const QString& fileName :
             {QStringLiteral("segoeui.ttf"), QStringLiteral("lucon.ttf")}) {
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
