#include <QGuiApplication>
#include <QQuickStyle>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDebug>

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
    return application.exec();
}
