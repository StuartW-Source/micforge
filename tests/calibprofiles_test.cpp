// Per-microphone Setup profiles: save, switch, restore, never lose the other mic's Setup.
#include "calibprofiles.h"

#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>

#include <cstdio>

static int failures = 0;
static void check(bool ok, const char *what) {
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++failures;
}
static void writeFile(const QString &path, const QByteArray &data) {
    QFile f(path);
    if (f.open(QIODevice::WriteOnly)) f.write(data);
}
static QByteArray readFile(const QString &path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir tmp;
    QSettings s(tmp.filePath("settings.ini"), QSettings::IniFormat);
    const QString calib = tmp.filePath("calib");
    QDir().mkpath(calib);
    CalibrationProfiles prof(calib, s);

    // Mic A calibrated.
    s.setValue("calib/valid", true);
    s.setValue("calib/you", -20.0);
    writeFile(calib + "/room.f32", "roomA");
    writeFile(calib + "/you.f32", "youA");
    writeFile(calib + "/others.f32", "othersA");
    check(prof.save("micA", "Mic A", {{"inputGain", 12.0}, {"gateThreshold", -40.0}, {"compRatio", 4.0}}), "save mic A");
    check(prof.has("micA") && !prof.has("micB"), "profile exists only for mic A");

    // Mic B calibrated (no "others" take), saved.
    s.setValue("calib/you", -30.0);
    writeFile(calib + "/room.f32", "roomB");
    writeFile(calib + "/you.f32", "youB");
    QFile::remove(calib + "/others.f32");
    check(prof.save("micB", "Mic B", {{"inputGain", 20.0}}), "save mic B");

    // Back to A: everything of A returns, B's profile is untouched.
    QVariantMap pa = prof.restore("micA");
    check(readFile(calib + "/you.f32") == "youA" && readFile(calib + "/others.f32") == "othersA", "restore A: A's recordings active");
    check(s.value("calib/you").toDouble() == -20.0 && s.value("calib/device").toString() == "micA", "restore A: A's zones active");
    check(pa.value("inputGain").toDouble() == 12.0 && pa.value("gateThreshold").toDouble() == -40.0, "restore A: A's parameters returned");
    check(!pa.contains("compRatio"), "sound-preset parameters are not part of a mic profile");
    check(readFile(prof.dirFor("micB") + "/you.f32") == "youB", "B's saved profile unchanged");

    // And to B: A's "others" take must not leak into B.
    QVariantMap pb = prof.restore("micB");
    check(readFile(calib + "/you.f32") == "youB" && !QFile::exists(calib + "/others.f32"), "restore B: no leftover take from A");
    check(pb.value("inputGain").toDouble() == 20.0, "restore B: B's gain");
    check(prof.list().size() == 2, "both profiles listed");
    check(prof.restore("unknown").isEmpty() && readFile(calib + "/you.f32") == "youB", "unknown mic changes nothing");

    s.setValue("calib/valid", false);
    check(!prof.save("micC", "Mic C", {}), "nothing saved without a valid Setup");

    std::printf("\ncalibration profile tests: %s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
