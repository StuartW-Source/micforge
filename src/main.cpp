#include "controller.h"
#include "startup.h"

#include <pipewire/pipewire.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSystemTrayIcon>
#include <QTimer>

#include <cstring>

using namespace mf;

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
    // Test instance: separate settings, separate virtual mic, separate lock/log.
    // Lets MicForge be checked while the real copy keeps running.
    const bool testInstance = qEnvironmentVariableIsSet("MICFORGE_TEST");
    app.setApplicationName(testInstance ? "MicForgeTest" : "MicForge");
    app.setApplicationVersion(MICFORGE_VERSION);
    app.setOrganizationName("MicForge");
    app.setDesktopFileName("micforge");
    app.setWindowIcon(QIcon::fromTheme("micforge", QIcon(":/icons/micforge.svg")));
    app.setQuitOnLastWindowClosed(false);

    QCommandLineParser cli;
    cli.setApplicationDescription("MicForge: microphone processing for voice chat, streaming and recording.");
    cli.addHelpOption();
    cli.addVersionOption();
    const QCommandLineOption minimizedOpt("minimized", "Start hidden in the system tray (used at login).");
    const QCommandLineOption safeOpt("safe-mode",
        "Start without your plugins and with default sound settings. Nothing is saved, so your configuration stays untouched.");
    const QCommandLineOption replaceOpt("replace", "If another MicForge is running but not responding, close it and start this one.");
    cli.addOptions({minimizedOpt, safeOpt, replaceOpt});
    cli.process(app);
    const bool startMinimized = cli.isSet(minimizedOpt);

    startup::initLogging();
    qInfo("MicForge %s starting (PID %lld, %s, binary %s)", MICFORGE_VERSION, app.applicationPid(),
          startMinimized ? "minimized/login launch" : "normal launch", qPrintable(app.applicationFilePath()));

    // ---- single instance
    startup::SingleInstance instance(testInstance ? "micforge-test-" + qEnvironmentVariable("USER")
                                                  : "micforge-" + qEnvironmentVariable("USER"));
    const bool replace = cli.isSet(replaceOpt) || qEnvironmentVariableIsSet("MICFORGE_REPLACE_UNRESPONSIVE");
    switch (instance.acquire(/*interactive*/ !startMinimized, replace)) {
    case startup::SingleInstance::Result::Primary: break;
    case startup::SingleInstance::Result::HandedOver: return 0;
    case startup::SingleInstance::Result::Unresponsive:
        qCritical("startup: another MicForge is running but not responding; not starting a second copy. "
                  "Run `micforge --replace` to replace it.");
        return 3;
    case startup::SingleInstance::Result::Failed:
        qCritical("startup: couldn't set up single-instance handling");
        return 4;
    }

    startup::quitCleanlyOnSignals();

    // ---- crash-loop guard and settings safety
    startup::CrashGuard guard;
    StartupInfo info;
    info.logPath = startup::logFilePath();
    if (testInstance) info.backend = {"micforge_test", "MicForge TEST (ignore)"};
    if (cli.isSet(safeOpt) || qEnvironmentVariableIsSet("MICFORGE_SAFE_MODE")) {
        info.safeMode = true;
        info.safeModeReason = QObject::tr("You started MicForge in safe mode.");
    } else if (guard.previousLaunchesFailed()) {
        info.safeMode = true;
        info.safeModeReason = QObject::tr("MicForge didn't start properly the last %1 times, so it started in safe mode.")
                                  .arg(guard.failedLaunches());
        qWarning("startup: %d previous launches failed; starting in safe mode", guard.failedLaunches());
    }
    if (!info.safeMode) {
        const auto check = startup::checkAndBackupSettings();
        info.settingsDamaged = check.damaged;
        info.damagedSettingsCopy = check.keptCopy;
    }

    QQuickStyle::setStyle("Fusion");

    Controller controller(info);
    QQmlApplicationEngine engine;
    QStringList qmlErrors;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [&](const QList<QQmlError> &warnings) {
        for (const auto &w : warnings) {
            qmlErrors << w.toString();
            qWarning("qml: %s", qPrintable(w.toString()));
        }
    });
    auto *ctx = engine.rootContext();
    ctx->setContextProperty("controller", &controller);
    ctx->setContextProperty("trayAvailable", QSystemTrayIcon::isSystemTrayAvailable() && !testInstance);
    // Development aids for screenshots: open a given page / the voice setup on launch.
    ctx->setContextProperty("initialPage", qEnvironmentVariable("MICFORGE_PAGE"));
    ctx->setContextProperty("initialSetup", qEnvironmentVariableIsSet("MICFORGE_SETUP"));
    ctx->setContextProperty("initialLibrary", qEnvironmentVariableIsSet("MICFORGE_LIBRARY"));
    ctx->setContextProperty("initialExpandPlugin", qEnvironmentVariableIsSet("MICFORGE_EXPAND_PLUGIN"));
    engine.load(QUrl("qrc:/qml/Main.qml"));
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window) {
        // Without this, a broken Qt installation (e.g. a missing QML module) would fail silently.
        qCritical("startup: the interface failed to load");
        QMessageBox::critical(nullptr, "MicForge couldn't start",
                              "MicForge's window couldn't be loaded. This usually means part of Qt is missing "
                              "(for example the qt6-declarative package).\n\nDetails:\n" +
                                  qmlErrors.mid(0, 5).join("\n") + "\n\nLog file: " + info.logPath);
        return 2;
    }
    QObject::connect(&instance, &startup::SingleInstance::showRequested, window, [window] { showWindow(window); });

    // ---- tray icon, so processing keeps running with the window closed
    QSystemTrayIcon tray(QIcon::fromTheme("micforge", QIcon(":/icons/micforge.svg")));
    tray.setToolTip("MicForge — mic processing is running");
    QMenu menu;
    QAction *showAct = menu.addAction(QIcon::fromTheme("window"), "Show MicForge");
    QAction *bypassAct = menu.addAction("Bypass processing");
    bypassAct->setCheckable(true);
    QAction *listenAct = menu.addAction("Listen to myself");
    listenAct->setCheckable(true);
    QAction *restartAct = menu.addAction(QIcon::fromTheme("view-refresh"), "Restart audio engine");
    menu.addSeparator();
    QAction *quitAct = menu.addAction(QIcon::fromTheme("application-exit"), "Quit MicForge");
    tray.setContextMenu(&menu);

    QObject::connect(showAct, &QAction::triggered, [&] { showWindow(window); });
    QObject::connect(bypassAct, &QAction::toggled, [&](bool on) { controller.setParam("bypass", on ? 1 : 0); });
    QObject::connect(listenAct, &QAction::toggled, &controller, &Controller::setMonitor);
    QObject::connect(restartAct, &QAction::triggered, &controller, &Controller::restartEngine);
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

    // At login the panel's tray may not exist yet. Keep processing in the background and add the icon as
    // soon as the tray appears; only if it never does, show the window so MicForge isn't invisible.
    auto useTray = [&] {
        tray.show();
        ctx->setContextProperty("trayAvailable", true);
        app.setQuitOnLastWindowClosed(false);
    };
    QTimer trayWait;
    if (testInstance) {
        app.setQuitOnLastWindowClosed(true);
        showWindow(window);
    } else if (QSystemTrayIcon::isSystemTrayAvailable()) {
        useTray();
        if (!startMinimized) showWindow(window);
    } else if (startMinimized) {
        qInfo("startup: system tray not available yet; running in the background until it appears");
        int waited = 0;
        QObject::connect(&trayWait, &QTimer::timeout, [&] {
            waited += trayWait.interval();
            if (QSystemTrayIcon::isSystemTrayAvailable()) {
                trayWait.stop();
                qInfo("startup: system tray appeared after %d ms", waited);
                useTray();
            } else if (waited >= 120000) {
                trayWait.stop();
                qWarning("startup: no system tray after 2 minutes; showing the window instead");
                app.setQuitOnLastWindowClosed(true);
                showWindow(window);
            }
        });
        trayWait.start(1000);
    } else {
        qWarning("startup: no system tray; closing the window will quit MicForge");
        app.setQuitOnLastWindowClosed(true);
        showWindow(window);
    }

    // ---- restart (normally or in safe mode) from the UI
    QObject::connect(&controller, &Controller::restartRequested, &app, [&](bool safe) {
        qInfo("startup: restarting%s", safe ? " in safe mode" : "");
        controller.saveNow();
        instance.release(); // let the new copy become the main instance
        QStringList args;
        if (safe) args << "--safe-mode";
        QProcess::startDetached(app.applicationFilePath(), args);
        app.quit();
    });

    // A launch that stays up this long counts as successful (resets the crash-loop guard).
    QTimer::singleShot(startup::CrashGuard::kStableSeconds * 1000, &app, [&] { guard.markStable(); });
    QObject::connect(&app, &QApplication::aboutToQuit, [&] {
        guard.markStable(); // a deliberate quit is never a failed launch
        qInfo("MicForge quitting");
    });

    // Development aid: save a picture of MicForge's own window, then quit.
    const QString snapshot = qEnvironmentVariable("MICFORGE_SNAPSHOT");
    if (!snapshot.isEmpty()) {
        QTimer::singleShot(qEnvironmentVariableIntValue("MICFORGE_SNAPSHOT_DELAY") > 0 ? qEnvironmentVariableIntValue("MICFORGE_SNAPSHOT_DELAY") : 2500, &app, [&] {
            window->grabWindow().save(snapshot);
            app.quit();
        });
    }

    qInfo("startup: ready (%s)", info.safeMode ? "SAFE MODE" : "normal");
    const int rc = app.exec();
    controller.saveNow();
    return rc;
}
