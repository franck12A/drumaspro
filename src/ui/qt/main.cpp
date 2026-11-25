#include <QGuiApplication>
#include <QSize>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>
#include <QQuickWindow>

#include "EngineBridge.h"

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QGuiApplication application(argc, argv);
    drumas::ui::EngineBridge bridge;
    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty(QStringLiteral("engineBridge"), &bridge);
    if (!bridge.start()) {
        qWarning() << "Audio engine failed to start:" << bridge.error();
    }
    qml.loadFromModule("DrumasPro", "Main");
    if (qml.rootObjects().isEmpty()) {
        return 1;
    }
    auto* window = qobject_cast<QQuickWindow*>(qml.rootObjects().front());
    if (window == nullptr) {
        qCritical() << "Drumaspro Main root is not a QQuickWindow";
        return 1;
    }
    window->setMinimumSize(QSize(960, 600));
    window->resize(QSize(1280, 720));
    qInfo() << "Drumaspro window size:" << window->width() << "x" << window->height();
    return application.exec();
}
