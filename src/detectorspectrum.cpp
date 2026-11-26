#include "detectorspectrum.h"

#include "spectrum.h"

using namespace mf;

DetectorSpectrum::DetectorSpectrum(AudioCore &core, std::function<std::vector<float>(const QString &)> load, QObject *parent)
    : QObject(parent), m_core(core), m_load(std::move(load)) {
    m_timer.setInterval(66); // ~15 updates a second
    connect(&m_timer, &QTimer::timeout, this, &DetectorSpectrum::tick);
}

void DetectorSpectrum::setActive(bool on) {
    if (on == active()) return;
    m_core.scope.on.store(on, std::memory_order_relaxed);
    if (on) {
        if (m_recordedStale) updateRecorded();
        m_timer.start();
    } else {
        m_timer.stop();
    }
    emit activeChanged();
}

void DetectorSpectrum::tick() {
    std::vector<float> x(4096);
    m_core.scope.latest(x.data(), x.size());
    const auto bands = logBandsDb(powerSpectrum(x.data(), x.size()), AudioCore::kRate, float(lowHz()), float(highHz()), kBands);
    if (m_smooth.size() != bands.size()) m_smooth = bands;
    QVariantList out;
    for (size_t i = 0; i < bands.size(); ++i) {
        // Rise at once, fall gently, so the display is readable.
        m_smooth[i] = bands[i] > m_smooth[i] ? bands[i] : m_smooth[i] - 1.5f;
        out << double(m_smooth[i]);
    }
    m_live = out;
    emit liveChanged();
}

void DetectorSpectrum::updateRecorded() {
    m_recordedStale = false;
    QVariantMap m;
    for (const char *name : {"you", "others", "room"}) {
        const std::vector<float> x = m_load(QString::fromLatin1(name));
        if (x.size() < 4096) continue;
        QVariantList l;
        for (float v : logBandsDb(averageSpectrum(x, 4096), AudioCore::kRate, float(lowHz()), float(highHz()), kBands)) l << double(v);
        m[QString::fromLatin1(name)] = l;
    }
    m_recorded = m;
    emit recordedChanged();
}
