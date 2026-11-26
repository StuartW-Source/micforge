// Application start-up lifecycle: logging, single-instance handling, crash-loop detection
// (safe mode) and settings backups. Everything here runs on the UI thread before/around
// QApplication::exec(); nothing here is used by the audio thread.
#pragma once

#include <QLocalServer>
#include <QLockFile>
#include <QObject>
#include <QString>

#include <memory>

namespace mf::startup {

// ---------------------------------------------------------------- logging
// Writes timestamped messages to <state dir>/micforge.log (rotated: micforge.log.1, .2, max ~1 MB each)
// and to stderr, which the systemd user journal captures when MicForge is launched by the desktop.
void initLogging();
QString logFilePath();
QString stateDir(); // e.g. ~/.local/state/MicForge

// Turn SIGTERM / SIGINT / SIGHUP (logout, shutdown, systemd stop, Ctrl+C) into a clean QCoreApplication::quit(),
// so settings are saved and the exit isn't mistaken for a crash. No-op on Windows (handled differently there).
void quitCleanlyOnSignals();

// ---------------------------------------------------------------- single instance
// The first instance takes a lock file in the runtime directory and listens on a local socket.
// Later launches ask it to show its window and exit. A frozen instance is detected (no reply)
// and, if the user agrees (or `replaceUnresponsive` is set), terminated and replaced.
class SingleInstance : public QObject {
    Q_OBJECT
public:
    explicit SingleInstance(const QString &baseName, QObject *parent = nullptr);
    ~SingleInstance() override;

    enum class Result { Primary, HandedOver, Unresponsive, Failed };
    // interactive: may show a dialog (manual launches). Autostart launches pass false.
    Result acquire(bool interactive, bool replaceUnresponsive);
    void release(); // before restarting ourselves

signals:
    void showRequested();

private:
    bool listen();
    bool askExistingToShow(int timeoutMs);
    bool terminate(qint64 pid);

    QString m_base;
    std::unique_ptr<QLockFile> m_lock;
    QLocalServer m_server;
};

// ---------------------------------------------------------------- crash-loop guard / safe mode
// Counts launches that didn't survive kStableSeconds (crash, kill, freeze). After two such launches in a
// row the next one starts in safe mode automatically. A normal quit or a stable run resets the count.
class CrashGuard {
public:
    static constexpr int kStableSeconds = 20;
    CrashGuard();
    bool previousLaunchesFailed() const { return m_failedLaunches >= 2; }
    int failedLaunches() const { return m_failedLaunches; }
    void markStable(); // call once the app has been running for kStableSeconds, and on a clean quit
private:
    QString m_path;
    int m_failedLaunches = 0;
};

// ---------------------------------------------------------------- settings safety
// Keeps one copy of the settings file per day (last 14 days) in <state dir>/backups.
// If the settings file can't be parsed, a copy is kept and the problem is reported (never silently lost).
struct SettingsCheck {
    bool damaged = false;
    QString keptCopy; // where the damaged file was copied to
};
SettingsCheck checkAndBackupSettings();

} // namespace mf::startup
