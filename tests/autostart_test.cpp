// XDG autostart manager test. Runs as the test instance, so the real login entry is never touched.
// Build: g++ -std=c++20 -fPIC -Isrc $(pkg-config --cflags Qt6Core) tests/autostart_test.cpp src/platform/autostart_xdg.cpp $(pkg-config --libs Qt6Core) -o /tmp/autostart_test
#include "platform/autostart.h"

#include <QCoreApplication>
#include <QFile>
#include <cstdio>

static int failures = 0;
static void check(bool ok, const char *what) {
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++failures;
}

int main(int argc, char **argv) {
    qputenv("MICFORGE_TEST", "1");
    QCoreApplication app(argc, argv);
    app.setOrganizationName("MicForge");
    app.setApplicationName("MicForgeTest");
    auto a = mf::createAutostart();
    const QString path = a->describe();
    check(!path.contains("/.config/autostart/"), "test instance uses its own entry, not the real login entry");
    a->setEnabled(false, true);
    check(!a->enabled(), "disabled: no entry");
    a->setEnabled(true, true);
    QFile f(path);
    const QByteArray body = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
    check(a->enabled(), "enabled: entry exists");
    check(body.contains("Exec=\"" + QCoreApplication::applicationFilePath().toUtf8() + "\" --minimized"), "entry runs this binary, hidden");
    a->setEnabled(true, false);
    f.close();
    f.open(QIODevice::ReadOnly);
    check(!f.readAll().contains("--minimized"), "start-visible option removes --minimized");
    a->setEnabled(false, true);
    check(!a->enabled() && !QFile::exists(path), "disabling removes the entry");
    std::printf("\nautostart tests: %s\n", failures ? "FAILED" : "ALL PASSED");
    return failures ? 1 : 0;
}
