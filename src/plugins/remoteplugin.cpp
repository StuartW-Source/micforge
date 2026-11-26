#include "plugins/remoteplugin.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSocketNotifier>

#include <cstdio>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <sys/prctl.h>
#include <thread>
#include <unistd.h>

namespace mf {

namespace {

QJsonObject paramToJson(const PluginParam &p) {
    QJsonArray cv;
    for (float v : p.choiceValues) cv << double(v);
    return QJsonObject{{"id", p.id}, {"name", p.name}, {"unit", p.unit}, {"min", double(p.min)}, {"max", double(p.max)},
                       {"def", double(p.def)}, {"toggle", p.toggle}, {"integer", p.integer}, {"log", p.logScale},
                       {"choices", QJsonArray::fromStringList(p.choices)}, {"choiceValues", cv}};
}
PluginParam paramFromJson(const QJsonObject &o) {
    PluginParam p;
    p.id = o["id"].toString(); p.name = o["name"].toString(); p.unit = o["unit"].toString();
    p.min = float(o["min"].toDouble()); p.max = float(o["max"].toDouble()); p.def = float(o["def"].toDouble());
    p.toggle = o["toggle"].toBool(); p.integer = o["integer"].toBool(); p.logScale = o["log"].toBool();
    for (const auto &c : o["choices"].toArray()) p.choices << c.toString();
    for (const auto &v : o["choiceValues"].toArray()) p.choiceValues.push_back(float(v.toDouble()));
    return p;
}
QByteArray line(const QJsonObject &o) { return QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n'; }

} // namespace

// ---------------------------------------------------------------- host side

std::unique_ptr<RemotePlugin> RemotePlugin::create(const PluginInfo &info, double rate, QString *error) {
    std::unique_ptr<RemotePlugin> r(new RemotePlugin());
    r->info = info;
    r->m_rate = rate;
    if (!r->m_seg.create(bridge::Segment::uniqueName())) {
        if (error) *error = QObject::tr("couldn't set up shared memory for it");
        return nullptr;
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("QT_QPA_PLATFORM", "xcb;offscreen"); // plugin windows are X11 (via XWayland)
    r->m_proc.setProcessEnvironment(env);
    r->m_proc.setProcessChannelMode(QProcess::SeparateChannels);
    QObject::connect(&r->m_proc, &QProcess::readyReadStandardOutput, r.get(), &RemotePlugin::readOutput);
    QObject::connect(&r->m_proc, &QProcess::readyReadStandardError, r.get(), [p = r.get()] {
        for (const QByteArray &l : p->m_proc.readAllStandardError().split('\n'))
            if (!l.trimmed().isEmpty()) qInfo("plugin host [%s]: %s", qPrintable(p->info.name), l.constData());
    });
    QObject::connect(&r->m_proc, &QProcess::finished, r.get(), [p = r.get()](int code, QProcess::ExitStatus st) {
        const bool wasAlive = p->m_alive.exchange(false);
        if (wasAlive && p->onCrashed)
            p->onCrashed(st == QProcess::CrashExit ? QObject::tr("it crashed") : QObject::tr("it stopped (code %1)").arg(code));
    });
    // MICFORGE_HOST_BINARY lets tests run the helper from the real micforge binary.
    const QString bin = qEnvironmentVariableIsSet("MICFORGE_HOST_BINARY") ? qEnvironmentVariable("MICFORGE_HOST_BINARY")
                                                                          : QCoreApplication::applicationFilePath();
    r->m_proc.start(bin,
                    {"--plugin-host", QString::fromStdString(r->m_seg.name()), info.format, info.id, info.path, QString::number(rate)});
    // Wait for "ready" or "error" (plugins can take a while to load).
    QElapsedTimer t;
    t.start();
    while (!r->m_ready && r->m_error.isEmpty() && t.elapsed() < 15000 && r->m_proc.state() != QProcess::NotRunning) {
        r->m_proc.waitForReadyRead(100);
        r->readOutput();
    }
    if (!r->m_ready) {
        if (error) *error = r->m_error.isEmpty() ? QObject::tr("its separate process didn't start") : r->m_error;
        r->m_proc.kill();
        r->m_proc.waitForFinished(1000);
        return nullptr;
    }
    r->m_alive = true;
    r->m_watch.setInterval(1000);
    QObject::connect(&r->m_watch, &QTimer::timeout, r.get(), &RemotePlugin::checkHung);
    r->m_watch.start();
    return r;
}

RemotePlugin::~RemotePlugin() {
    m_alive = false;
    m_watch.stop();
    QObject::disconnect(&m_proc, nullptr, this, nullptr);
    if (bridge::Shared *s = m_seg.get()) { s->quit = 1; bridge::wake(s->state); }
    if (m_proc.state() != QProcess::NotRunning) {
        send(line({{"quit", true}}));
        if (!m_proc.waitForFinished(1000)) { m_proc.kill(); m_proc.waitForFinished(1000); }
    }
}

void RemotePlugin::process(float *io, int n) {
    ParamQueue::Change c;
    while (queue.pop(c)) {} // parameters go to the helper over its stdin; just keep the queue empty
    if (!m_alive.load(std::memory_order_acquire)) return; // helper gone: pass through dry
    const int64_t deadline = int64_t(0.5 * 1e9 * n / m_rate); // half the block's duration
    if (!bridge::processBlock(*m_seg.get(), io, n, deadline)) m_missed.fetch_add(1, std::memory_order_relaxed);
}

void RemotePlugin::onValueSet(int i, float v) {
    if (m_ready) send(line({{"set", QJsonArray{i, double(v)}}}));
}

void RemotePlugin::showEditor(bool show) { send(line({{"editor", show}})); }

void RemotePlugin::send(const QByteArray &l) {
    if (m_proc.state() != QProcess::Running) return;
    m_proc.write(l);
    m_proc.waitForBytesWritten(0); // hand it to the pipe now (settings arrive within milliseconds)
}

void RemotePlugin::readOutput() {
    m_pending += m_proc.readAllStandardOutput();
    int nl;
    while ((nl = m_pending.indexOf('\n')) >= 0) {
        const QByteArray l = m_pending.left(nl);
        m_pending.remove(0, nl + 1);
        if (!l.trimmed().isEmpty()) handle(l);
    }
}

void RemotePlugin::handle(const QByteArray &l) {
    const QJsonObject o = QJsonDocument::fromJson(l).object();
    if (o.contains("ready")) {
        const QJsonObject r = o["ready"].toObject();
        params.clear();
        for (const auto &v : r["params"].toArray()) params.push_back(paramFromJson(v.toObject()));
        initValues();
        latency = r["latency"].toInt();
        m_hasEditor = r["editor"].toBool();
        m_ready = true;
    } else if (o.contains("error")) {
        m_error = o["error"].toString();
    } else if (o.contains("value")) {
        const QJsonArray a = o["value"].toArray();
        const int i = a.at(0).toInt();
        if (i >= 0 && i < paramCount()) {
            values[size_t(i)].store(float(a.at(1).toDouble()), std::memory_order_relaxed);
            if (onValueFromPlugin) onValueFromPlugin(i, float(a.at(1).toDouble()));
        }
    } else if (o.contains("latency")) {
        latency = o["latency"].toInt();
    }
}

void RemotePlugin::checkHung() {
    // A request that has waited 3 s with no sign of life from the helper's audio loop: it's hung.
    bridge::Shared *s = m_seg.get();
    const uint32_t alive = s->helperAlive.load(std::memory_order_relaxed);
    const bool waiting = s->state.load(std::memory_order_acquire) == bridge::Request;
    m_stuckTicks = waiting && alive == m_lastAlive ? m_stuckTicks + 1 : 0;
    m_lastAlive = alive;
    if (m_stuckTicks >= 3) {
        qWarning("plugin host [%s]: not responding, restarting it", qPrintable(info.name));
        m_proc.kill(); // finished() reports it as a crash
    }
}

// ---------------------------------------------------------------- helper process

int runPluginHost(int argc, char **argv) {
    prctl(PR_SET_PDEATHSIG, SIGKILL); // never outlive MicForge
    if (argc < 7) return 2;
    QGuiApplication app(argc, argv);
    const std::string shm = argv[2];
    PluginInfo info;
    info.format = QString::fromLocal8Bit(argv[3]);
    info.id = QString::fromLocal8Bit(argv[4]);
    info.path = QString::fromLocal8Bit(argv[5]);
    const double rate = QString::fromLocal8Bit(argv[6]).toDouble();
    auto say = [](const QJsonObject &o) { const QByteArray b = line(o); std::fwrite(b.constData(), 1, size_t(b.size()), stdout); std::fflush(stdout); };

    bridge::Segment seg;
    if (!seg.open(shm)) { say({{"error", "couldn't open shared memory"}}); return 3; }
    QString err;
    std::unique_ptr<PluginInstance> inst = createPlugin(info, rate, bridge::kMaxBlock, &err);
    if (!inst) { say({{"error", err.isEmpty() ? QStringLiteral("couldn't be loaded") : err}}); return 3; }

    QJsonArray ps;
    for (int i = 0; i < inst->paramCount(); ++i) ps << paramToJson(inst->param(i));
    say({{"ready", QJsonObject{{"params", ps}, {"latency", inst->latency}, {"editor", false}}}});

    // Audio loop on its own thread; ask for real-time priority (works if the user may, harmless if not).
    bridge::Shared &s = *seg.get();
    std::thread audio([&] {
        sched_param sp{};
        sp.sched_priority = 20;
        pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
        while (bridge::waitForRequest(s, 100'000'000)) {
            inst->process(s.audio, s.frames);
            bridge::finishRequest(s);
        }
    });

    // Control messages from MicForge on stdin.
    QByteArray pending;
    QSocketNotifier in(STDIN_FILENO, QSocketNotifier::Read);
    QObject::connect(&in, &QSocketNotifier::activated, &app, [&] {
        char buf[4096];
        const ssize_t n = read(STDIN_FILENO, buf, sizeof buf);
        if (n <= 0) { app.quit(); return; } // MicForge went away
        pending.append(buf, int(n));
        int nl;
        while ((nl = pending.indexOf('\n')) >= 0) {
            const QJsonObject o = QJsonDocument::fromJson(pending.left(nl)).object();
            pending.remove(0, nl + 1);
            if (o.contains("set")) {
                const QJsonArray a = o["set"].toArray();
                inst->setValue(a.at(0).toInt(), float(a.at(1).toDouble()));
            } else if (o.contains("quit")) {
                app.quit();
            }
        }
    });
    const int rc = app.exec();
    s.quit = 1;
    bridge::wake(s.state);
    audio.join();
    inst.reset();
    return rc;
}

} // namespace mf
