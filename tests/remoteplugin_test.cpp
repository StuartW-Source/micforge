// Out-of-process plugins, end to end with a real LV2 plugin (Calf Filter): same sound as in-process,
// parameter changes reach the helper, a killed helper never takes the caller down (audio passes dry),
// and the cost per block. Skipped if Calf or lilv isn't installed.
#include "plugins/remoteplugin.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <csignal>
#include <vector>

using namespace mf;

static int failures = 0;
static void check(bool ok, const char *what, double v) {
    std::printf("[%s] %-60s %10.3f\n", ok ? " OK " : "FAIL", what, v);
    if (!ok) ++failures;
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    if (!lv2Available()) { std::puts("[SKIP] lilv not installed"); return 0; }
    PluginInfo info;
    info.format = "LV2";
    info.id = "http://calf.sourceforge.net/plugins/Filter";
    info.name = "Calf Filter";
    QString err;
    auto local = createPlugin(info, 48000, PluginRack::kMaxBlock, &err);
    if (!local) { std::printf("[SKIP] Calf Filter not available: %s\n", qPrintable(err)); return 0; }
    if (qEnvironmentVariable("MICFORGE_HOST_BINARY").isEmpty()) { std::puts("[SKIP] set MICFORGE_HOST_BINARY to build/micforge"); return 0; }

    auto remote = RemotePlugin::create(info, 48000, &err);
    check(remote != nullptr, "plugin starts in its own process", remote ? double(remote->helperPid()) : 0);
    if (!remote) { std::printf("       %s\n", qPrintable(err)); return 1; }
    check(remote->paramCount() == local->paramCount() && remote->paramCount() > 0, "same settings as in-process (count)", remote->paramCount());
    bool crashed = false;
    remote->onCrashed = [&](const QString &) { crashed = true; };

    // Same input through both; also move the cutoff on both halfway through.
    const int freq = local->indexOf("freq");
    std::vector<float> a(256), b(256);
    unsigned seed = 1;
    double maxDiff = 0, energy = 0, diffBefore = 0;
    for (int blk = 0; blk < 400; ++blk) {
        // Settings reach the separate process asynchronously (a few ms): give it time before the next block.
        if (blk == 200 && freq >= 0) { local->setValue(freq, 500); remote->setValue(freq, 500); QThread::msleep(50); app.processEvents(); }
        for (int i = 0; i < 256; ++i) { seed = seed * 1664525u + 1013904223u; a[size_t(i)] = b[size_t(i)] = ((seed >> 8) / float(1u << 24) - 0.5f) * 0.5f; }
        local->process(a.data(), 256);
        remote->process(b.data(), 256);
        for (int i = 0; i < 256; ++i) { maxDiff = std::max(maxDiff, double(std::fabs(a[size_t(i)] - b[size_t(i)]))); energy += double(a[size_t(i)]) * a[size_t(i)]; }
        if (blk < 200) diffBefore = maxDiff;
        if (blk % 20 == 0) app.processEvents();
    }
    std::printf("       difference before the setting change %.6f, overall %.6f\n", diffBefore, maxDiff);
    check(energy > 0 && maxDiff < 1e-6, "identical sound in its own process (max difference)", maxDiff);
    check(remote->missedBlocks() == 0, "no missed blocks", double(remote->missedBlocks()));

    // Cost: time per block, in-process vs separate.
    auto timeIt = [&](PluginInstance &p) {
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < 2000; ++k) p.process(a.data(), 256);
        return std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / 2000;
    };
    const double tl = timeIt(*local), tr = timeIt(*remote);
    std::printf("       per 256-frame block: in-process %.1f us, separate %.1f us (+%.1f us; block lasts 5333 us)\n", tl, tr, tr - tl);
    check(tr - tl < 200, "extra time per block for running separately (us)", tr - tl);

    // Kill the helper: the caller survives, audio passes through dry, the crash is reported.
    ::kill(pid_t(remote->helperPid()), SIGKILL);
    QElapsedTimer t; t.start();
    while (!crashed && t.elapsed() < 3000) { app.processEvents(); QThread::msleep(5); }
    check(crashed, "a killed plugin process is reported", crashed);
    std::vector<float> dry(256, 0.25f);
    remote->process(dry.data(), 256);
    check(dry[0] == 0.25f && dry[255] == 0.25f, "after the crash, audio passes through untouched", dry[0]);
    remote.reset();
    std::printf("\nremote plugin tests: %s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
