#include "diagnostics.h"

#include "controller.h"

#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTextStream>

#include <chrono>

using namespace mf;

Diagnostics::Diagnostics(Controller *controller, AudioCore &core, IAudioBackend &backend, QString logPath, QObject *parent)
    : QObject(parent), m_ctl(controller), m_core(core), m_backend(backend), m_logPath(std::move(logPath)) {
    m_uptime.start();
    m_timer.setInterval(1000);
    connect(&m_timer, &QTimer::timeout, this, &Diagnostics::refresh);
}

void Diagnostics::setActive(bool on) {
    if (on == active()) return;
    if (on) {
        refresh();
        m_timer.start();
    } else {
        m_timer.stop();
    }
    emit activeChanged();
}

QString Diagnostics::deviceKind(const QString &n) {
    if (n.startsWith("bluez")) return QStringLiteral("Bluetooth");
    if (n.contains(".usb-")) return QStringLiteral("USB");
    if (n.contains(".pci-") || n.contains("platform-")) return QStringLiteral("built-in");
    if (n.isEmpty()) return QStringLiteral("none");
    return QStringLiteral("other");
}

QString Diagnostics::scrub(QString t) {
    const QString home = QDir::homePath();
    if (home.size() > 1) t.replace(home, QStringLiteral("~"));
    const QString user = qEnvironmentVariable("USER", qEnvironmentVariable("USERNAME"));
    if (user.size() > 1) t.replace(QRegularExpression("\\b" + QRegularExpression::escape(user) + "\\b"), QStringLiteral("<user>"));
    const QString host = QSysInfo::machineHostName();
    if (host.size() > 1) t.replace(QRegularExpression("\\b" + QRegularExpression::escape(host) + "\\b"), QStringLiteral("<computer>"));
    return t;
}

void Diagnostics::refresh() {
    EngineStats &s = m_core.stats;
    const float procMax = s.procUsMax.exchange(0, std::memory_order_relaxed);
    const float loadMax = s.loadMax.exchange(0, std::memory_order_relaxed);
    m_sessionProcMax = std::max(m_sessionProcMax, procMax);
    m_sessionLoadMax = std::max(m_sessionLoadMax, loadMax);
    const uint32_t frames = s.blockFrames.load();

    QVariantMap m;
    m["appVersion"] = QStringLiteral(MICFORGE_VERSION);
    m["qtVersion"] = QString::fromLatin1(qVersion());
    m["os"] = QSysInfo::prettyProductName();
    m["kernel"] = QSysInfo::kernelVersion();
    m["arch"] = QSysInfo::currentCpuArchitecture();
    m["backend"] = m_ctl->backendName();
    m["backendVersion"] = QString::fromStdString(m_backend.version());
    m["uptimeS"] = qint64(m_uptime.elapsed() / 1000);
    m["safeMode"] = m_ctl->safeMode();
    m["appState"] = m_ctl->appState();
    m["appStateText"] = m_ctl->appStateText();
    m["engineState"] = m_ctl->engineState();
    m["engineError"] = m_ctl->engineError();
    m["captureRunning"] = m_ctl->captureRunning();
    m["virtualMicInUse"] = m_ctl->virtualMicInUse();
    m["virtualMicName"] = m_ctl->virtualMicName();
    m["monitor"] = m_ctl->monitor();

    QString devName, devId = m_ctl->currentSource();
    for (const QVariant &v : m_ctl->sources()) {
        const QVariantMap d = v.toMap();
        if (d.value("name").toString() == devId) devName = d.value("description").toString();
    }
    m["inputName"] = devName.isEmpty() ? (devId.isEmpty() ? tr("none selected") : tr("not connected")) : devName;
    m["inputKind"] = deviceKind(devId);
    m["inputChannel"] = QStringList{tr("input 1 (left)"), tr("input 2 (right)"), tr("both")}
                            .value(int(m_ctl->params()->value("inputChannel").toDouble() + 0.5));
    m["inputCount"] = m_ctl->sources().size();

    m["processingRate"] = AudioCore::kRate;
    m["graphRate"] = s.graphRate.load();
    m["quantum"] = frames;
    m["quantumMs"] = frames ? 1000.0 * frames / AudioCore::kRate : 0.0;
    m["inputLatencyMs"] = s.inputLatencyMs.load();
    m["outputLatencyMs"] = s.outputLatencyMs.load();
    m["bufferMs"] = 1000.0 * double(m_core.virtualMicQueued()) / AudioCore::kRate;
    m["procUsAvg"] = s.procUsAvg.load();
    m["procUsMax"] = procMax;
    m["procUsSessionMax"] = m_sessionProcMax;
    m["loadPct"] = 100.0 * loadMax;
    m["loadSessionPct"] = 100.0 * m_sessionLoadMax;
    m["blocks"] = qulonglong(s.blocks.load());
    m["lateCallbacks"] = qulonglong(s.lateCallbacks.load());
    m["vmicUnderruns"] = qulonglong(s.vmicUnderruns.load());
    m["monitorUnderruns"] = qulonglong(s.monitorUnderruns.load());
    m["inputClips"] = qulonglong(s.inputClips.load());
    const int64_t lastClip = s.lastClipNs.load();
    const int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    m["secondsSinceClip"] = lastClip ? double(now - lastClip) / 1e9 : -1.0;

    QVariantList plugins;
    for (const QVariant &v : m_ctl->plugins()->chain()) {
        const QVariantMap p = v.toMap();
        plugins << QVariantMap{{"name", p.value("name")}, {"format", p.value("format")},
                               {"loaded", p.value("loaded")}, {"bypass", p.value("bypass")},
                               {"error", scrub(p.value("error").toString())}, {"latency", p.value("latency")}};
    }
    m["plugins"] = plugins;
    m["unavailable"] = m_ctl->unavailableFeatures();
    m["logPath"] = m_logPath;

    // Problems worth pointing out, in plain English.
    QStringList issues;
    if (m_ctl->appState() == "disconnected" || m_ctl->appState() == "connecting")
        issues << tr("MicForge isn't connected to the audio system.");
    else if (!m_ctl->captureRunning())
        issues << tr("No sound is arriving from the microphone.");
    if (m["secondsSinceClip"].toDouble() >= 0 && m["secondsSinceClip"].toDouble() < 60)
        issues << tr("Your microphone clipped in the last minute: turn down the gain on your interface or mic.");
    if (m_sessionLoadMax > 0.7f)
        issues << tr("Processing came close to the time limit (%1%). Remove heavy plugins or effects if you hear crackles.")
                      .arg(qRound(100 * m_sessionLoadMax));
    if (s.lateCallbacks.load() > 0)
        issues << tr("Audio from the microphone arrived late %n time(s) (possible crackles).", nullptr, int(s.lateCallbacks.load()));
    if (s.vmicUnderruns.load() > 0)
        issues << tr("The processed microphone ran dry %n time(s) while an app was listening (possible dropouts).", nullptr, int(s.vmicUnderruns.load()));
    for (const QVariant &v : plugins)
        if (!v.toMap().value("error").toString().isEmpty())
            issues << tr("Plugin “%1”: %2").arg(v.toMap().value("name").toString(), v.toMap().value("error").toString());
    m["issues"] = issues;

    m_info = m;
    emit updated();
}

void Diagnostics::resetCounters() {
    m_core.stats.lateCallbacks = 0;
    m_core.stats.vmicUnderruns = 0;
    m_core.stats.monitorUnderruns = 0;
    m_core.stats.inputClips = 0;
    m_core.stats.lastClipNs = 0;
    m_sessionProcMax = m_sessionLoadMax = 0;
    refresh();
}

QString Diagnostics::logTail(int lines) const {
    QFile f(m_logPath);
    if (!f.open(QIODevice::ReadOnly)) return tr("(The log file can't be read: %1)").arg(m_logPath);
    const qint64 maxBytes = 256 * 1024;
    if (f.size() > maxBytes) f.seek(f.size() - maxBytes);
    QStringList all = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    if (all.size() > lines) all = all.mid(all.size() - lines);
    return all.join('\n');
}

QString Diagnostics::report(bool includeDeviceNames, bool includeLog) const {
    const QVariantMap &m = m_info;
    QString out;
    QTextStream t(&out);
    auto line = [&](const QString &k, const QVariant &v) { t << k.leftJustified(26, ' ') << v.toString() << '\n'; };
    t << "MicForge diagnostic report\n"
      << "Created " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n"
      << "Contains no audio, no recordings and no sound settings. Home folder, user name, computer name,\n"
      << "preset names and plugin file locations are removed"
      << (includeDeviceNames ? "." : "; device names are replaced by their type.") << "\n\n";
    t << "[System]\n";
    line("MicForge", m.value("appVersion").toString() + (m.value("safeMode").toBool() ? " (safe mode)" : ""));
    line("OS", m.value("os").toString() + ", kernel " + m.value("kernel").toString() + ", " + m.value("arch").toString());
    line("Qt", m.value("qtVersion"));
    line("Audio system", m.value("backend").toString() + " (" + m.value("backendVersion").toString() + ")");
    line("Running for", QString::number(m.value("uptimeS").toLongLong()) + " s");
    t << "\n[State]\n";
    line("State", m.value("appState").toString() + ": " + m.value("appStateText").toString());
    line("Engine", m.value("engineState").toString() + (m.value("engineError").toString().isEmpty() ? "" : " (" + m.value("engineError").toString() + ")"));
    line("Microphone", (includeDeviceNames ? m.value("inputName").toString() : m.value("inputKind").toString() + " device")
                           + ", " + m.value("inputChannel").toString());
    line("Microphones available", m.value("inputCount"));
    line("Receiving sound", m.value("captureRunning").toBool() ? "yes" : "no");
    line("Virtual mic in use", m.value("virtualMicInUse").toBool() ? "yes" : "no");
    line("Listening (monitor)", m.value("monitor").toBool() ? "on" : "off");
    t << "\n[Timing]\n";
    line("Processing rate", QString::number(m.value("processingRate").toInt()) + " Hz");
    line("Device/graph rate", m.value("graphRate").toInt() ? QString::number(m.value("graphRate").toInt()) + " Hz" : QStringLiteral("unknown"));
    line("Block size", QString::number(m.value("quantum").toInt()) + " frames (" + QString::number(m.value("quantumMs").toDouble(), 'f', 1) + " ms)");
    line("Input latency", QString::number(m.value("inputLatencyMs").toDouble(), 'f', 1) + " ms");
    line("Output latency", QString::number(m.value("outputLatencyMs").toDouble(), 'f', 1) + " ms");
    line("Internal buffer", QString::number(m.value("bufferMs").toDouble(), 'f', 1) + " ms");
    line("Processing time", QString::number(m.value("procUsAvg").toDouble(), 'f', 0) + " µs average, "
                                + QString::number(m.value("procUsSessionMax").toDouble(), 'f', 0) + " µs worst");
    line("Load (worst)", QString::number(m.value("loadSessionPct").toDouble(), 'f', 1) + " % of each block");
    line("Late input blocks", m.value("lateCallbacks"));
    line("Virtual mic underruns", m.value("vmicUnderruns"));
    line("Monitor underruns", m.value("monitorUnderruns"));
    line("Clipped input samples", m.value("inputClips"));
    t << "\n[Plugins]\n";
    const QVariantList plugins = m.value("plugins").toList();
    if (plugins.isEmpty()) t << "none\n";
    for (const QVariant &v : plugins) {
        const QVariantMap p = v.toMap();
        t << "- " << p.value("name").toString() << " (" << p.value("format").toString() << ")"
          << (p.value("bypass").toBool() ? ", bypassed" : "") << (p.value("loaded").toBool() ? "" : ", NOT LOADED")
          << (p.value("error").toString().isEmpty() ? "" : ", error: " + p.value("error").toString()) << '\n';
    }
    t << "\n[Optional features unavailable]\n";
    const QStringList un = m.value("unavailable").toStringList();
    t << (un.isEmpty() ? QStringLiteral("none") : un.join('\n')) << '\n';
    t << "\n[Problems noticed]\n";
    const QStringList issues = m.value("issues").toStringList();
    t << (issues.isEmpty() ? QStringLiteral("none") : "- " + issues.join("\n- ")) << '\n';
    if (includeLog) {
        QString log = logTail(150);
        if (!includeDeviceNames)
            for (const QVariant &v : m_ctl->sources()) {
                const QVariantMap d = v.toMap();
                const QString kind = "<" + deviceKind(d.value("name").toString()) + " device>";
                log.replace(d.value("description").toString(), kind).replace(d.value("name").toString(), kind);
            }
        t << "\n[Recent log]\n" << log << '\n';
    }
    t.flush();
    return scrub(out);
}

void Diagnostics::copyReport(bool includeDeviceNames, bool includeLog) const {
    QGuiApplication::clipboard()->setText(report(includeDeviceNames, includeLog));
}

bool Diagnostics::saveReport(const QUrl &file, bool includeDeviceNames, bool includeLog) const {
    QFile f(file.isLocalFile() ? file.toLocalFile() : file.toString());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return f.write(report(includeDeviceNames, includeLog).toUtf8()) > 0;
}
