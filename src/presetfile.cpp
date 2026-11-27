#include "presetfile.h"

#include "params.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

namespace mf {

bool isSetupKey(const QString &k) {
    static const QStringList keys = {
        "mute", "bypass", "inputChannel", "inputGain", "micType", "needOthers", "needNoise", "needBroadcast",
        "gateThreshold", "gateHysteresis", "gateFocus", "gateScLow", "gateScHigh", "gateCloseCheck",
        "gateTilt", "gateTiltDir"};
    return keys.contains(k);
}

namespace {
int idOf(const QString &key) {
    for (int i = 0; i < PARAM_COUNT; ++i)
        if (key == QLatin1String(kParams[i].key)) return i;
    return -1;
}
} // namespace

QByteArray PresetFile::toJson(const QString &appVersion) const {
    QJsonObject o;
    o["format"] = "micforge-preset";
    o["version"] = kVersion;
    o["name"] = name;
    o["createdWith"] = "MicForge " + appVersion;
    QJsonObject p;
    for (auto it = params.begin(); it != params.end(); ++it)
        if (idOf(it.key()) >= 0 && !isSetupKey(it.key())) p[it.key()] = it.value().toDouble();
    o["params"] = p;
    o["plugins"] = QJsonArray::fromVariantList(plugins);
    return QJsonDocument(o).toJson(QJsonDocument::Indented);
}

bool PresetFile::fromJson(const QByteArray &data, PresetFile &out, QString *error) {
    auto fail = [&](const QString &why) { if (error) *error = why; return false; };
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) return fail(QStringLiteral("This isn't a MicForge preset (the file can't be read as one)."));
    const QJsonObject o = doc.object();

    out = PresetFile{};
    QJsonObject values;
    if (o.contains("format")) {
        if (o["format"].toString() != "micforge-preset") return fail(QStringLiteral("This isn't a MicForge preset."));
        out.version = o["version"].toInt(0);
        if (out.version < 2) return fail(QStringLiteral("This preset file is damaged (no version)."));
        if (out.version > kVersion)
            return fail(QStringLiteral("This preset was made with a newer MicForge (format %1). Update MicForge to use it.").arg(out.version));
        out.name = o["name"].toString();
        values = o["params"].toObject();
        if (o.contains("plugins")) {
            out.hasPlugins = true;
            for (const QJsonValue &v : o["plugins"].toArray()) {
                const QJsonObject p = v.toObject();
                if (p["format"].toString().isEmpty() || p["id"].toString().isEmpty()) continue;
                out.plugins << QVariantMap{{"format", p["format"].toString()}, {"id", p["id"].toString()},
                                           {"name", p["name"].toString()}, {"bypass", p["bypass"].toBool()},
                                           {"params", p["params"].toObject().toVariantMap()}};
            }
        }
    } else {
        out.version = 1; // before Phase 5: a flat object of every parameter
        values = o;
    }
    for (auto it = values.begin(); it != values.end(); ++it) {
        const int id = idOf(it.key());
        if (id < 0 || isSetupKey(it.key()) || !it.value().isDouble()) { out.ignored << it.key(); continue; }
        out.params[it.key()] = std::clamp(it.value().toDouble(), double(kParams[id].min), double(kParams[id].max));
    }
    // Presets from before 0.3.0 have no "vcFormant": their formants followed the pitch.
    if (out.params.contains("vcPitch") && !out.params.contains("vcFormant")) out.params["vcFormant"] = out.params["vcPitch"];
    if (out.params.isEmpty() && !out.hasPlugins) return fail(QStringLiteral("This preset contains no settings MicForge knows."));
    return true;
}

} // namespace mf
