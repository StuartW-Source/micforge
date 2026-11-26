// Diagnostics: engine health, devices, plugin errors, the log, and a shareable report that leaves out
// personal information and never contains audio. Read-only: nothing here changes processing.
#pragma once

#include "audio/audiobackend.h"
#include "audio/audiocore.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>

class Controller;

class Diagnostics : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap info READ info NOTIFY updated)
    // Polls once a second while true (the Diagnostics page sets it when visible).
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
    Diagnostics(Controller *controller, mf::AudioCore &core, mf::IAudioBackend &backend, QString logPath,
                QObject *parent = nullptr);

    QVariantMap info() const { return m_info; }
    bool active() const { return m_timer.isActive(); }
    void setActive(bool on);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void resetCounters();
    Q_INVOKABLE QString logTail(int lines) const;
    // Report text. Personal details (home folder, user and computer name, preset names, plugin paths)
    // are always removed; device names and log lines are included only when asked for.
    Q_INVOKABLE QString report(bool includeDeviceNames, bool includeLog) const;
    Q_INVOKABLE void copyReport(bool includeDeviceNames, bool includeLog) const;
    Q_INVOKABLE bool saveReport(const QUrl &file, bool includeDeviceNames, bool includeLog) const;

    // Remove the home folder, user name and computer name from any text.
    static QString scrub(QString text);
    // Replace file paths by their file name ("…/ADelay.vst3").
    static QString stripPaths(QString text);
    // "USB", "Bluetooth", "built-in", "virtual" … from a device's internal name.
    static QString deviceKind(const QString &nodeName);

signals:
    void updated();
    void activeChanged();

private:
    Controller *m_ctl;
    mf::AudioCore &m_core;
    mf::IAudioBackend &m_backend;
    QString m_logPath;
    QTimer m_timer;
    QElapsedTimer m_uptime;
    QVariantMap m_info;
    float m_sessionProcMax = 0, m_sessionLoadMax = 0;
};
