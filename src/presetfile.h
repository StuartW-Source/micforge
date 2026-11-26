// Sound preset files: versioned JSON that is the same on every platform (no file paths, no device names).
//
//   { "format": "micforge-preset", "version": 2, "name": "...", "createdWith": "MicForge 0.1.1",
//     "params": { "compRatio": 3, ... },                  // sound settings only (see isSetupKey)
//     "plugins": [ { "format": "LV2", "id": "...", "name": "...", "bypass": false, "params": {...} } ] }
//
// Version 1 files (before Phase 5) are a flat object of every parameter; they still load, minus the
// microphone/room ("Setup") values they wrongly contained. Qt Core only.
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace mf {

// Parameters that belong to the microphone, room and Setup, never to a sound preset.
bool isSetupKey(const QString &key);

struct PresetFile {
    static constexpr int kVersion = 2;
    QString name;
    QVariantMap params;      // key -> double (only known, non-Setup keys, clamped to their range)
    QVariantList plugins;    // empty list = "no plugins"; see hasPlugins
    bool hasPlugins = false; // false for version 1 files: leave the user's plugins alone
    int version = kVersion;
    QStringList ignored;     // keys that were unknown or not allowed in a preset

    QByteArray toJson(const QString &appVersion) const;
    // On failure returns false and sets `error` to a plain-English reason.
    static bool fromJson(const QByteArray &data, PresetFile &out, QString *error);
};

} // namespace mf
