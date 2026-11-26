#include "startup.h"

#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocalSocket>
#include <QMessageBox>
#include <QMutex>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <QSocketNotifier>
#include <csignal>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace mf::startup {

// ---------------------------------------------------------------- logging

namespace {
QMutex g_logMutex;
QFile *g_log = nullptr;
constexpr qint64 kMaxLogBytes = 1024 * 1024;
constexpr int kKeepLogs = 3;

void rotateIfNeeded() {
    if (!g_log || g_log->size() < kMaxLogBytes) return;
    const QString path = g_log->fileName();
    g_log->close();
    for (int i = kKeepLogs - 1; i >= 1; --i) {
        QFile::remove(path + "." + QString::number(i + 1));
        QFile::rename(path + "." + QString::number(i), path + "." + QString::number(i + 1));
    }
    QFile::remove(path + ".1");
    QFile::rename(path, path + ".1");
    g_log->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
}

void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg) {
    static const char *names[] = {"DEBUG", "WARN ", "ERROR", "FATAL", "INFO "};
    const char *level = names[type == QtDebugMsg ? 0 : type == QtWarningMsg ? 1 : type == QtCriticalMsg ? 2
                                 : type == QtFatalMsg ? 3 : 4];
    const QByteArray line = QStringLiteral("%1 %2 %3\n")
                                .arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs), QLatin1String(level), msg)
                                .toUtf8();
    {
        QMutexLocker l(&g_logMutex);
        if (g_log) {
            rotateIfNeeded();
            g_log->write(line);
            g_log->flush();
        }
    }
    std::fputs(line.constData() + 24, stderr); // the journal adds its own timestamp
    std::fflush(stderr);
}
} // namespace

QString stateDir() {
    QString d = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    if (d.isEmpty()) d = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(d);
    return d;
}

QString logFilePath() { return stateDir() + "/micforge.log"; }

void initLogging() {
    g_log = new QFile(logFilePath());
    if (!g_log->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete g_log;
        g_log = nullptr;
    }
    qInstallMessageHandler(messageHandler);
}

// ---------------------------------------------------------------- signals

#ifndef _WIN32
namespace {
int g_signalPipe[2] = {-1, -1};
void onSignal(int sig) {
    const char c = char(sig);
    [[maybe_unused]] auto r = ::write(g_signalPipe[1], &c, 1); // async-signal-safe
}
} // namespace

void quitCleanlyOnSignals() {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_signalPipe) != 0) return;
    auto *notifier = new QSocketNotifier(g_signalPipe[0], QSocketNotifier::Read, QCoreApplication::instance());
    QObject::connect(notifier, &QSocketNotifier::activated, [] {
        char c = 0;
        [[maybe_unused]] auto r = ::read(g_signalPipe[0], &c, 1);
        qInfo("received signal %d; quitting cleanly", int(c));
        QCoreApplication::quit();
    });
    struct sigaction sa {};
    sa.sa_handler = onSignal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
}
#else
void quitCleanlyOnSignals() {}
#endif

// ---------------------------------------------------------------- single instance

namespace {
bool processAlive(qint64 pid) {
    if (pid <= 0) return false;
#ifdef _WIN32
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, DWORD(pid));
    if (!h) return false;
    const bool alive = WaitForSingleObject(h, 0) == WAIT_TIMEOUT;
    CloseHandle(h);
    return alive;
#else
    return ::kill(pid_t(pid), 0) == 0;
#endif
}
} // namespace

SingleInstance::SingleInstance(const QString &baseName, QObject *parent) : QObject(parent), m_base(baseName) {
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty()) dir = QDir::tempPath();
    m_lock = std::make_unique<QLockFile>(dir + "/" + baseName + ".lock");
    m_lock->setStaleLockTime(0); // stale = the owning process no longer exists (not time-based)
}

SingleInstance::~SingleInstance() { release(); }

void SingleInstance::release() {
    m_server.close();
    if (m_lock && m_lock->isLocked()) m_lock->unlock();
}

bool SingleInstance::listen() {
    QLocalServer::removeServer(m_base); // clears a socket left behind by a crashed instance
    if (!m_server.listen(m_base)) {
        qWarning("single-instance: can't listen on %s: %s", qPrintable(m_base), qPrintable(m_server.errorString()));
        return false;
    }
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *c = m_server.nextPendingConnection()) {
            connect(c, &QLocalSocket::disconnected, c, &QObject::deleteLater);
            connect(c, &QLocalSocket::readyRead, c, [this, c] {
                if (c->readAll().contains("show")) {
                    c->write("ok\n");
                    c->flush();
                    emit showRequested();
                }
            });
        }
    });
    return true;
}

bool SingleInstance::askExistingToShow(int timeoutMs) {
    QLocalSocket s;
    s.connectToServer(m_base);
    if (!s.waitForConnected(std::min(timeoutMs, 2000))) return false;
    s.write("show\n");
    s.waitForBytesWritten(1000);
    // The other instance replies from its event loop; while it's still starting up that can take a while.
    QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired()) {
        if (s.waitForReadyRead(int(std::max<qint64>(1, deadline.remainingTime()))) && s.readAll().contains("ok"))
            return true;
        if (s.state() != QLocalSocket::ConnectedState) break;
    }
    return false;
}

bool SingleInstance::terminate(qint64 pid) {
    qWarning("single-instance: terminating unresponsive instance (PID %lld)", pid);
#ifdef _WIN32
    HANDLE h = OpenProcess(PROCESS_TERMINATE | SYNCHRONIZE, FALSE, DWORD(pid));
    if (!h) return false;
    TerminateProcess(h, 1);
    WaitForSingleObject(h, 3000);
    CloseHandle(h);
#else
    ::kill(pid_t(pid), SIGTERM);
    for (int i = 0; i < 30 && processAlive(pid); ++i) QThread::msleep(100);
    if (processAlive(pid)) ::kill(pid_t(pid), SIGKILL);
    for (int i = 0; i < 20 && processAlive(pid); ++i) QThread::msleep(100);
#endif
    return !processAlive(pid);
}

SingleInstance::Result SingleInstance::acquire(bool interactive, bool replaceUnresponsive) {
    if (m_lock->tryLock(0)) {
        qInfo("single-instance: this is the main instance (PID %lld)", QCoreApplication::applicationPid());
        return listen() ? Result::Primary : Result::Failed;
    }
    qint64 pid = 0;
    QString host, app;
    m_lock->getLockInfo(&pid, &host, &app);

    // Another instance owns the lock. Ask it to show its window. Give a freshly started instance time
    // to finish starting (its event loop isn't running yet), so simultaneous launches don't look frozen.
    if (askExistingToShow(10000)) {
        qInfo("single-instance: MicForge is already running (PID %lld); asked it to show its window", pid);
        return Result::HandedOver;
    }
    if (!processAlive(pid)) {
        // Lock owner is gone: QLockFile treats it as stale, try again.
        m_lock->removeStaleLockFile();
        if (m_lock->tryLock(0)) return listen() ? Result::Primary : Result::Failed;
        return Result::Failed;
    }

    qWarning("single-instance: MicForge is already running (PID %lld) but isn't responding", pid);
    bool replace = replaceUnresponsive;
    if (!replace && interactive) {
        replace = QMessageBox::question(
                      nullptr, "MicForge isn't responding",
                      QString("MicForge is already running (process %1) but isn't responding.\n\n"
                              "Close it and start a fresh copy? Your settings and calibration are kept.")
                          .arg(pid),
                      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes;
    }
    if (!replace) return Result::Unresponsive;
    if (!terminate(pid)) {
        qCritical("single-instance: couldn't stop the unresponsive instance (PID %lld)", pid);
        return Result::Unresponsive;
    }
    m_lock->removeStaleLockFile();
    if (m_lock->tryLock(2000)) return listen() ? Result::Primary : Result::Failed;
    return Result::Failed;
}

// ---------------------------------------------------------------- crash-loop guard

CrashGuard::CrashGuard() : m_path(stateDir() + "/launch-guard") {
    if (QFile r(m_path); r.open(QIODevice::ReadOnly)) m_failedLaunches = r.readAll().trimmed().toInt();
    // Count this launch as "not yet stable" until markStable() is called.
    if (QFile w(m_path); w.open(QIODevice::WriteOnly | QIODevice::Truncate)) w.write(QByteArray::number(m_failedLaunches + 1));
}

void CrashGuard::markStable() {
    QFile f(m_path);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write("0");
}

// ---------------------------------------------------------------- settings safety

SettingsCheck checkAndBackupSettings() {
    SettingsCheck out;
    QSettings s;
    const QString file = s.fileName();
    if (!QFileInfo::exists(file)) return out;
    const QString dir = stateDir() + "/backups";
    QDir().mkpath(dir);
    const QString base = QFileInfo(file).completeBaseName();

    if (s.status() == QSettings::FormatError) {
        out.damaged = true;
        out.keptCopy = dir + "/" + base + "-damaged-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + ".conf";
        QFile::copy(file, out.keptCopy);
        qCritical("settings: %s couldn't be read; kept a copy at %s", qPrintable(file), qPrintable(out.keptCopy));
        return out;
    }
    const QString today = dir + "/" + base + "-" + QDate::currentDate().toString("yyyy-MM-dd") + ".conf";
    if (!QFileInfo::exists(today) && QFile::copy(file, today)) qInfo("settings: daily backup saved to %s", qPrintable(today));
    QStringList old = QDir(dir).entryList({base + "-20*.conf"}, QDir::Files, QDir::Name);
    while (old.size() > 14) QFile::remove(dir + "/" + old.takeFirst());

    // Sound presets: a copy of the presets folder once a day (14 days kept).
    const QDir presets(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/presets");
    const QStringList files = presets.entryList({"*.json"}, QDir::Files);
    const QString presetCopy = dir + "/presets-" + QDate::currentDate().toString("yyyy-MM-dd");
    if (!files.isEmpty() && !QFileInfo::exists(presetCopy) && QDir().mkpath(presetCopy)) {
        int copied = 0;
        for (const QString &f : files) copied += QFile::copy(presets.filePath(f), presetCopy + "/" + f) ? 1 : 0;
        qInfo("settings: daily backup of %d preset(s) saved to %s", copied, qPrintable(presetCopy));
    }
    QStringList oldPresets = QDir(dir).entryList({"presets-20*"}, QDir::Dirs, QDir::Name);
    while (oldPresets.size() > 14) QDir(dir + "/" + oldPresets.takeFirst()).removeRecursively();
    return out;
}

} // namespace mf::startup
