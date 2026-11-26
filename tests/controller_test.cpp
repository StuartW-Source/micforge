// Controller-level tests: undo/redo, presets (Setup values never travel), import/export.
// Runs with private, temporary settings/data folders and its own virtual-mic name; never touches the
// user's MicForge settings. The audio engine isn't started (no pw_init), so PipeWire is never touched.
#include "controller.h"
#include "params.h"

#include <QFile>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>

#include <cstdio>

// Stand-in for the desktop portal: records what MicForge asks for and lets the test press keys.
struct FakeShortcuts : mf::IGlobalShortcuts {
    int binds = 0;
    QStringList ids;
    void bind(const QVector<Shortcut> &s) override { ++binds; ids.clear(); for (const auto &x : s) ids << x.id; }
    bool available() const override { return true; }
    QString status() const override { return "Ready."; }
    QMap<QString, QString> triggers() const override { return {{"mute", "Ctrl+Alt+M"}}; }
};
static FakeShortcuts *fake = nullptr;

static int failures = 0;
static void check(bool ok, const char *what) {
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++failures;
}

int main(int argc, char **argv) {
    QTemporaryDir home;
    qputenv("XDG_CONFIG_HOME", home.filePath("config").toUtf8());
    qputenv("XDG_DATA_HOME", home.filePath("data").toUtf8());
    qputenv("XDG_STATE_HOME", home.filePath("state").toUtf8());
    qputenv("MICFORGE_TEST", "1");
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setOrganizationName("MicForge");
    app.setApplicationName("MicForgeControllerTest");

    mf::IGlobalShortcuts::factoryOverride() = [] { auto f = std::make_unique<FakeShortcuts>(); fake = f.get(); return f; };

    StartupInfo info;
    info.backend = {"micforge_ctltest", "MicForge controller test (ignore)"};
    Controller c(info);
    auto p = [&](const char *k) { return c.params()->value(k).toDouble(); };

    // ---- undo / redo
    check(c.undoText().isEmpty(), "nothing to undo at start");
    const double ratio0 = p("compRatio");
    c.setParam("compRatio", ratio0 + 2);
    check(c.undoText() == "Compressor", "a change can be undone ('Undo Compressor')");
    c.undo();
    check(p("compRatio") == ratio0 && c.redoText() == "Compressor", "undo restores the value; redo offered");
    c.redo();
    check(p("compRatio") == ratio0 + 2, "redo re-applies");
    c.setParam("mute", 1);
    c.setParam("mute", 0);
    check(c.undoText() == "Compressor", "mute isn't part of undo history");

    // ---- presets never carry Setup values
    c.setParam("inputGain", 20);
    c.setParam("gateThreshold", -42);
    c.setParam("reverbAdded", 1);
    c.setParam("reverbMix", 33);
    c.savePreset("Mine");
    QFile f(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/presets/Mine.json");
    check(f.open(QIODevice::ReadOnly), "preset saved to the presets folder");
    const QByteArray json = f.readAll();
    check(json.contains("\"version\": 2") && json.contains("reverbMix") && !json.contains("inputGain") && !json.contains("gateThreshold"),
          "saved preset: version 2, sound settings, no Setup values");
    c.setParam("inputGain", 5);
    c.setParam("gateThreshold", -30);
    c.setParam("reverbMix", 10);
    c.loadPreset("Mine");
    check(p("reverbMix") == 33 && p("inputGain") == 5 && p("gateThreshold") == -30, "loading a preset keeps your Setup values");
    check(c.undoText().startsWith("Load preset"), "a preset load is one undo step");
    c.undo();
    check(p("reverbMix") == 10, "undoing the preset load restores the previous sound");

    // ---- an old (version 1) preset that contains Setup values
    {
        QFile old(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/presets/Old.json");
        old.open(QIODevice::WriteOnly);
        old.write(R"({"reverbMix": 44, "inputGain": 30, "gateThreshold": -60})");
    }
    c.loadPreset("Old");
    check(p("reverbMix") == 44 && p("inputGain") == 5 && p("gateThreshold") == -30, "old presets load without overwriting Setup");

    // ---- export / import
    const QString exported = home.filePath("shared.json");
    check(c.exportPreset("Mine", QUrl::fromLocalFile(exported)).startsWith("Exported"), "export to a file");
    check(c.importPreset(QUrl::fromLocalFile(exported)).contains("Mine (2)"), "import never overwrites: picks 'Mine (2)'");
    check(c.presetNames().contains("Mine") && c.presetNames().contains("Mine (2)"), "both presets listed");
    {
        QFile junk(home.filePath("junk.json"));
        junk.open(QIODevice::WriteOnly);
        junk.write("{ not a preset");
    }
    check(!c.importPreset(QUrl::fromLocalFile(home.filePath("junk.json"))).startsWith("Imported"), "a bad file is refused");

    // ---- global shortcuts and push-to-talk (fake desktop portal)
    check(fake && fake->binds == 0, "shortcuts: nothing is bound until switched on (no desktop dialog)");
    c.setShortcutsEnabled(true);
    check(fake->binds == 1 && fake->ids.contains("mute") && fake->ids.contains("push-to-talk"), "shortcuts: mute and push-to-talk requested");
    check(c.muteKey() == "Ctrl+Alt+M", "shortcuts: the desktop's key is shown");
    fake->onActivated("mute");
    check(c.muted(), "mute shortcut mutes");
    fake->onActivated("mute");
    check(!c.muted(), "...and unmutes");
    c.setPushToTalk(true);
    check(c.muted(), "push-to-talk: silent until the key is held");
    fake->onActivated("push-to-talk");
    check(!c.muted(), "push-to-talk: key held -> live");
    fake->onDeactivated("push-to-talk");
    check(c.muted(), "push-to-talk: key released -> silent");
    c.setShortcutsEnabled(false);
    check(!c.muted(), "turning shortcuts off never leaves you stuck muted");
    fake->onActivated("push-to-talk");
    c.setPushToTalk(false);

    std::printf("\ncontroller tests: %s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
