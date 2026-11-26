// Checks that diagnostic reports can't leak the user's name, computer name, home folder or file paths.
#include "privacy.h"

#include <cstdio>

using namespace mf::privacy;

static int failures = 0;
static void check(bool ok, const char *what, const QString &got) {
    std::printf("[%s] %-52s %s\n", ok ? " OK " : "FAIL", what, qPrintable(got));
    if (!ok) ++failures;
}

int main() {
    const QString home = "/home/alex", user = "alex", host = "alex-desktop";
    QString t = scrub("Log file: /home/alex/.local/state/MicForge/micforge.log", home, user, host);
    check(t == "Log file: ~/.local/state/MicForge/micforge.log", "home folder becomes ~", t);
    t = scrub("user alex on alex-desktop, ALEX", home, user, host);
    check(t == "user <user> on <computer>, <user>", "user and computer names removed (any case)", t);
    t = scrub("alexander and malexa stay", home, user, host);
    check(t == "alexander and malexa stay", "only whole-word matches are replaced", t);
    t = scrub("/tmp/build-alex/x", home, user, host);
    check(!t.contains("alex"), "name inside other paths removed", t);
    t = stripPaths("This plugin couldn't be loaded: /home/alex/vst/ADelay.vst3 is not a module directory.");
    check(t == "This plugin couldn't be loaded: …/ADelay.vst3 is not a module directory.", "plugin path reduced to file name", t);
    t = stripPaths("“~/Music/My Plugins/x.lv2” failed");
    check(!t.contains("Music"), "~ paths stripped too", t);
    check(deviceKind("bluez_input.12_34_56") == "Bluetooth" && deviceKind("alsa_input.usb-PreSonus_AudioBox") == "USB"
              && deviceKind("alsa_input.pci-0000_00_1f.3.analog-stereo") == "built-in",
          "device names reduced to their type", deviceKind("bluez_input.x"));
    std::printf("\nprivacy tests: %s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
