// Per-microphone Setup profiles. Each microphone keeps its own Setup result: the measured zones
// (settings "calib/*"), the parameters Setup chose (gain, gate, compressor, mic type…) and its three
// recordings. Switching microphones saves the outgoing mic's profile and restores the incoming one's.
// Profiles live in <calib dir>/devices/<id>/ and are never deleted automatically. Qt Core only.
#pragma once

#include <QDateTime>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class CalibrationProfiles {
public:
    // `calibDir` holds the active recordings (room/you/others.f32); `settings` the active calib/* keys.
    CalibrationProfiles(QString calibDir, QSettings &settings) : m_dir(std::move(calibDir)), m_settings(settings) {}

    // Parameters that belong to a microphone's Setup rather than to the sound preset.
    static const QStringList &profileParams();

    bool has(const QString &device) const;
    // Save the active Setup (calib/* + `params` + recordings) as `device`'s profile.
    bool save(const QString &device, const QString &description, const QVariantMap &params);
    // Make `device`'s profile the active Setup: copies its recordings and calib/* back, returns its
    // parameters to apply (empty map if there is no profile).
    QVariantMap restore(const QString &device);
    // [{device, description, savedAt}] for the UI.
    QVariantList list() const;

    QString dirFor(const QString &device) const;

private:
    QString m_dir;
    QSettings &m_settings;
};
