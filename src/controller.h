#pragma once

#include "audio/audiobackend.h"
#include "audio/audiocore.h"
#include "platform/autostart.h"
#include "separation.h"
#include "pluginmanager.h"
#include "diagnostics.h"
#include "mictest.h"
#include "levels.h"
#include "calibprofiles.h"
#include "optional/rnnoise_api.h"

#include <QFutureWatcher>
#include <QSettings>
#include <QObject>
#include <QQmlPropertyMap>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <map>

// Facts decided before the controller exists (see main.cpp / startup.cpp).
struct StartupInfo {
    bool safeMode = false;
    QString safeModeReason;     // shown to the user
    bool settingsDamaged = false;
    QString damagedSettingsCopy;
    QString logPath;
    mf::BackendConfig backend; // node names (the test instance uses its own)
};

class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQmlPropertyMap *params READ params CONSTANT)
    Q_PROPERTY(PluginManager *plugins READ plugins CONSTANT)
    Q_PROPERTY(Diagnostics *diagnostics READ diagnostics CONSTANT)
    Q_PROPERTY(MicTestSession *micTest READ micTest CONSTANT)
    // Per-microphone Setup: a message after switching mics, and the microphones that have a saved Setup.
    Q_PROPERTY(QString profileNotice READ profileNotice NOTIFY profileNoticeChanged)
    Q_PROPERTY(QVariantList micProfiles READ micProfiles NOTIFY profileNoticeChanged)

    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QString currentSource READ currentSource WRITE setCurrentSource NOTIFY currentSourceChanged)

    Q_PROPERTY(qreal inPeak MEMBER m_inPeak NOTIFY metersChanged)
    Q_PROPERTY(qreal noiseVad MEMBER m_noiseVad NOTIFY metersChanged)
    Q_PROPERTY(qreal gateLevel MEMBER m_gateLevel NOTIFY metersChanged)
    Q_PROPERTY(qreal gateReduction MEMBER m_gateReduction NOTIFY metersChanged)
    Q_PROPERTY(bool gateOpen MEMBER m_gateOpen NOTIFY metersChanged)
    Q_PROPERTY(qreal gateTilt MEMBER m_gateTilt NOTIFY metersChanged)
    Q_PROPERTY(qreal compLevel MEMBER m_compLevel NOTIFY metersChanged)
    Q_PROPERTY(qreal compReduction MEMBER m_compReduction NOTIFY metersChanged)
    Q_PROPERTY(qreal deessReduction MEMBER m_deessReduction NOTIFY metersChanged)
    Q_PROPERTY(qreal autoLevelGain MEMBER m_autoLevelGain NOTIFY metersChanged)
    Q_PROPERTY(qreal outPeak MEMBER m_outPeak NOTIFY metersChanged)
    Q_PROPERTY(qreal limReduction MEMBER m_limReduction NOTIFY metersChanged)

    Q_PROPERTY(bool engineOk READ engineOk NOTIFY statusChanged)
    Q_PROPERTY(QString engineState READ engineState NOTIFY statusChanged)  // running / reconnecting / stopped
    Q_PROPERTY(QString engineError READ engineError NOTIFY statusChanged)
    // One overall state for the window, tray and diagnostics:
    // disconnected / connecting / no-input / muted / bypassed / processing
    Q_PROPERTY(QString appState READ appState NOTIFY statusChanged)
    Q_PROPERTY(QString appStateText READ appStateText NOTIFY statusChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY statusChanged)
    Q_PROPERTY(QString backendName READ backendName CONSTANT)
    Q_PROPERTY(QString virtualMicName READ virtualMicName CONSTANT)
    Q_PROPERTY(bool safeMode READ safeMode CONSTANT)
    Q_PROPERTY(QString safeModeReason READ safeModeReason CONSTANT)
    Q_PROPERTY(QString damagedSettingsCopy READ damagedSettingsCopy CONSTANT)
    Q_PROPERTY(QString logPath READ logPath CONSTANT)
    Q_PROPERTY(QStringList unavailableFeatures READ unavailableFeatures CONSTANT)
    Q_PROPERTY(bool noiseRemovalAvailable READ noiseRemovalAvailable CONSTANT)
    Q_PROPERTY(bool captureRunning READ captureRunning NOTIFY statusChanged)
    Q_PROPERTY(bool virtualMicInUse READ virtualMicInUse NOTIFY statusChanged)

    Q_PROPERTY(bool monitor READ monitor WRITE setMonitor NOTIFY monitorChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(bool autostartHidden READ autostartHidden WRITE setAutostartHidden NOTIFY autostartChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool showWelcome READ showWelcome WRITE setShowWelcome NOTIFY showWelcomeChanged)
    Q_PROPERTY(QString themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged) // "system" | "dark" | "light"

    Q_PROPERTY(QStringList presetNames READ presetNames NOTIFY presetsChanged)
    Q_PROPERTY(QString currentPreset READ currentPreset NOTIFY presetsChanged)

    // Voice-separation setup.
    Q_PROPERTY(QString calibPhase READ calibPhase NOTIFY calibChanged)
    Q_PROPERTY(qreal calibProgress READ calibProgress NOTIFY calibChanged)
    Q_PROPERTY(QVariantMap calibResult READ calibResult NOTIFY calibChanged)
    // Saved measurement zones for the gate meter: {valid, room, others, you, youTilt, othersTilt} in current dB.
    Q_PROPERTY(QVariantMap zones READ zones NOTIFY zonesChanged)
    Q_PROPERTY(bool hasRecordings READ hasRecordings NOTIFY recordingsChanged)

public:
    explicit Controller(const StartupInfo &startup, QObject *parent = nullptr);
    ~Controller() override;

    QQmlPropertyMap *params() { return &m_params; }
    PluginManager *plugins() { return m_plugins; }
    Diagnostics *diagnostics() { return m_diagnostics; }
    MicTestSession *micTest() { return m_micTest; }
    QString profileNotice() const { return m_profileNotice; }
    QVariantList micProfiles() const;
    Q_INVOKABLE void dismissProfileNotice();
    QVariantList sources() const { return m_sources; }
    QString currentSource() const { return m_currentSource; }
    void setCurrentSource(const QString &name);

    bool engineOk() const { return m_backend->connected(); }
    QString backendName() const { return QString::fromStdString(m_backend->name()); }
    QString virtualMicName() const { return QString::fromStdString(m_startup.backend.virtualMicDescription); }
    QString engineState() const;
    QString appState() const;
    QString appStateText() const;
    bool muted() const { return m_params.value("mute").toDouble() > 0.5; }
    void setMuted(bool on) { setParam("mute", on ? 1 : 0); }
    Q_INVOKABLE void toggleMute() { setMuted(!muted()); }
    QString engineError() const;
    bool safeMode() const { return m_startup.safeMode; }
    QString safeModeReason() const { return m_startup.safeModeReason; }
    QString damagedSettingsCopy() const { return m_startup.damagedSettingsCopy; }
    QString logPath() const { return m_startup.logPath; }
    QStringList unavailableFeatures() const { return m_unavailable; }
    bool noiseRemovalAvailable() const { return mf::rnnoise::available(); }
    Q_INVOKABLE void restartEngine();
    Q_INVOKABLE void openLogFolder();
    Q_INVOKABLE void requestRestart(bool safeMode); // relaunch MicForge (normally or in safe mode)
    bool captureRunning() const { return m_backend->inputRunning(); }
    bool virtualMicInUse() const { return m_backend->virtualMicInUse(); }

    bool monitor() const { return m_monitor; }
    void setMonitor(bool on);
    bool autostart() const;
    void setAutostart(bool on);
    bool autostartHidden() const;
    void setAutostartHidden(bool hidden);
    QString version() const { return QStringLiteral(MICFORGE_VERSION); }
    bool showWelcome() const { return m_showWelcome; }
    void setShowWelcome(bool on);
    QString themeMode() const { return m_themeMode; }
    void setThemeMode(const QString &mode);

    QStringList presetNames() const;
    QString currentPreset() const { return m_currentPreset; }

    QString calibPhase() const { return m_calibPhase; }
    qreal calibProgress() const { return m_calibProgress; }
    QVariantMap calibResult() const { return m_calibResult; }
    QVariantMap zones() const;
    bool hasRecordings() const;

    Q_INVOKABLE double defaultValue(const QString &key) const;
    Q_INVOKABLE void setParam(const QString &key, double value);
    Q_INVOKABLE void resetParam(const QString &key);
    Q_INVOKABLE void addEffect(const QString &name);    // "noise", "deess", "eq"
    Q_INVOKABLE void removeEffect(const QString &name);
    Q_INVOKABLE void loadPreset(const QString &name);
    Q_INVOKABLE void savePreset(const QString &name);
    Q_INVOKABLE void deletePreset(const QString &name);
    Q_INVOKABLE bool isBuiltinPreset(const QString &name) const;
    Q_INVOKABLE void startCalibration();
    Q_INVOKABLE void skipOthers();
    Q_INVOKABLE void startLearnOthers();
    Q_INVOKABLE void applyNeeds();
    Q_INVOKABLE void continueCalibration();
    Q_INVOKABLE void redoStep(const QString &step);
    Q_INVOKABLE void restartStep();
    Q_INVOKABLE void retuneFromRecordings();
    Q_INVOKABLE void cancelCalibration();
    Q_INVOKABLE void saveNow();

signals:
    void sourcesChanged();
    void currentSourceChanged();
    void metersChanged();
    void statusChanged();
    void monitorChanged();
    void autostartChanged();
    void showWelcomeChanged();
    void themeModeChanged();
    void profileNoticeChanged();
    void presetsChanged();
    void calibChanged();
    void zonesChanged();
    void restartRequested(bool safeMode);
    void recordingsChanged();

private:
    void pollMeters();
    void startEngine();
    void scheduleEngineRetry(bool immediate);
    void onParamEdited(const QString &key, const QVariant &value);
    void applyToEngine(int id, double value);
    void updateSources(const std::vector<mf::InputDevice> &list);
    void adoptDefaultInputIfUnset();
    bool m_defaultWaitOver = false;
    void calibTick();
    void enterPhase(const QString &phase);
    void startAnalysis();
    void applyAnalysis(const mf::SeparationResult &r);
    QString presetDir() const;
    int idForKey(const QString &key) const;

    mf::AudioCore m_core;                       // platform-independent processing
    std::unique_ptr<mf::IAudioBackend> m_backend; // OS audio system (PipeWire on Linux)
    QQmlPropertyMap m_params;
    PluginManager *m_plugins = nullptr;
    Diagnostics *m_diagnostics = nullptr;
    MicTestSession *m_micTest = nullptr;
    bool m_testPlayback = false;          // mic-test playback needs the monitor output
    void updateMonitorStream();
    QSettings m_settingsStore;
    CalibrationProfiles m_profiles;
    QString m_profileNotice;
    QString sourceDescription(const QString &name) const;
    QVariantMap profileParamValues() const;
    void saveProfileFor(const QString &device);
    void switchProfile(const QString &from, const QString &to);
    QTimer m_meterTimer, m_saveTimer;
    QVariantList m_sources;
    QString m_currentSource;
    QString m_currentPreset;
    StartupInfo m_startup;
    std::unique_ptr<mf::IAutostart> m_autostart;
    QStringList m_unavailable;
    QTimer m_engineRetry;
    int m_engineAttempts = 0;
    bool m_monitor = false;
    bool m_showWelcome = true;
    QString m_themeMode = QStringLiteral("system");

    qreal m_inPeak = -120, m_noiseVad = 0, m_gateLevel = -120, m_gateReduction = 0, m_compLevel = -120;
    qreal m_compReduction = 0, m_deessReduction = 0, m_outPeak = -120, m_limReduction = 0;
    bool m_gateOpen = false;
    qreal m_gateTilt = -120;
    qreal m_autoLevelGain = 0;

    QString m_calibPhase;
    qreal m_calibProgress = 0;
    int m_calibTicks = 0;
    QVariantMap m_calibResult;
    std::map<QString, std::vector<float>> m_takes; // finished recordings of this run
    std::map<QString, uint64_t> m_takeClips;       // input clipping (before gain) during each take of this run
    uint64_t m_clipsFrom = 0;
    mf::LevelStats m_youLevels, m_roomLevels;      // level analysis of the takes being analysed
    size_t m_takeFrom = 0;
    QString m_redo;                                // step being redone on its own ("" = full run)
    bool m_learning = false;
    bool m_retune = false;
    QFutureWatcher<mf::SeparationResult> m_analysis;
};
