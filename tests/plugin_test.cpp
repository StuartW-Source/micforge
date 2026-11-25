// Plugin host test: scan LV2 + a VST3 bundle, load plugins, process audio, change parameters.
// Built by tests/build_plugin_test.sh.
#include "plugins/plugins.h"

#include <QCoreApplication>

#include <cmath>
#include <cstdio>
#include <vector>

using namespace mf;

static int failures = 0;
static void check(bool ok, const char *what, double v = 0) {
    std::printf("[%s] %-62s %10.3f\n", ok ? " OK " : "FAIL", what, v);
    if (!ok) ++failures;
}

static double rms(const std::vector<float> &v, size_t a, size_t b) {
    double s = 0;
    for (size_t i = a; i < b; ++i) s += double(v[i]) * v[i];
    return std::sqrt(s / double(b - a));
}

static void runBlocks(PluginInstance &p, std::vector<float> &buf, int block) {
    for (size_t i = 0; i < buf.size(); i += size_t(block)) p.process(buf.data() + i, int(std::min<size_t>(size_t(block), buf.size() - i)));
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const int fs = 48000, block = 256;

    // ---------------------------------------------------------------- LV2
    const auto lv2 = scanLv2();
    check(!lv2.empty(), "LV2: found installed effect plugins", double(lv2.size()));
    int loaded = 0, refused = 0;
    const PluginInfo *firstLoaded = nullptr;
    for (const auto &info : lv2) {
        // Instantiate a sample of the installed plugins to make sure the host copes broadly.
        if (loaded + refused >= 40) break;
        QString err;
        auto inst = createPlugin(info, fs, 1024, &err);
        if (!inst) { ++refused; continue; }
        std::vector<float> buf(static_cast<size_t>(fs / 4));
        for (size_t i = 0; i < buf.size(); ++i) buf[i] = 0.2f * std::sin(2 * M_PI * 440 * double(i) / fs);
        runBlocks(*inst, buf, block);
        bool finite = true;
        for (float x : buf) finite &= std::isfinite(x);
        if (finite) { ++loaded; if (!firstLoaded) firstLoaded = &info; }
    }
    std::printf("       LV2 sample: %d loaded and processed cleanly, %d politely refused\n", loaded, refused);
    check(loaded > 10, "LV2: most plugins load and process finite audio", loaded);

    // A specific, predictable LV2 test: Calf Filter as a 300 Hz low-pass should cut a 5 kHz tone.
    for (const auto &info : lv2) {
        if (info.id != "http://calf.sourceforge.net/plugins/Filter") continue;
        QString err;
        auto f = createPlugin(info, fs, 1024, &err);
        check(f != nullptr, "LV2: Calf Filter instantiated");
        if (!f) break;
        int iFreq = f->indexOf("freq"), iMode = f->indexOf("mode");
        std::printf("       Calf Filter: %d parameters, freq=%d mode=%d\n", f->paramCount(), iFreq, iMode);
        if (iFreq >= 0) f->setValue(iFreq, 300);
        if (iMode >= 0) f->setValue(iMode, 1); // 24 dB/oct low-pass
        std::vector<float> buf(static_cast<size_t>(fs));
        for (size_t i = 0; i < buf.size(); ++i) buf[i] = 0.3f * std::sin(2 * M_PI * 5000 * double(i) / fs);
        const double before = rms(buf, buf.size() / 2, buf.size());
        runBlocks(*f, buf, block);
        const double after = rms(buf, buf.size() / 2, buf.size());
        check(after < before * 0.1, "LV2: parameter change works (5 kHz cut > 20 dB at 300 Hz)", 20 * std::log10(after / before));
        check(!f->valueText(iFreq, 300).isEmpty(), "LV2: value text for display", 0);
        break;
    }

    // ---------------------------------------------------------------- VST3
    if (argc > 1) {
        const auto classes = scanVst3Bundle(argv[1]);
        check(classes.size() == 1, "VST3: scan finds the ADelay effect", double(classes.size()));
        if (!classes.empty()) {
            QString err;
            auto d = createPlugin(classes[0], fs, 1024, &err);
            if (!d) std::printf("       error: %s\n", qPrintable(err));
            check(d != nullptr, "VST3: plugin instantiated and activated");
            if (d) {
                std::printf("       ADelay: %d parameters:", d->paramCount());
                for (int i = 0; i < d->paramCount(); ++i) std::printf(" '%s'", qPrintable(d->param(i).name));
                std::printf("\n");
                int iDelay = -1;
                for (int i = 0; i < d->paramCount(); ++i) if (d->param(i).name.contains("Delay", Qt::CaseInsensitive)) iDelay = i;
                check(iDelay >= 0, "VST3: Delay parameter listed");
                if (iDelay >= 0) {
                    d->setValue(iDelay, 0.25f); // 0.25 s
                    std::printf("       Delay value text at 0.25: '%s'\n", qPrintable(d->valueText(iDelay, 0.25f)));
                    std::vector<float> buf(static_cast<size_t>(fs));
                    buf[1000] = 1.0f;
                    runBlocks(*d, buf, block);
                    size_t at = 0; float mx = 0;
                    for (size_t i = 0; i < buf.size(); ++i) if (std::fabs(buf[i]) > mx) { mx = std::fabs(buf[i]); at = i; }
                    check(std::abs(long(at) - long(1000 + fs / 4)) < 64, "VST3: audio processed with the new delay (ms)", (double(at) - 1000) / 48.0);
                }
            }
        }
    }

    std::printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
