#pragma once

#include "audio/audiobackend.h"
#include "audio/audiocore.h"
#include "platform/autostart.h"
#include "platform/globalshortcuts.h"
#include "separation.h"
#include "pluginmanager.h"
#include "diagnostics.h"
#include "mictest.h"
#include "levels.h"
#include "calibprofiles.h"
#include "detectorspectrum.h"
#include "undostack.h"
#include "optional/rnnoise_api.h"

#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QSettings>
#include <QUrl>
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
    Q_PROPERTY(DetectorSpectrum *spectrum READ spectrum CONSTANT)
    // Per-microphone Setup: a message after switching mics, and the microphones that have a saved Setup.
    Q_PROPERTY(QString profileNotice READ profileNotice NOTIFY profileNoticeChanged)
    Q_PROPERTY(QString presetNotice READ presetNotice NOTIFY presetsChanged)
    // True when Setup was measured before the Voice Gate started listening ahead of Noise Removal (0.2.1).
    Q_PROPERTY(bool setupOutdated READ setupOutdated NOTIFY zonesChanged)
    // Undo/redo of sound settings ("" when there's nothing to undo/redo).
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoChanged)
    // System-wide shortcuts (mute, push-to-talk) through the desktop.
    Q_PROPERTY(bool shortcutsAvailable READ shortcutsAvailable NOTIFY shortcutsChanged)
    Q_PROPERTY(bool shortcutsEnabled READ shortcutsEnabled WRITE setShortcutsEnabled NOTIFY shortcutsChanged)
    Q_PROPERTY(bool pushToTalk READ pushToTalk WRITE setPushToTalk NOTIFY shortcutsChanged)
    Q_PROPERTY(QString shortcutsStatus READ shortcutsStatus NOTIFY shortcutsChanged)
    Q_PROPERTY(QString muteKey READ muteKey NOTIFY shortcutsChanged)
    Q_PROPERTY(QString pushToTalkKey READ pushToTalkKey NOTIFY shortcutsChanged)
    // One shortcut per Voice Changer preset (plus "back to your own voice"), opt-in.
    Q_PROPERTY(bool voiceShortcutsEnabled READ voiceShortcutsEnabled WRITE setVoiceShortcutsEnabled NOTIFY shortcutsChanged)
    Q_PROPERTY(QVariantMap voiceKeys READ voiceKeys NOTIFY shortcutsChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoChanged)
    Q_PROPERTY(QVariantList micProfiles READ micProfiles NOTIFY profileNoticeChanged)

    Q_PROPERTY(QVariantList sources READ sources NOTIFY sourcesChanged)
    Q_PROPERTY(QString currentSource READ currentSource WRITE setCurrentSource NOTIFY currentSourceChanged)

    Q_PROPERTY(qreal inPeak MEMBER m_inPeak NOTIFY metersChanged)
    Q_PROPERTY(qreal noiseVad MEMBER m_noiseVad NOTIFY metersChanged)
    Q_PROPERTY(qreal gateLevel MEMBER m_gateLevel NOTIFY metersChanged)
    Q_PROPERTY(qreal gateReduction MEMBER m_gateReduction NOTIFY metersChanged)
    Q_PROPERTY(bool gateOpen MEMBER m_gateOpen NOTIFY metersChanged)
    Q_PROPERTY(qreal gateTilt MEMBER m_gateTilt NOTIFY metersChanged)
    // Why the gate is open/closed: "" (gate off), "quiet", "noise", "far", "open". Held ~0.3 s so it's readable.
    Q_PROPERTY(QString gateReason MEMBER m_gateReason NOTIFY metersChanged)
    Q_PROPERTY(qreal gateFloor MEMBER m_gateFloor NOTIFY metersChanged)     // steady background, dB (-120 = none)
    Q_PROPERTY(qreal gateOpenAt MEMBER m_gateOpenAt NOTIFY metersChanged)   // threshold in use, dB
    Q_PROPERTY(qreal humBase MEMBER m_humBase NOTIFY metersChanged)         // 50 / 60 Hz, 0 = not found yet
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
    DetectorSpectrum *spectrum() { return m_spectrum; }
    QString profileNotice() const { return m_profileNotice; }
    QString presetNotice() const { return m_presetNotice; }
    bool setupOutdated() const;
    QString undoText() const { return m_undo.undoLabel(); }
    QString redoText() const { return m_undo.redoLabel(); }
    bool shortcutsAvailable() const { return m_shortcuts && m_shortcuts->available(); }
    bool shortcutsEnabled() const { return m_shortcutsEnabled; }
    void setShortcutsEnabled(bool on);
    bool pushToTalk() const { return m_pushToTalk; }
    void setPushToTalk(bool on);
    QString shortcutsStatus() const;
    QString muteKey() const { return m_shortcuts ? m_shortcuts->triggers().value("mute") : QString(); }
    QString pushToTalkKey() const { return m_shortcuts ? m_shortcuts->triggers().value("push-to-talk") : QString(); }
    Q_INVOKABLE void configureShortcuts() { if (m_shortcuts) m_shortcuts->configure(); }
    bool voiceShortcutsEnabled() const { return m_voiceShortcutsEnabled; }
    void setVoiceShortcutsEnabled(bool on);
    QVariantMap voiceKeys() const; // preset id ("off" = your own voice) -> key description
    // The UI lists its Voice Changer presets once at start: [{id, name}, ...].
    Q_INVOKABLE void setVoiceList(const QVariantList &voices);
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void dismissPresetNotice() { m_presetNotice.clear(); emit presetsChanged(); }
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
    // Preset files: export one to a file, import a file (returns a plain-English result).
    Q_INVOKABLE QString exportPreset(const QString &name, const QUrl &file);
    Q_INVOKABLE QString importPreset(const QUrl &file);
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
    void undoChanged();
    void shortcutsChanged();
    void voiceShortcut(const QString &id); // a voice's system-wide shortcut was pressed ("off" = your own voice)
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
    DetectorSpectrum *m_spectrum = nullptr;
    bool m_testPlayback = false;          // mic-test playback needs the monitor output
    void updateMonitorStream();
    QSettings m_settingsStore;
    CalibrationProfiles m_profiles;
    QString m_profileNotice;
    QString m_presetNotice;
    std::unique_ptr<mf::IGlobalShortcuts> m_shortcuts;
    bool m_shortcutsEnabled = false, m_pushToTalk = false, m_voiceShortcutsEnabled = false;
    QVariantList m_voiceList;
    bool m_bindScheduled = false;
    void scheduleBind();
    void bindShortcuts();
    UndoStack m_undo;
    std::array<double, mf::PARAM_COUNT> m_shadow{}; // last value applied, per parameter (for undo)
    bool m_recordUndo = false, m_applyingUndo = false;
    QElapsedTimer m_clock;
    void applyUndoStep(const UndoStack::Step &step, bool undo);
    struct UndoGroup { // records everything changed in a scope as one undo step
        Controller *c;
        UndoGroup(Controller *ctl, const QString &label) : c(ctl) { c->m_undo.beginGroup(label); }
        ~UndoGroup() { c->m_undo.endGroup(c->m_clock.elapsed()); emit c->undoChanged(); }
    };
    bool applyPresetFile(const QString &path, const QString &name);
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
    QString m_gateReason;
    qreal m_gateFloor = -120, m_gateOpenAt = -120, m_humBase = 0;
    int m_reasonHold = 0, m_reasonCode = 0;
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
