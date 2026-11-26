// Manual check (opens a plugin window on the desktop, so it is not part of ctest):
//   MICFORGE_HOST_BINARY=build-cmake/micforge build-cmake/plugin_editor_check
// Needs lsp-plugins-vst3. Loads "Compressor Mono" in a separate process and opens its window for 4 s.
#include "plugins/remoteplugin.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QProcess>
#include <QThread>
#include <cstdio>
#include <vector>
using namespace mf;
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    PluginInfo info; info.format = "VST3"; info.id = "647370206B316D202020202062677379"; info.path = "/usr/lib/vst3/lsp-plugins.vst3"; info.name = "Compressor Mono";
    QString err;
    auto r = RemotePlugin::create(info, 48000, &err);
    if (!r) { std::printf("create failed: %s\n", qPrintable(err)); return 1; }
    std::printf("loaded in pid %lld, %d settings, window available: %s\n", r->helperPid(), r->paramCount(), r->hasEditor() ? "yes" : "no");
    bool closed = false, crashed = false; int edits = 0;
    r->onEditorClosed = [&] { closed = true; };
    r->onCrashed = [&](const QString &w) { crashed = true; std::printf("crashed: %s\n", qPrintable(w)); };
    r->onValueFromPlugin = [&](int, float) { ++edits; };
    r->onLog = [](const QString &m) { std::printf("helper: %s\n", qPrintable(m)); std::fflush(stdout); };
    std::vector<float> buf(256, 0.1f);
    r->showEditor(true);
    QElapsedTimer t; t.start();
    int blocks = 0;
    while (t.elapsed() < 4000) { r->process(buf.data(), 256); ++blocks; app.processEvents(); QThread::usleep(5000); }
    // A setting changed from MicForge while the window is open:
    r->setValue(0, 0.7f);
    for (int i = 0; i < 50; ++i) { r->process(buf.data(), 256); app.processEvents(); QThread::usleep(5000); }
    std::printf("after 4 s with the window open: crashed=%d, closed by itself=%d, blocks=%d, missed=%llu\n", crashed, closed, blocks, (unsigned long long)r->missedBlocks());
    r->showEditor(false);
    for (int i = 0; i < 40 && !closed; ++i) { app.processEvents(); QThread::msleep(25); }
    std::printf("window closed on request: %s\n", closed ? "yes" : "no");
    r.reset();
    return 0;
}
