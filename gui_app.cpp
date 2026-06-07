#include "gui_app.hpp"
#include "cheat_bridge.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QUrl>
#include <QtQml/qqml.h>
#include <cstdlib>
#include <iostream>

#ifndef SOBER_QML_DIR
#define SOBER_QML_DIR ""
#endif

static QString find_main_qml() {
    const QString source_qml = QString::fromUtf8(SOBER_QML_DIR) + "/Main.qml";
    const QStringList candidates = {
        source_qml,
        QCoreApplication::applicationDirPath() + "/../qml/Main.qml",
        QCoreApplication::applicationDirPath() + "/qml/Main.qml",
        QStringLiteral("/usr/share/sober/qml/Main.qml"),
    };
    for (const auto& path : candidates) {
        if (QFile::exists(path)) return path;
    }
    return {};
}

int run_sober_gui(int argc, char** argv, CheatRuntime& runtime) {
    qputenv("QT_QUICK_CONTROLS_STYLE", "org.kde.desktop");
    if (qEnvironmentVariableIsSet("WAYLAND_DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "wayland");

    QGuiApplication app(argc, argv);
    app.setApplicationName("Penguin");
    app.setOrganizationName("sober");
    app.setDesktopFileName("sober-cheat");

    QQmlApplicationEngine engine;
    const QStringList import_paths = {
        QStringLiteral("/usr/lib/qt6/qml"),
        QStringLiteral("/usr/lib64/qt6/qml"),
    };
    for (const auto& p : import_paths) {
        if (QDir(p).exists()) engine.addImportPath(p);
    }

    static CheatBridge bridge(runtime);
    qmlRegisterSingletonInstance("Sober", 1, 0, "Cheat", &bridge);

    const QString qml_path = find_main_qml();
    if (qml_path.isEmpty()) {
        std::cerr << "Missing Main.qml (set SOBER_QML_DIR or install to share/sober/qml)\n";
        return 1;
    }

    engine.load(QUrl::fromLocalFile(qml_path));
    if (engine.rootObjects().isEmpty()) {
        std::cerr << "Failed to load Kirigami UI (install kirigami: "
                     "sudo pacman -S kirigami qt6-declarative)\n";
        return 1;
    }

    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&runtime] { runtime.shutdown(); });

    return app.exec();
}
