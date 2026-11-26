// A plugin running in its own "micforge --plugin-host" process (see docs/PLUGIN_HOSTING.md).
// To MicForge it looks like any other PluginInstance: process() exchanges each block with the helper
// through shared memory (bridge.h), with a deadline; parameters travel as JSON lines over the
// helper's stdin/stdout. If the helper crashes or hangs, the audio passes through dry and
// `onCrashed` tells the plugin manager, which restarts it.
#pragma once

#include "plugins/bridge.h"
#include "plugins/plugins.h"

#include <QObject>
#include <QProcess>
#include <QTimer>

#include <functional>

namespace mf {

class RemotePlugin final : public QObject, public PluginInstance {
    Q_OBJECT
public:
    // Starts the helper and waits (up to 15 s) until the plugin is loaded. nullptr + error on failure.
    static std::unique_ptr<RemotePlugin> create(const PluginInfo &info, double sampleRate, QString *error);
    ~RemotePlugin() override;

    void process(float *io, int n) override;      // audio thread

    bool hasEditor() const { return m_hasEditor; }
    void showEditor(bool show);                    // the plugin's own window (in the helper)
    uint64_t missedBlocks() const { return m_missed.load(std::memory_order_relaxed); }
    qint64 helperPid() const { return m_proc.processId(); }

    std::function<void(const QString &why)> onCrashed;       // UI thread
    std::function<void(int index, float value)> onValueFromPlugin; // a change made in the plugin's window

protected:
    void onValueSet(int i, float v) override;

private:
    RemotePlugin() = default;
    void send(const QByteArray &jsonLine);
    void readOutput();
    void handle(const QByteArray &line);
    void checkHung();

    QProcess m_proc;
    bridge::Segment m_seg;
    double m_rate = 48000;
    bool m_ready = false, m_hasEditor = false;
    QString m_error;
    QByteArray m_pending;
    std::atomic<bool> m_alive{false};
    std::atomic<uint64_t> m_missed{0};
    QTimer m_watch;
    uint32_t m_lastAlive = 0;
    int m_stuckTicks = 0;
};

// Helper-process entry point: micforge --plugin-host <shm> <format> <id> <path> <rate>
int runPluginHost(int argc, char **argv);

} // namespace mf
