// Sound preset files: round trip, old (version 1) files, Setup values never travel, bad files rejected.
#include "presetfile.h"

#include <cstdio>

using namespace mf;

static int failures = 0;
static void check(bool ok, const char *what) {
    std::printf("[%s] %s\n", ok ? " OK " : "FAIL", what);
    if (!ok) ++failures;
}

int main() {
    PresetFile p;
    p.name = "Late night";
    p.params = {{"compRatio", 4.0}, {"reverbMix", 20.0}, {"inputGain", 18.0}, {"gateThreshold", -40.0}, {"echoAdded", 1.0}};
    p.plugins = {QVariantMap{{"format", "LV2"}, {"id", "urn:x"}, {"name", "Filter"}, {"bypass", false}, {"params", QVariantMap{{"freq", 300.0}}}}};
    const QByteArray json = p.toJson("0.1.1");
    PresetFile q;
    QString err;
    check(PresetFile::fromJson(json, q, &err), "round trip: loads");
    check(q.version == 2 && q.name == "Late night", "round trip: version and name");
    check(q.params.value("compRatio").toDouble() == 4.0 && q.params.value("echoAdded").toDouble() == 1.0, "round trip: sound settings and effect chain kept");
    check(!q.params.contains("inputGain") && !q.params.contains("gateThreshold"), "Setup values (mic gain, gate threshold) are never written");
    check(q.hasPlugins && q.plugins.size() == 1 && q.plugins[0].toMap().value("id") == "urn:x", "plugins kept, without file paths");
    check(!json.contains("/home") && !json.contains("path"), "no paths in the file");

    PresetFile v1;
    check(PresetFile::fromJson(R"({"compRatio": 2.5, "inputGain": 20, "gateThreshold": -35, "bypass": 1, "oldThing": 3})", v1, &err),
          "version 1 file loads");
    check(v1.version == 1 && v1.params.value("compRatio").toDouble() == 2.5, "version 1: sound settings kept");
    check(!v1.params.contains("inputGain") && !v1.params.contains("bypass") && v1.ignored.contains("oldThing"),
          "version 1: Setup values, bypass and unknown keys ignored");
    check(!v1.hasPlugins, "version 1: the user's plugins are left alone");

    PresetFile c;
    check(PresetFile::fromJson(R"({"format":"micforge-preset","version":2,"params":{"compRatio":999}})", c, &err) &&
              c.params.value("compRatio").toDouble() <= 20, "values clamped to their range");
    PresetFile bad;
    check(!PresetFile::fromJson("not json", bad, &err) && !err.isEmpty(), "garbage rejected with a reason");
    check(!PresetFile::fromJson(R"({"format":"micforge-preset","version":3,"params":{}})", bad, &err) && err.contains("newer"),
          "a newer format is refused politely");
    check(!PresetFile::fromJson(R"({"format":"something-else","version":2})", bad, &err), "other apps' JSON rejected");
    std::printf("\npreset file tests: %s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    { // Presets from before the voice-size control keep their sound: formants follow the pitch.
        PresetFile old;
        check(PresetFile::fromJson(R"({"format":"micforge-preset","version":2,"name":"Old","params":{"vcPitch":5,"vcAdded":1}})", old, &err)
              && old.params.value("vcFormant").toDouble() == 5.0, "old preset: vcFormant follows vcPitch");
        PresetFile now;
        check(PresetFile::fromJson(R"({"format":"micforge-preset","version":2,"name":"New","params":{"vcPitch":5,"vcFormant":2}})", now, &err)
              && now.params.value("vcFormant").toDouble() == 2.0, "new preset: vcFormant kept");
    }
    return failures ? 1 : 0;
}
