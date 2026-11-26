// Live spectrum of what the Voice Gate hears, plus the average spectra of the Setup recordings
// (your voice, other voices, room), for the Voice Isolation page. Only runs while the view is visible.
#pragma once

#include "audio/audiocore.h"

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <vector>

class DetectorSpectrum : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QVariantList live READ live NOTIFY liveChanged)              // dB per band
    Q_PROPERTY(QVariantMap recorded READ recorded NOTIFY recordedChanged)   // {you, others, room}: dB per band
    Q_PROPERTY(qreal lowHz READ lowHz CONSTANT)
    Q_PROPERTY(qreal highHz READ highHz CONSTANT)

public:
    // `loadRecording(name)` returns a saved Setup recording ("you", "others", "room"), or empty.
    DetectorSpectrum(mf::AudioCore &core, std::function<std::vector<float>(const QString &)> loadRecording, QObject *parent = nullptr);
    static constexpr int kBands = 96;

    bool active() const { return m_timer.isActive(); }
    void setActive(bool on);
    QVariantList live() const { return m_live; }
    QVariantMap recorded() const { return m_recorded; }
    qreal lowHz() const { return 50; }
    qreal highHz() const { return 8000; }

    // Call when the Setup recordings change.
    void recordingsChanged() { m_recordedStale = true; if (active()) updateRecorded(); }

signals:
    void activeChanged();
    void liveChanged();
    void recordedChanged();

private:
    void tick();
    void updateRecorded();

    mf::AudioCore &m_core;
    std::function<std::vector<float>(const QString &)> m_load;
    QTimer m_timer;
    QVariantList m_live;
    QVariantMap m_recorded;
    std::vector<float> m_smooth;
    bool m_recordedStale = true;
};
