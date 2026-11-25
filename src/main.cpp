#include "controller.h"

#include <pipewire/pipewire.h>

#include <QApplication>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMenu>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSystemTrayIcon>
#include <QTimer>

#include <cstring>

static void showWindow(QQuickWindow *w) {
    w->show();
    w->raise();
    w->requestActivate();
}

int main(int argc, char *argv[]) {
    // Helper mode used for safe VST3 scanning: load one bundle, print its effects, exit.
    if (argc >= 3 && std::strcmp(argv[1], "--scan-vst3") == 0) {
        QCoreApplication helper(argc, argv);
        return PluginManager::runScanHelper(QString::fromLocal8Bit(argv[2]));
    }
    pw_init(&argc, &argv);

    QApplication app(argc, argv);
    // Test instance: separate settings, separate virtual mic, no single-instance lock.
    // Lets MicForge be checked while the real copy keeps running.
    const bool testInstance = qEnvironmentVariableIsSet("MICFORGE_TEST");
    app.setApplicationName(testInstance ? "MicForgeTest" : "MicForge");
    if (testInstance) {
        mf::AudioEngine::prefix = "micforge_test";
        mf::AudioEngine::micDescription = "MicForge TEST (ignore)";
    }
    app.setOrganizationName("MicForge");
    app.setDesktopFileName("micforge");
    app.setWindowIcon(QIcon::fromTheme("micforge", QIcon(":/icons/micforge.svg")));
    app.setQuitOnLastWindowClosed(false);

    const bool startMinimized = app.arguments().contains("--minimized");

    // Single instance: a second launch just brings the first window forward.
    const QString serverName = (testInstance ? "micforge-test-" : "micforge-") + qEnvironmentVariable("USER");
    {
        QLocalSocket probe;
        probe.connectToServer(serverName);
        if (probe.waitForConnected(300)) {
            probe.write("show");
            probe.waitForBytesWritten(300);
            return 0;
        }
    }
    QLocalServer::removeServer(serverName);
    QLocalServer server;
    server.listen(serverName);

    QQuickStyle::setStyle("Fusion");

    Controller controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    engine.rootContext()->setContextProperty("trayAvailable", QSystemTrayIcon::isSystemTrayAvailable());
    // Development aids for screenshots: open a given page / the voice setup on launch.
    engine.rootContext()->setContextProperty("initialPage", qEnvironmentVariable("MICFORGE_PAGE"));
    engine.rootContext()->setContextProperty("initialSetup", qEnvironmentVariableIsSet("MICFORGE_SETUP"));
    engine.rootContext()->setContextProperty("initialLibrary", qEnvironmentVariableIsSet("MICFORGE_LIBRARY"));
    engine.rootContext()->setContextProperty("initialExpandPlugin", qEnvironmentVariableIsSet("MICFORGE_EXPAND_PLUGIN"));
    engine.load(QUrl("qrc:/qml/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) return 1;

    QObject::connect(&server, &QLocalServer::newConnection, [&] {
        while (QLocalSocket *c = server.nextPendingConnection()) {
            QObject::connect(c, &QLocalSocket::disconnected, c, &QObject::deleteLater);
            showWindow(window);
        }
    });

    // Tray icon so processing keeps running with the window closed.
    QSystemTrayIcon tray(QIcon::fromTheme("micforge", QIcon(":/icons/micforge.svg")));
    tray.setToolTip("MicForge — mic processing is running");
    QMenu menu;
    QAction *showAct = menu.addAction(QIcon::fromTheme("window"), "Show MicForge");
    QAction *bypassAct = menu.addAction("Bypass processing");
    bypassAct->setCheckable(true);
    QAction *listenAct = menu.addAction("Listen to myself");
    listenAct->setCheckable(true);
    menu.addSeparator();
    QAction *quitAct = menu.addAction(QIcon::fromTheme("application-exit"), "Quit");
    tray.setContextMenu(&menu);

    QObject::connect(showAct, &QAction::triggered, [&] { showWindow(window); });
    QObject::connect(bypassAct, &QAction::toggled, [&](bool on) { controller.setParam("bypass", on ? 1 : 0); });
    QObject::connect(listenAct, &QAction::toggled, &controller, &Controller::setMonitor);
    QObject::connect(&controller, &Controller::monitorChanged, [&] { listenAct->setChecked(controller.monitor()); });
    QObject::connect(controller.params(), &QQmlPropertyMap::valueChanged, [&](const QString &key, const QVariant &v) {
        if (key == "bypass") bypassAct->setChecked(v.toDouble() > 0.5);
    });
    QObject::connect(&menu, &QMenu::aboutToShow, [&] {
        bypassAct->setChecked(controller.params()->value("bypass").toDouble() > 0.5);
    });
    QObject::connect(quitAct, &QAction::triggered, &app, &QApplication::quit);
    QObject::connect(&tray, &QSystemTrayIcon::activated, [&](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger) {
            if (window->isVisible()) window->hide();
            else showWindow(window);
        }
    });
    if (QSystemTrayIcon::isSystemTrayAvailable() && !testInstance) tray.show();
    else app.setQuitOnLastWindowClosed(true);

    if (!startMinimized || !QSystemTrayIcon::isSystemTrayAvailable()) showWindow(window);

    // Development aid: save a picture of MicForge's own window, then quit.
    const QString snapshot = qEnvironmentVariable("MICFORGE_SNAPSHOT");
    if (!snapshot.isEmpty()) {
        QTimer::singleShot(qEnvironmentVariableIntValue("MICFORGE_SNAPSHOT_DELAY") > 0 ? qEnvironmentVariableIntValue("MICFORGE_SNAPSHOT_DELAY") : 2500, &app, [&] {
            window->grabWindow().save(snapshot);
            app.quit();
        });
    }

    const int rc = app.exec();
    controller.saveNow();
    return rc;
}
