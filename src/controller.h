#pragma once

#include "audioengine.h"
#include "separation.h"
#include "pluginmanager.h"

#include <QFutureWatcher>
#include <QObject>
#include <QQmlPropertyMap>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <map>

class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(QQmlPropertyMap *params READ params CONSTANT)
    Q_PROPERTY(PluginManager *plugins READ plugins CONSTANT)

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
    Q_PROPERTY(bool captureRunning READ captureRunning NOTIFY statusChanged)
    Q_PROPERTY(bool virtualMicInUse READ virtualMicInUse NOTIFY statusChanged)

    Q_PROPERTY(bool monitor READ monitor WRITE setMonitor NOTIFY monitorChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(bool autostartHidden READ autostartHidden WRITE setAutostartHidden NOTIFY autostartChanged)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool showWelcome READ showWelcome WRITE setShowWelcome NOTIFY showWelcomeChanged)

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
    explicit Controller(QObject *parent = nullptr);
    ~Controller() override;

    QQmlPropertyMap *params() { return &m_params; }
    PluginManager *plugins() { return m_plugins; }
    QVariantList sources() const { return m_sources; }
    QString currentSource() const { return m_currentSource; }
    void setCurrentSource(const QString &name);

    bool engineOk() const { return m_engineOk; }
    bool captureRunning() const { return m_engine.captureRunning; }
    bool virtualMicInUse() const { return m_engine.virtualMicInUse; }

    bool monitor() const { return m_monitor; }
    void setMonitor(bool on);
    bool autostart() const;
    void setAutostart(bool on);
    bool autostartHidden() const;
    void setAutostartHidden(bool hidden);
    QString version() const { return QStringLiteral(MICFORGE_VERSION); }
    bool showWelcome() const { return m_showWelcome; }
    void setShowWelcome(bool on);

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
    void presetsChanged();
    void calibChanged();
    void zonesChanged();
    void recordingsChanged();

private:
    void pollMeters();
    void writeAutostartEntry();
    void onParamEdited(const QString &key, const QVariant &value);
    void applyToEngine(int id, double value);
    void updateSources(const std::vector<mf::SourceInfo> &list);
    void calibTick();
    void enterPhase(const QString &phase);
    void startAnalysis();
    void applyAnalysis(const mf::SeparationResult &r);
    QString presetDir() const;
    int idForKey(const QString &key) const;

    mf::AudioEngine m_engine;
    QQmlPropertyMap m_params;
    PluginManager *m_plugins = nullptr;
    QTimer m_meterTimer, m_saveTimer;
    QVariantList m_sources;
    QString m_currentSource;
    QString m_currentPreset;
    bool m_engineOk = false;
    bool m_monitor = false;
    bool m_showWelcome = true;

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
    size_t m_takeFrom = 0;
    QString m_redo;                                // step being redone on its own ("" = full run)
    bool m_learning = false;
    bool m_retune = false;
    QFutureWatcher<mf::SeparationResult> m_analysis;
};
