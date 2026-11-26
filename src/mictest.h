// Mic test workspace: record the raw and the processed microphone at the same time, play either back
// to the monitor output only (never to the virtual microphone), compare levels, export WAV files.
// Recordings stay in memory and on this computer; nothing is saved unless the user exports it.
#pragma once

#include "audio/audiocore.h"
#include "levels.h"

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include <functional>

class MicTestSession : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)          // idle / recording / ready / playing
    Q_PROPERTY(int playing READ playing NOTIFY stateChanged)          // 0 none, 1 raw, 2 processed
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)   // recording or playback, 0..1
    Q_PROPERTY(int seconds READ seconds WRITE setSeconds NOTIFY secondsChanged)
    Q_PROPERTY(bool matchLevels READ matchLevels WRITE setMatchLevels NOTIFY secondsChanged)
    Q_PROPERTY(qreal takeSeconds READ takeSeconds NOTIFY takeChanged)
    Q_PROPERTY(QVariantList rawWave READ rawWave NOTIFY takeChanged)
    Q_PROPERTY(QVariantList processedWave READ processedWave NOTIFY takeChanged)
    Q_PROPERTY(QVariantMap analysis READ analysis NOTIFY takeChanged)

public:
    explicit MicTestSession(mf::AudioCore &core, QObject *parent = nullptr);
    std::function<void(bool)> onNeedMonitorStream; // keep the monitor output open while playing

    QString state() const { return m_state; }
    int playing() const { return m_playing; }
    qreal progress() const { return m_progress; }
    int seconds() const { return m_seconds; }
    void setSeconds(int s);
    bool matchLevels() const { return m_match; }
    void setMatchLevels(bool on);
    qreal takeSeconds() const { return m_frames / double(mf::AudioCore::kRate); }
    QVariantList rawWave() const { return m_rawWave; }
    QVariantList processedWave() const { return m_procWave; }
    QVariantMap analysis() const { return m_analysis; }

    Q_INVOKABLE void record();
    Q_INVOKABLE void stop();                 // stop recording or playback
    Q_INVOKABLE void play(int which);        // 1 = raw, 2 = processed
    Q_INVOKABLE void discard();
    // Writes micforge-test-<time>-raw.wav and -processed.wav into `folder`. Returns a message.
    Q_INVOKABLE QString exportWav(const QUrl &folder);

signals:
    void stateChanged();
    void progressChanged();
    void secondsChanged();
    void takeChanged();

private:
    void tick();
    void finishRecording();
    void setState(const QString &s, int playing = 0);

    mf::AudioCore &m_core;
    QTimer m_timer;
    QString m_state = QStringLiteral("idle");
    int m_playing = 0;
    qreal m_progress = 0;
    int m_seconds = 10;
    bool m_match = true;
    size_t m_frames = 0;
    uint64_t m_clipsAtStart = 0, m_clipsDuring = 0;
    mf::LevelStats m_raw, m_proc;
    QVariantList m_rawWave, m_procWave;
    QVariantMap m_analysis;
};
