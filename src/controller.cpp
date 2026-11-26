#include "controller.h"
#include "presetfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <cstring>

#include <algorithm>
#include <QDesktopServices>
#include <QUrl>
#include <cmath>

using namespace mf;

namespace {

struct BuiltinPreset {
    const char *name;
    QVariantMap values; // overrides on top of defaults
};

const QList<BuiltinPreset> &builtinPresets() {
    static const QList<BuiltinPreset> list = {
        {"Voice chat", {}},
        {"Gentle / natural",
         {{"gateMode", 1}, {"gateRatio", 2.5}, {"gateRange", -25}, {"gateRelease", 400}, {"compRatio", 2},
          {"compThreshold", -20}, {"compMakeup", 3}}},
        {"Shared room (strict)",
         {{"gateRange", -60}, {"gateHold", 120}, {"gateRelease", 180}, {"gateLookahead", 5}, {"compRatio", 4}}},
        {"Broadcast / radio",
         {{"lowCutFreq", 90}, {"compRatio", 5}, {"compThreshold", -28}, {"compMakeup", 9}, {"compAttack", 5},
          {"toneBass", 2}, {"tonePresence", 2.5}, {"toneAir", 1.5}}},
    };
    return list;
}

// Settings that belong to this mic/room, not to a sound style. Built-in presets never touch them,
// so loading a preset doesn't undo the voice-separation setup or remove added effects.
std::vector<float> loadSamples(const QString &name, float gain); // Setup recordings (defined below)

QString calibDirPath() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/calib"; }

bool isPersonalKey(const QString &k) {
    static const QStringList keys = {"mute", "inputChannel", "inputGain", "gateThreshold", "gateHysteresis",
                                     "gateFocus", "gateScLow", "gateScHigh", "gateCloseCheck", "gateTilt", "gateTiltDir",
                                     "micType", "needOthers", "needNoise", "needBroadcast", "bypass"};
    return keys.contains(k) || k.endsWith("Added");
}

double roundHalf(double v) { return std::round(v * 2.0) / 2.0; }


constexpr int kCalibVersion = 2; // bump when the analysis changes in a way that should re-tune saved setups

QString fmtHz(double v) { return v >= 1000 ? QString::number(v / 1000, 'f', 1) + " kHz" : QString::number(v, 'f', 0) + " Hz"; }

} // namespace

Controller::Controller(const StartupInfo &startup, QObject *parent)
    : QObject(parent), m_profiles(calibDirPath(), m_settingsStore), m_startup(startup), m_autostart(createAutostart()) {
    QSettings s;
    if (m_startup.safeMode)
        qWarning("safe mode: using default sound settings and no plugins; nothing will be saved (%s)",
                 qPrintable(m_startup.safeModeReason));
    for (int i = 0; i < PARAM_COUNT; ++i) {
        // Invalid or out-of-range stored values fall back to defaults instead of reaching the DSP.
        bool ok = false;
        double v = m_startup.safeMode ? kParams[i].def
                                      : s.value(QString("params/") + kParams[i].key, kParams[i].def).toDouble(&ok);
        if (!m_startup.safeMode && (!ok || !std::isfinite(v))) v = kParams[i].def;
        v = std::clamp(v, double(kParams[i].min), double(kParams[i].max));
        m_params.insert(kParams[i].key, v);
        m_core.params.set(i, float(v));
    }
    m_core.params.set(Bypass, 0); // never start bypassed or muted
    m_core.params.set(Mute, 0);
    m_params.insert("mute", 0);
    m_params.insert("bypass", 0);
    m_currentSource = s.value("source").toString();
    m_currentPreset = s.value("preset", builtinPresets().first().name).toString();
    if (m_currentPreset == QStringLiteral("SM57 · Voice chat")) m_currentPreset = builtinPresets().first().name;
    m_showWelcome = s.value("showWelcome", true).toBool();
    m_themeMode = s.value("ui/theme", "system").toString();
    if (m_themeMode != "dark" && m_themeMode != "light") m_themeMode = "system";

    connect(&m_params, &QQmlPropertyMap::valueChanged, this, &Controller::onParamEdited);

    m_backend = createAudioBackend(m_core, m_startup.backend);
    m_backend->onDevicesChanged = [this](std::vector<InputDevice> list) {
        QMetaObject::invokeMethod(this, [this, list] { updateSources(list); }, Qt::QueuedConnection);
    };
    m_backend->onStateChanged = [this] {
        QMetaObject::invokeMethod(this, [this] { emit statusChanged(); }, Qt::QueuedConnection);
    };
    m_backend->onConnectionLost = [this] {
        QMetaObject::invokeMethod(this, [this] {
            qWarning("audio engine: %s; reconnecting", m_backend->error().c_str());
            scheduleEngineRetry(true);
        }, Qt::QueuedConnection);
    };
    m_engineRetry.setSingleShot(true);
    connect(&m_engineRetry, &QTimer::timeout, this, &Controller::startEngine);

    if (!rnnoise::available()) {
        m_unavailable << tr("AI Noise Removal is unavailable because %1. Install the “rnnoise” package to enable it.")
                             .arg(QString::fromStdString(rnnoise::error()));
        qWarning("optional feature: AI noise removal unavailable: %s", rnnoise::error().c_str());
    }
    QString lv2Why;
    if (!lv2Available(&lv2Why)) {
        m_unavailable << tr("LV2 plugins are unavailable because %1. Install the “lilv” package to enable them.").arg(lv2Why);
        qWarning("optional feature: LV2 hosting unavailable: %s", qPrintable(lv2Why));
    }

    // First run (no microphone chosen yet): adopt the system default as soon as the backend reports it.
    m_backend->onDefaultInputChanged = [this] {
        QMetaObject::invokeMethod(this, [this] { adoptDefaultInputIfUnset(); }, Qt::QueuedConnection);
    };
    m_backend->setInputDevice(m_currentSource.toStdString());
    startEngine();
    QTimer::singleShot(3000, this, [this] {
        m_defaultWaitOver = true;
        adoptDefaultInputIfUnset();
    });

    // Third-party plugins: restore the saved chain now (not in safe mode), discover what's installed in the background.
    m_plugins = new PluginManager(&m_core.rack, AudioCore::kRate, this);
    m_diagnostics = new Diagnostics(this, m_core, *m_backend, m_startup.logPath, this);
    m_micTest = new MicTestSession(m_core, this);
    m_spectrum = new DetectorSpectrum(m_core, [](const QString &n) { return loadSamples(n, 1.0f); }, this);
    connect(this, &Controller::recordingsChanged, m_spectrum, &DetectorSpectrum::recordingsChanged);
    m_micTest->onNeedMonitorStream = [this](bool on) { m_testPlayback = on; updateMonitorStream(); };
    m_plugins->setReadOnly(m_startup.safeMode);
    if (!m_startup.safeMode) m_plugins->restore();
    QTimer::singleShot(1200, m_plugins, [this] { m_plugins->rescan(); });

    if (!m_startup.safeMode && !qEnvironmentVariableIsSet("MICFORGE_TEST")) m_autostart->refresh(autostartHidden());

    m_meterTimer.setInterval(33);
    connect(&m_meterTimer, &QTimer::timeout, this, &Controller::pollMeters);
    m_meterTimer.start();

    // System-wide shortcuts. Probing the desktop is silent; binding (which may ask the user) only happens
    // once they've switched shortcuts on.
    m_shortcutsEnabled = s.value("shortcuts/enabled", false).toBool();
    m_pushToTalk = s.value("shortcuts/pushToTalk", false).toBool();
    m_shortcuts = createGlobalShortcuts();
    m_shortcuts->onChanged = [this] { emit shortcutsChanged(); };
    m_shortcuts->onActivated = [this](const QString &id) {
        if (id == "mute") toggleMute();
        else if (id == "push-to-talk" && m_pushToTalk) setMuted(false);
    };
    m_shortcuts->onDeactivated = [this](const QString &id) {
        if (id == "push-to-talk" && m_pushToTalk) setMuted(true);
    };
    if (m_shortcutsEnabled && !m_startup.safeMode) bindShortcuts();
    if (m_pushToTalk && m_shortcutsEnabled) setMuted(true); // push-to-talk: silent until the key is held

    // From here on, every settings change can be undone.
    for (int i = 0; i < PARAM_COUNT; ++i) m_shadow[size_t(i)] = m_params.value(kParams[i].key).toDouble();
    m_clock.start();
    m_recordUndo = true;

    // The voice-separation analysis improved since these recordings were tuned: re-tune once.
    if (!m_startup.safeMode && hasRecordings() && QSettings().value("calib/version", 1).toInt() < kCalibVersion)
        QTimer::singleShot(1500, this, &Controller::retuneFromRecordings);

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(600);
    connect(&m_saveTimer, &QTimer::timeout, this, &Controller::saveNow);

    connect(&m_analysis, &QFutureWatcher<SeparationResult>::finished, this,
            [this] { applyAnalysis(m_analysis.result()); });

    // Development aid: show an example result page (for screenshots of the UI without recording).
    if (qEnvironmentVariableIsSet("MICFORGE_DEMO_RESULT")) {
        auto step = [](const char *id, const char *label, const char *status, const char *note) {
            return QVariantMap{{"id", id}, {"label", label}, {"status", status}, {"note", note}};
        };
        m_calibResult = QVariantMap{
            {"ok", true}, {"learning", false}, {"redo", ""}, {"hadOthers", true}, {"quality", 1},
            {"separation", 6.0}, {"broadbandSeparation", 2.0}, {"room", -70.0}, {"others", -52.0}, {"you", -40.0},
            {"threshold", -46.0}, {"closeAt", -49.0}, {"band", "90 Hz – 200 Hz"}, {"gain", 20.0},
            {"compThreshold", -22.0}, {"makeup", 8.0}, {"closeCheck", true}, {"tiltMeasured", false},
            {"tiltGap", 0.5}, {"headroom", 6.0}, {"tips", QStringList{}},
            {"steps", QVariantList{
                step("room", "Quiet room", "ok", "Nice and quiet."),
                step("others", "Other voices", "warn", "These voices sounded as close and loud as you. Was it you talking? Redo it while staying silent."),
                step("you", "Your voice", "ok", "Clear and strong.")}}};
        m_calibPhase = "done";
    }
}

Controller::~Controller() {
    m_analysis.waitForFinished();
    saveNow();
    // Explicit shutdown order: stop the audio thread, then destroy plugins (they live in m_core.rack,
    // which is destroyed before Qt would delete child objects), then everything else.
    delete m_spectrum;
    m_spectrum = nullptr;
    delete m_micTest;
    m_micTest = nullptr;
    delete m_diagnostics;
    m_diagnostics = nullptr;
    m_backend.reset();
    delete m_plugins;
    m_plugins = nullptr;
}

// ---------------------------------------------------------------- params

int Controller::idForKey(const QString &key) const {
    for (int i = 0; i < PARAM_COUNT; ++i)
        if (key == QLatin1String(kParams[i].key)) return i;
    return -1;
}

double Controller::defaultValue(const QString &key) const {
    const int id = idForKey(key);
    return id < 0 ? 0 : kParams[id].def;
}

namespace {
// Plain-English name of the effect or control a parameter belongs to, for "Undo …".
QString undoLabelFor(const QString &key) {
    static const std::pair<const char *, const char *> prefixes[] = {
        {"input", "Input"}, {"noise", "Noise Removal"}, {"hum", "Hum Removal"}, {"lowCut", "Low Cut"},
        {"gate", "Voice Gate"}, {"breath", "Breath reduction"}, {"autoLevel", "Auto Level"}, {"comp", "Compressor"},
        {"deess", "De-esser"}, {"vc", "Voice Changer"}, {"eq", "Equaliser"}, {"tone", "Tone"},
        {"plugins", "Plugins"}, {"warmth", "Warmth"}, {"exciter", "Exciter"}, {"doubler", "Doubler"},
        {"echo", "Echo"}, {"reverb", "Reverb"}, {"output", "Output"}, {"limiter", "Limiter"}, {"need", "Setup answers"},
        {"mic", "Setup answers"}};
    for (const auto &[p, name] : prefixes)
        if (key.startsWith(QLatin1String(p))) return key.endsWith("Added") ? QString("Add/remove %1").arg(name) : QString(name);
    return QStringLiteral("setting");
}
} // namespace

void Controller::applyToEngine(int id, double value) {
    if (m_recordUndo && !m_applyingUndo && id != Bypass && id != Mute && value != m_shadow[size_t(id)]) {
        m_undo.record(kParams[id].key, m_shadow[size_t(id)], value, m_clock.elapsed(), undoLabelFor(kParams[id].key));
        emit undoChanged();
    }
    m_shadow[size_t(id)] = value;
    m_core.params.set(id, float(value));
    if (id != Bypass && id != Mute) m_saveTimer.start();
    if (id == InputGain || id == GateFocus || id == GateScLow || id == GateScHigh) emit zonesChanged();
}

void Controller::onParamEdited(const QString &key, const QVariant &value) {
    const int id = idForKey(key);
    if (id < 0) return;
    const double v = std::clamp(value.toDouble(), double(kParams[id].min), double(kParams[id].max));
    applyToEngine(id, v);
    if (id == Bypass || id == Mute) emit statusChanged();
    if (!isPersonalKey(key) && !m_currentPreset.isEmpty()) {
        m_currentPreset.clear();
        emit presetsChanged();
    }
}

void Controller::setParam(const QString &key, double value) {
    const int id = idForKey(key);
    if (id < 0) return;
    value = std::clamp(value, double(kParams[id].min), double(kParams[id].max));
    m_params.insert(key, value);
    onParamEdited(key, value);
}

void Controller::resetParam(const QString &key) { setParam(key, defaultValue(key)); }

void Controller::addEffect(const QString &name) {
    setParam(name + "On", 1);
    setParam(name + "Added", 1);
}

void Controller::removeEffect(const QString &name) { setParam(name + "Added", 0); }

void Controller::saveNow() {
    if (m_startup.safeMode) return; // safe mode never overwrites the user's configuration
    QSettings s;
    for (int i = 0; i < PARAM_COUNT; ++i)
        if (i != Bypass && i != Mute) s.setValue(QString("params/") + kParams[i].key, m_params.value(kParams[i].key));
    s.setValue("source", m_currentSource);
    s.setValue("preset", m_currentPreset);
    s.setValue("showWelcome", m_showWelcome);
    s.setValue("ui/theme", m_themeMode);
}

// ---------------------------------------------------------------- devices

void Controller::updateSources(const std::vector<InputDevice> &list) {
    QVariantList out;
    for (const auto &s : list) {
        QVariantMap m;
        m["name"] = QString::fromStdString(s.id);
        m["description"] = QString::fromStdString(s.name);
        out << m;
    }
    m_sources = out;
    emit sourcesChanged();
    adoptDefaultInputIfUnset();
}

// Only when the user hasn't chosen a microphone yet: use the system default (never MicForge's own or
// another processing app's virtual mic), else the first hardware microphone.
void Controller::adoptDefaultInputIfUnset() {
    if (!m_currentSource.isEmpty() || m_sources.isEmpty()) return;
    auto usable = [](const QString &id) { return !id.startsWith("micforge") && !id.contains("easyeffects"); };
    const QString def = QString::fromStdString(m_backend->defaultInputDevice());
    for (const auto &v : m_sources)
        if (v.toMap()["name"].toString() == def && usable(def)) {
            qInfo("devices: no microphone chosen yet; using the system default %s", qPrintable(def));
            return setCurrentSource(def);
        }
    // Devices and the default are announced one by one: keep waiting for the default to appear.
    // Only if it hasn't after a few seconds (adoptFallbackInput) take the first hardware microphone.
    if (!m_defaultWaitOver) return;
    for (const auto &v : m_sources) {
        const QString id = v.toMap()["name"].toString();
        if (id.startsWith("alsa_input.") && usable(id)) {
            qInfo("devices: system default (%s) isn't usable; using %s", qPrintable(def), qPrintable(id));
            return setCurrentSource(id);
        }
    }
}

void Controller::setCurrentSource(const QString &name) {
    if (name == m_currentSource) return;
    const QString previous = m_currentSource;
    m_currentSource = name;
    switchProfile(previous, name);
    m_backend->setInputDevice(name.toStdString());
    m_saveTimer.start();
    emit currentSourceChanged();
}

void Controller::setMonitor(bool on) {
    if (on == m_monitor) return;
    m_monitor = on;
    m_core.setLiveMonitor(on);
    updateMonitorStream();
    emit monitorChanged();
}

// ---------------------------------------------------------------- per-microphone Setup

QString Controller::sourceDescription(const QString &name) const {
    for (const QVariant &v : m_sources)
        if (v.toMap().value("name").toString() == name) return v.toMap().value("description").toString();
    return name;
}

QVariantMap Controller::profileParamValues() const {
    QVariantMap m;
    for (const QString &k : CalibrationProfiles::profileParams()) m[k] = m_params.value(k);
    return m;
}

void Controller::saveProfileFor(const QString &device) {
    if (m_startup.safeMode || device.isEmpty()) return;
    m_settingsStore.setValue("calib/device", device);
    if (m_profiles.save(device, sourceDescription(device), profileParamValues()))
        qInfo("setup: saved the Setup for this microphone (%s)", qPrintable(device));
    emit profileNoticeChanged();
}

// Called when the user picks another microphone. The outgoing mic's Setup is saved first (so nothing
// is lost), then the incoming mic's own Setup is restored if it has one.
void Controller::switchProfile(const QString &from, const QString &to) {
    if (m_startup.safeMode || from.isEmpty() || to.isEmpty() || !m_calibPhase.isEmpty()) return;
    m_settingsStore.sync();
    const bool valid = m_settingsStore.value("calib/valid").toBool();
    const QString owner = m_settingsStore.value("calib/device").toString();
    if (valid && (owner.isEmpty() || owner == from)) saveProfileFor(from); // older setups have no owner yet: it's the mic in use
    if (m_profiles.has(to)) {
        UndoGroup group(this, tr("Switch microphone"));
        const QString preset = m_currentPreset;
        const QVariantMap params = m_profiles.restore(to);
        for (auto it = params.begin(); it != params.end(); ++it) setParam(it.key(), it.value().toDouble());
        if (m_currentPreset != preset) { m_currentPreset = preset; emit presetsChanged(); } // a mic switch isn't an edit
        m_profileNotice = tr("Loaded the Setup saved for “%1”.").arg(sourceDescription(to));
        qInfo("setup: restored the Setup for %s", qPrintable(to));
        emit zonesChanged();
        emit recordingsChanged();
    } else if (valid) {
        m_profileNotice = tr("“%1” hasn't been set up yet. The current Setup (made with another microphone) stays in "
                             "use until you run Setup for this one.").arg(sourceDescription(to));
    } else {
        m_profileNotice.clear();
    }
    emit profileNoticeChanged();
}

QVariantList Controller::micProfiles() const {
    QVariantList out = m_profiles.list();
    const QString owner = QSettings().value("calib/device").toString();
    for (QVariant &v : out) {
        QVariantMap m = v.toMap();
        m["active"] = m.value("device").toString() == owner;
        m["connected"] = std::any_of(m_sources.begin(), m_sources.end(), [&](const QVariant &s) {
            return s.toMap().value("name") == m.value("device");
        });
        v = m;
    }
    return out;
}

void Controller::dismissProfileNotice() {
    m_profileNotice.clear();
    emit profileNoticeChanged();
}

// ---------------------------------------------------------------- global shortcuts

void Controller::bindShortcuts() {
    m_shortcuts->bind({{"mute", tr("Mute or unmute MicForge"), "CTRL+ALT+M"},
                       {"push-to-talk", tr("Push to talk (hold to speak through MicForge)"), ""}});
}

void Controller::setShortcutsEnabled(bool on) {
    if (on == m_shortcutsEnabled) return;
    m_shortcutsEnabled = on;
    if (!m_startup.safeMode) QSettings().setValue("shortcuts/enabled", on);
    if (on) bindShortcuts();
    else if (m_pushToTalk) setMuted(false); // shortcuts off: push-to-talk can't work, so don't stay muted
    emit shortcutsChanged();
}

void Controller::setPushToTalk(bool on) {
    if (on == m_pushToTalk) return;
    m_pushToTalk = on;
    if (!m_startup.safeMode) QSettings().setValue("shortcuts/pushToTalk", on);
    if (m_shortcutsEnabled) setMuted(on);
    emit shortcutsChanged();
}

QString Controller::shortcutsStatus() const {
    if (!m_shortcuts) return {};
    if (!m_shortcuts->available()) return m_shortcuts->status();
    return m_shortcutsEnabled ? m_shortcuts->status() : tr("Off.");
}

void Controller::undo() {
    if (!m_undo.canUndo()) return;
    applyUndoStep(m_undo.takeUndo(), true);
}

void Controller::redo() {
    if (!m_undo.canRedo()) return;
    applyUndoStep(m_undo.takeRedo(), false);
}

void Controller::applyUndoStep(const UndoStack::Step &step, bool undo) {
    m_applyingUndo = true;
    bool soundChanged = false;
    for (int k = 0; k < step.changes.size(); ++k) {
        const auto &c = step.changes[undo ? step.changes.size() - 1 - k : k];
        const int id = idForKey(c.key);
        if (id < 0) continue;
        const double v = undo ? c.before : c.after;
        m_params.insert(c.key, v);
        applyToEngine(id, v);
        soundChanged |= !isPersonalKey(c.key);
    }
    m_applyingUndo = false;
    if (soundChanged && !m_currentPreset.isEmpty()) m_currentPreset.clear();
    emit presetsChanged();
    emit zonesChanged();
    emit undoChanged();
}

void Controller::updateMonitorStream() { m_backend->setMonitor(m_monitor || m_testPlayback); }

void Controller::setShowWelcome(bool on) {
    if (on == m_showWelcome) return;
    m_showWelcome = on;
    m_saveTimer.start();
    emit showWelcomeChanged();
}

void Controller::setThemeMode(const QString &mode) {
    const QString m = (mode == "dark" || mode == "light") ? mode : QStringLiteral("system");
    if (m == m_themeMode) return;
    m_themeMode = m;
    m_saveTimer.start();
    emit themeModeChanged();
}

// ---------------------------------------------------------------- meters

void Controller::pollMeters() {
    Meters &m = m_core.meters;
    m_inPeak = m.inPeak.exchange(-120);
    m_noiseVad = m.noiseVad.exchange(0);
    m_gateLevel = m.gateLevel.exchange(-120);
    m_gateReduction = m.gateReduction.exchange(0);
    m_gateOpen = m.gateOpen.exchange(0) > 0.5f;
    m_compLevel = m.compLevel.exchange(-120);
    m_compReduction = m.compReduction.exchange(0);
    m_autoLevelGain = m.autoLevelGain.load();
    m_gateTilt = m.gateTilt.exchange(-120);
    {
        // Show the most important reason seen recently ("open" or a rejection beats "too quiet") for ~0.3 s.
        const int code = m.gateReason.exchange(0);
        if (code >= m_reasonCode || --m_reasonHold <= 0) { m_reasonCode = code; m_reasonHold = 10; }
        static const char *names[] = {"", "quiet", "noise", "notvoice", "far", "open"};
        m_gateReason = QString::fromLatin1(names[std::clamp(m_reasonCode, 0, 5)]);
        m_gateFloor = m.gateFloor.load();
        m_gateOpenAt = m.gateOpenAt.load();
        m_humBase = m.humBase.load();
    }
    m_deessReduction = m.deessReduction.exchange(0);
    m_outPeak = m.outPeak.exchange(-120);
    m_limReduction = m.limReduction.exchange(0);
    emit metersChanged();
    calibTick();
}

// ---------------------------------------------------------------- presets

QString Controller::presetDir() const {
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/presets";
}

QStringList Controller::presetNames() const {
    QStringList names;
    for (const auto &p : builtinPresets()) names << p.name;
    QDir dir(presetDir());
    for (const QString &f : dir.entryList({"*.json"}, QDir::Files, QDir::Name)) names << f.chopped(5);
    return names;
}

bool Controller::isBuiltinPreset(const QString &name) const {
    for (const auto &p : builtinPresets())
        if (name == p.name) return true;
    return false;
}

void Controller::loadPreset(const QString &name) {
    UndoGroup group(this, tr("Load preset “%1”").arg(name));
    m_presetNotice.clear();
    for (const auto &p : builtinPresets()) {
        if (name != p.name) continue;
        // Built-in presets reset every sound setting, but never touch Setup values, added effects or plugins.
        QVariantMap values;
        for (int i = 0; i < PARAM_COUNT; ++i)
            if (!isPersonalKey(kParams[i].key)) values[kParams[i].key] = kParams[i].def;
        for (auto it = p.values.begin(); it != p.values.end(); ++it) values[it.key()] = it.value();
        for (auto it = values.begin(); it != values.end(); ++it) {
            const int id = idForKey(it.key());
            if (id < 0) continue;
            m_params.insert(it.key(), it.value().toDouble());
            applyToEngine(id, it.value().toDouble());
        }
        m_currentPreset = name;
        emit presetsChanged();
        return;
    }
    applyPresetFile(presetDir() + "/" + name + ".json", name);
}

// Loads a user preset file: its sound settings (never Setup values) and, if it has them, its plugins.
bool Controller::applyPresetFile(const QString &path, const QString &name) {
    QFile f(path);
    PresetFile pf;
    QString err;
    if (!f.open(QIODevice::ReadOnly) || !PresetFile::fromJson(f.readAll(), pf, &err)) {
        m_presetNotice = tr("Couldn't load “%1”: %2").arg(name, err.isEmpty() ? tr("the file can't be read") : err);
        emit presetsChanged();
        return false;
    }
    for (auto it = pf.params.begin(); it != pf.params.end(); ++it) {
        const int id = idForKey(it.key());
        if (id < 0) continue;
        m_params.insert(it.key(), it.value().toDouble());
        applyToEngine(id, it.value().toDouble());
    }
    if (pf.hasPlugins && m_plugins) {
        const QStringList missing = m_plugins->importChain(pf.plugins);
        if (!missing.isEmpty())
            m_presetNotice = tr("“%1” uses plugins that aren't installed here, so they were left out: %2.").arg(name, missing.join(", "));
    }
    m_currentPreset = name;
    emit presetsChanged();
    return true;
}

void Controller::savePreset(const QString &rawName) {
    const QString name = rawName.trimmed().replace('/', '-');
    if (name.isEmpty() || isBuiltinPreset(name) || m_startup.safeMode) return;
    QDir().mkpath(presetDir());
    PresetFile pf;
    pf.name = name;
    for (int i = 0; i < PARAM_COUNT; ++i)
        if (!isSetupKey(kParams[i].key)) pf.params[kParams[i].key] = m_params.value(kParams[i].key).toDouble();
    if (m_plugins) pf.plugins = m_plugins->exportChain();
    pf.hasPlugins = true;
    QFile f(presetDir() + "/" + name + ".json");
    if (!f.open(QIODevice::WriteOnly)) return;
    f.write(pf.toJson(version()));
    m_currentPreset = name;
    m_saveTimer.start();
    emit presetsChanged();
}

void Controller::deletePreset(const QString &name) {
    if (isBuiltinPreset(name)) return;
    QFile::remove(presetDir() + "/" + name + ".json");
    if (m_currentPreset == name) m_currentPreset.clear();
    emit presetsChanged();
}

QString Controller::exportPreset(const QString &name, const QUrl &file) {
    const QString to = file.isLocalFile() ? file.toLocalFile() : file.toString();
    PresetFile pf;
    pf.name = name;
    if (isBuiltinPreset(name)) {
        for (const auto &p : builtinPresets())
            if (p.name == name) for (auto it = p.values.begin(); it != p.values.end(); ++it) pf.params[it.key()] = it.value();
    } else {
        QFile src(presetDir() + "/" + name + ".json");
        QString err;
        if (!src.open(QIODevice::ReadOnly) || !PresetFile::fromJson(src.readAll(), pf, &err)) return tr("Couldn't read “%1”.").arg(name);
        pf.name = name;
    }
    const QByteArray data = pf.toJson(version()); // always the current format
    QFile out(to);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate) || out.write(data) != data.size()) return tr("Couldn't write %1.").arg(to);
    return tr("Exported “%1”.").arg(name);
}

QString Controller::importPreset(const QUrl &file) {
    if (m_startup.safeMode) return tr("Presets can't be imported in safe mode.");
    const QString from = file.isLocalFile() ? file.toLocalFile() : file.toString();
    QFile f(from);
    PresetFile pf;
    QString err;
    if (!f.open(QIODevice::ReadOnly) || !PresetFile::fromJson(f.readAll(), pf, &err))
        return err.isEmpty() ? tr("That file can't be read.") : err;
    QString base = (pf.name.isEmpty() ? QFileInfo(from).completeBaseName() : pf.name).trimmed().replace('/', '-');
    if (base.isEmpty()) base = tr("Imported preset");
    // Never overwrite an existing preset: pick a free name.
    QString name = base;
    for (int n = 2; isBuiltinPreset(name) || QFile::exists(presetDir() + "/" + name + ".json"); ++n) name = QString("%1 (%2)").arg(base).arg(n);
    pf.name = name;
    QDir().mkpath(presetDir());
    QFile out(presetDir() + "/" + name + ".json");
    if (!out.open(QIODevice::WriteOnly)) return tr("Couldn't save the imported preset.");
    out.write(pf.toJson(version()));
    out.close();
    emit presetsChanged();
    return name == base ? tr("Imported “%1”.").arg(name) : tr("Imported as “%1” (that name was taken).").arg(name);
}

// ---------------------------------------------------------------- autostart

bool Controller::autostart() const { return m_autostart->enabled(); }

bool Controller::autostartHidden() const { return QSettings().value("autostartHidden", true).toBool(); }

void Controller::setAutostart(bool on) {
    if (on == autostart()) return;
    m_autostart->setEnabled(on, autostartHidden());
    qInfo("autostart: %s (%s)", on ? "enabled" : "disabled", qPrintable(m_autostart->describe()));
    emit autostartChanged();
}

void Controller::setAutostartHidden(bool hidden) {
    if (hidden == autostartHidden()) return;
    QSettings().setValue("autostartHidden", hidden);
    if (autostart()) m_autostart->setEnabled(true, hidden);
    emit autostartChanged();
}

// ---------------------------------------------------------------- voice-separation setup
//
// Full setup:   prepRoom -> room -> [askOthers -> prepOthers -> others] -> prepYou -> you -> analysing -> done
// Learn voices: learnPrep -> learn -> analysing -> done
//   (adds an "other voices" sample to the saved recordings and re-analyses; nobody has to take part,
//    they just need to be talking nearby, e.g. on their own call)
// The recorder captures exactly what the gate's detector hears.

namespace {
struct PhaseDef { const char *name; int ticks; const char *next; };
const PhaseDef kPhases[] = {
    {"prepRoom", 45, "room"},       {"room", 120, "@afterRoom"},
    {"prepOthers", 75, "others"},   {"others", 300, "prepYou"},
    {"prepYou", 75, "you"},         {"you", 180, "analysing"},
    {"learnPrep", 60, "learn"},     {"learn", 300, "analysing"},
};

QString calibDir() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/calib"; }

void saveSamples(const QString &name, const std::vector<float> &v) {
    QDir().mkpath(calibDir());
    QFile f(calibDir() + "/" + name + ".f32");
    if (v.empty()) { f.remove(); return; }
    if (f.open(QIODevice::WriteOnly))
        f.write(reinterpret_cast<const char *>(v.data()), qint64(v.size() * sizeof(float)));
}

std::vector<float> loadSamples(const QString &name, float gain) {
    QFile f(calibDir() + "/" + name + ".f32");
    if (!f.open(QIODevice::ReadOnly)) return {};
    const QByteArray b = f.readAll();
    std::vector<float> v(size_t(b.size()) / sizeof(float));
    std::memcpy(v.data(), b.constData(), v.size() * sizeof(float));
    if (gain != 1.0f)
        for (auto &x : v) x *= gain;
    return v;
}

QString fmtDbS(double v) { return QString::number(v, 'f', 0) + " dB"; }
} // namespace

bool Controller::hasRecordings() const {
    return QFile::exists(calibDir() + "/room.f32") && QFile::exists(calibDir() + "/you.f32");
}

void Controller::applyNeeds() {
    const bool others = m_params.value("needOthers").toDouble() > 0.5;
    const bool noise = m_params.value("needNoise").toDouble() > 0.5;
    const bool broadcast = m_params.value("needBroadcast").toDouble() > 0.5;
    const bool builtIn = m_params.value("micType").toDouble() > 2.5;

    addEffect("gate"); // Setup needs the Voice Gate in the chain
    if (others) {
        setParam("gateMode", 0);   // clean mute keeps other voices out best
        setParam("gateRange", -40);
        setParam("gateLookahead", 5);
    } else {
        setParam("gateCloseCheck", 0);
        if (broadcast) setParam("gateMode", 1); // softer, more natural between phrases
    }
    if (builtIn) setParam("gateCloseCheck", 0);
    if (noise) {
        addEffect("noise");
        setParam("lowCutFreq", std::max(100.0, m_params.value("lowCutFreq").toDouble()));
    }
    if (broadcast) {
        addEffect("deess");
        setParam("compRatio", 4);
        setParam("tonePresence", std::max(2.0, m_params.value("tonePresence").toDouble()));
    }
}

void Controller::startCalibration() {
    m_analysis.waitForFinished();
    setParam("bypass", 0);
    addEffect("gate"); // Setup needs the Voice Gate in the chain
    m_learning = false;
    m_redo.clear();
    m_takes.clear();
    m_takeClips.clear();
    m_calibResult.clear();
    enterPhase("prepRoom");
}

void Controller::startLearnOthers() {
    if (!hasRecordings()) return;
    m_analysis.waitForFinished();
    setParam("bypass", 0);
    addEffect("gate"); // Setup needs the Voice Gate in the chain
    m_learning = true;
    m_redo.clear();
    m_takes.clear();
    m_takeClips.clear();
    m_calibResult.clear();
    enterPhase("learnPrep");
}

// Re-record one step ("room", "others" or "you") and re-tune using the saved recordings for the rest.
void Controller::redoStep(const QString &step) {
    if (!hasRecordings()) return;
    if (step != "room" && step != "others" && step != "you") return;
    m_analysis.waitForFinished();
    setParam("bypass", 0);
    addEffect("gate"); // Setup needs the Voice Gate in the chain
    m_learning = false;
    m_redo = step;
    m_takes.clear();
    m_takeClips.clear();
    m_calibResult.clear();
    enterPhase(step == "room" ? "prepRoom" : step == "others" ? "prepOthers" : "prepYou");
}

// Re-analyse the saved recordings with the current analysis (used once after an update that improves it).
void Controller::retuneFromRecordings() {
    if (!hasRecordings() || !m_calibPhase.isEmpty()) return;
    m_analysis.waitForFinished();
    m_learning = false;
    m_redo.clear();
    m_takes.clear();
    m_takeClips.clear();
    m_retune = true;
    m_calibPhase = "analysing";
    startAnalysis();
}

// Start the step that's currently recording again from its countdown (e.g. someone bumped the desk).
void Controller::restartStep() {
    QString prep;
    if (m_calibPhase == "room" || m_calibPhase == "prepRoom") prep = "prepRoom";
    else if (m_calibPhase == "others" || m_calibPhase == "prepOthers") prep = "prepOthers";
    else if (m_calibPhase == "you" || m_calibPhase == "prepYou") prep = "prepYou";
    else if (m_calibPhase == "learn" || m_calibPhase == "learnPrep") prep = "learnPrep";
    if (prep.isEmpty()) return;
    m_calibPhase = "restarting"; // don't keep the interrupted take
    enterPhase(prep);
}

void Controller::enterPhase(const QString &phase) {
    Recorder &rec = m_core.recorder;
    static const QStringList takes = {"room", "others", "you", "learn"};
    // Keep the take that just finished.
    if (takes.contains(m_calibPhase)) {
        const size_t to = std::min(rec.pos.load(), rec.buf.size());
        m_takes[m_calibPhase] = to > m_takeFrom ? std::vector<float>(rec.buf.begin() + m_takeFrom, rec.buf.begin() + to)
                                                : std::vector<float>{};
        m_takeClips[m_calibPhase] = m_core.stats.inputClips.load() - m_clipsFrom;
    }
    // Each step records into a fresh buffer: restart the recorder at every countdown.
    if (phase.startsWith("prep") || phase == "learnPrep") {
        rec.active = false;
        rec.pos = 0;
    }
    if (takes.contains(phase)) {
        m_takeFrom = rec.pos.load();
        m_clipsFrom = m_core.stats.inputClips.load();
    }

    rec.active = phase.startsWith("prep") || phase == "learnPrep" || takes.contains(phase);
    m_calibPhase = phase;
    m_calibTicks = 0;
    m_calibProgress = 0;
    emit calibChanged();
    if (phase == "analysing") startAnalysis();
}

void Controller::calibTick() {
    for (const auto &p : kPhases) {
        if (m_calibPhase != p.name) continue;
        ++m_calibTicks;
        m_calibProgress = double(m_calibTicks) / p.ticks;
        if (m_calibTicks >= p.ticks) {
            QString next = p.next;
            const bool takeDone = m_calibPhase == "room" || m_calibPhase == "others" || m_calibPhase == "you";
            if (!m_redo.isEmpty() && takeDone) next = "analysing"; // redoing one step only
            else if (next == "@afterRoom")
                next = m_params.value("needOthers").toDouble() > 0.5 ? "askOthers" : "prepYou";
            enterPhase(next);
        } else {
            emit calibChanged();
        }
        return;
    }
}

void Controller::skipOthers() {
    if (m_calibPhase != "askOthers") return;
    m_takes.erase("others");
    enterPhase("prepYou");
}

void Controller::continueCalibration() {
    if (m_calibPhase == "askOthers") enterPhase("prepOthers");
}

void Controller::cancelCalibration() {
    m_core.recorder.active = false;
    m_calibPhase.clear();
    emit calibChanged();
}

void Controller::startAnalysis() {
    auto take = [&](const char *n) {
        auto it = m_takes.find(n);
        return it == m_takes.end() ? std::vector<float>{} : it->second;
    };
    const float curGain = float(m_params.value("inputGain").toDouble());
    std::vector<float> room, others, you;
    if (m_learning || m_retune || !m_redo.isEmpty()) {
        // Saved recordings were made at an older input gain: bring them to today's level.
        const float g = dbToLin(curGain - float(QSettings().value("calib/recGain", curGain).toDouble()));
        room = loadSamples("room", g);
        you = loadSamples("you", g);
        others = loadSamples("others", g);
    }
    if (m_retune) {
        // nothing new: just re-analyse the saved recordings
    } else if (m_learning) {
        auto fresh = take("learn");
        others.insert(others.end(), fresh.begin(), fresh.end());
        const size_t maxOthers = 48000 * 60; // keep the most recent minute of other voices
        if (others.size() > maxOthers) others.erase(others.begin(), others.end() - maxOthers);
    } else if (m_redo == "room") {
        room = take("room");
    } else if (m_redo == "others") {
        others = take("others");
    } else if (m_redo == "you") {
        you = take("you");
    } else {
        room = take("room");
        you = take("you");
        others = take("others");
    }
    m_youLevels = analyseLevels(you.data(), you.size(), AudioCore::kRate);
    m_roomLevels = analyseLevels(room.data(), room.size(), AudioCore::kRate);
    saveSamples("room", room);
    saveSamples("you", you);
    saveSamples("others", others);
    QSettings().setValue("calib/recGain", curGain);
    emit recordingsChanged();

    m_analysis.setFuture(QtConcurrent::run(
        [room = std::move(room), others = std::move(others), you = std::move(you)] {
            return analyseSeparation(room, others, you, 48000);
        }));
}

void Controller::applyAnalysis(const SeparationResult &r) {
    UndoGroup group(this, tr("Setup"));
    if (m_calibPhase != "analysing") return; // cancelled meanwhile
    QVariantMap out;
    out["learning"] = m_learning;
    out["redo"] = m_redo;
    out["hadOthers"] = r.hadOthers;
    {
        static const char *statusNames[] = {"ok", "warn", "bad", "none"};
        auto row = [&](const char *id, const QString &label, const StepCheck &c) {
            QVariantMap m;
            m["id"] = id;
            m["label"] = label;
            m["status"] = statusNames[c.status];
            m["note"] = tr(c.note);
            return m;
        };
        QVariantList steps;
        steps << row("room", tr("Quiet room"), checkRoom(r));
        if (m_params.value("needOthers").toDouble() > 0.5 || r.hadOthers)
            steps << row("others", tr("Other voices"), checkOthers(r));
        QVariantMap youRow = row("you", tr("Your voice"), checkYou(r));

        // Level checks: clipping happens before MicForge (in the mic or interface) and can't be undone
        // in software, so it outranks everything else; then how far the voice stands above the room.
        QSettings cs;
        if (m_takeClips.count("you")) cs.setValue("calib/youClips", qulonglong(m_takeClips["you"]));
        const qulonglong clips = cs.value("calib/youClips", 0).toULongLong();
        const double range = m_youLevels.valid && m_roomLevels.valid ? m_youLevels.speechDb - m_roomLevels.speechDb : 0.0;
        if (clips > 0) {
            youRow["status"] = "bad";
            youRow["note"] = tr("Your mic clipped (distorted) %n time(s) while you talked. Turn down the gain on your "
                                "interface or mic, then redo this step.", nullptr, int(std::min<qulonglong>(clips, 1u << 30)));
        } else if (youRow["status"] == "ok" && m_youLevels.valid && m_roomLevels.valid && range < 15) {
            youRow["status"] = "warn";
            youRow["note"] = tr("Your voice is only %1 dB above the room's background. Move closer to the mic or "
                                "make the room quieter, then redo this step.").arg(qRound(range));
        }
        steps << youRow;
        out["steps"] = steps;
        out["levels"] = QVariantMap{{"valid", m_youLevels.valid && m_roomLevels.valid}, {"range", range},
                                    {"youPeak", m_youLevels.peakDb}, {"clips", clips}};
    }
    const bool needOthers = m_params.value("needOthers").toDouble() > 0.5 || m_learning;
    const bool keepLevels = m_learning || m_retune; // don't touch gain/compressor
    const bool builtIn = m_params.value("micType").toDouble() > 2.5;

    if (!r.heardYou) {
        out["ok"] = false;
        out["message"] = tr("I couldn't hear you clearly in the “Your voice” recording, so no settings were "
                            "changed. Check the Input meter moves when you talk (right microphone and input "
                            "selected?), then redo just that step below.");
        m_calibResult = out;
        m_calibPhase = m_retune ? "" : "done";
        m_retune = false;
        emit calibChanged();
        return;
    }

    // Input gain: aim for speech peaks around -12 dBFS (full setup only).
    const double oldGain = m_params.value("inputGain").toDouble();
    const double newGain = keepLevels ? oldGain : roundHalf(std::clamp(oldGain + (-12.0 - r.youPeakDb), -12.0, 36.0));
    const double d = newGain - oldGain;
    const double thr = roundHalf(std::clamp(r.threshold + d, -80.0, 0.0));
    const bool closeOn = needOthers && !builtIn;

    if (!keepLevels) {
        const double speech = r.youPeakDb + d;
        const double ratio = m_params.value("compRatio").toDouble();
        const double cthr = roundHalf(std::clamp(speech - 10, -60.0, 0.0));
        const double est = (speech - cthr) * (1.0 - 1.0 / std::max(1.0, ratio));
        const double makeup = roundHalf(std::clamp(-9.0 - (speech - est), 0.0, 18.0));
        setParam("inputGain", newGain);
        setParam("compThreshold", cthr);
        setParam("compMakeup", makeup);
        out["compThreshold"] = cthr;
        out["makeup"] = makeup;
    }
    if (m_learning) setParam("needOthers", 1);
    setParam("gateFocus", r.focus ? 1 : 0);
    setParam("gateScLow", r.scLow);
    setParam("gateScHigh", r.scHigh);
    setParam("gateThreshold", thr);
    setParam("gateHysteresis", r.hysteresis);
    setParam("gateLookahead", std::max(3.0, m_params.value("gateLookahead").toDouble()));
    setParam("gateCloseCheck", closeOn ? 1 : 0);
    if (closeOn) {
        setParam("gateTilt", r.tiltThreshold);
        setParam("gateTiltDir", r.tiltBrighter ? 1 : 0);
    }

    // Remember the measured zones so the gate meter can show them.
    QSettings s;
    s.setValue("calib/valid", true);
    s.setValue("calib/room", r.roomDb + d);
    s.setValue("calib/others", r.hadOthers ? r.othersDb + d : -200.0);
    s.setValue("calib/you", r.youDb + d);
    s.setValue("calib/gain", newGain);
    s.setValue("calib/recGain", newGain); // recordings are rescaled from this gain next time
    s.setValue("calib/focus", r.focus);
    s.setValue("calib/scLow", r.scLow);
    s.setValue("calib/scHigh", r.scHigh);
    s.setValue("calib/youTilt", r.youTilt);
    s.setValue("calib/othersTilt", r.hadOthers ? r.othersTilt : -200.0);
    s.remove("calib/partner"); s.remove("calib/partnerLoud"); s.remove("calib/partnerTilt"); s.remove("calib/hasTilt");
    // Recordings now correspond to the new gain: rescale the saved files once.
    if (d != 0) {
        const float g = dbToLin(float(d));
        for (const char *n : {"room", "you", "others"}) saveSamples(n, loadSamples(n, g));
    }
    emit zonesChanged();

    QStringList tips;
    const int mic = int(m_params.value("micType").toDouble() + 0.5);
    if (builtIn)
        tips << tr("Built-in laptop/webcam mics hear the whole room, so telling voices apart is limited. "
                   "A headset, or a dynamic mic close to your mouth, makes a big difference.");
    if (needOthers && !r.hadOthers && !builtIn)
        tips << tr("No other voices were recorded, so the closeness check was set from your voice alone: it "
                   "blocks voices that sound further away than yours. Next time someone is talking nearby, "
                   "click “Learn other voices” (they don't need to do anything) to fine-tune it.");
    if (r.hadOthers && !r.tiltMeasured && !builtIn)
        tips << tr("Other voices sounded about as close as yours, so the closeness check uses your voice "
                   "only. Moving the mic closer to your mouth makes your voice stand out more.");
    if (r.hadOthers && r.quality <= 1) {
        if (mic == 0 || mic == 1)
            tips << tr("Get closer: 2–5 cm from the mic. Halving the distance makes your voice about 6 dB louder "
                       "than anyone else, and boosts its bass (which the closeness check uses).");
        if (mic == 0)
            tips << tr("Point the back of the mic (the cable end) towards the other person: that's where a "
                       "dynamic cardioid mic like the SM57/SM58 is deaf.");
        if (mic == 2)
            tips << tr("Move the headset boom so the mic sits right at the corner of your mouth.");
    }

    out["ok"] = true;
    out["quality"] = r.quality;
    out["separation"] = r.separation;
    out["broadbandSeparation"] = r.broadbandSeparation;
    out["room"] = r.roomDb + d;
    out["others"] = r.hadOthers ? r.othersDb + d : -200.0;
    out["you"] = r.youDb + d;
    out["threshold"] = thr;
    out["closeAt"] = thr - r.hysteresis;
    out["band"] = r.focus ? (r.scLow > 21 ? fmtHz(r.scLow) + " – " : QStringLiteral("below ")) + fmtHz(r.scHigh)
                          : tr("full range");
    out["gain"] = newGain;
    out["closeCheck"] = closeOn;
    out["tiltMeasured"] = r.tiltMeasured;
    out["tiltGap"] = r.tiltGap;
    out["youTilt"] = r.youTilt;
    out["othersTilt"] = r.othersTilt;
    out["tiltThreshold"] = r.tiltThreshold;
    out["headroom"] = r.hadOthers ? thr - (r.othersDb + d) : 0.0;
    out["tips"] = tips;
    m_calibResult = out;
    m_calibPhase = "done";
    emit calibChanged();
    emit recordingsChanged();
    QSettings().setValue("calib/version", kCalibVersion);
    saveProfileFor(m_currentSource); // this Setup now belongs to the microphone in use
    if (m_retune) {
        // Silent re-tune: don't leave a result page behind for the Setup window.
        m_retune = false;
        m_calibPhase.clear();
        m_calibResult.clear();
        emit calibChanged();
    }
    Q_UNUSED(fmtDbS);
}

QVariantMap Controller::zones() const {
    QSettings s;
    QVariantMap z;
    const auto near = [](double a, double b) { return std::fabs(a - b) < 0.5; };
    const bool valid = s.value("calib/valid").toBool() && s.contains("calib/others") &&
                       (s.value("calib/focus").toBool() == (m_params.value("gateFocus").toDouble() > 0.5)) &&
                       near(s.value("calib/scLow").toDouble(), m_params.value("gateScLow").toDouble()) &&
                       near(s.value("calib/scHigh").toDouble(), m_params.value("gateScHigh").toDouble());
    z["valid"] = valid;
    if (!valid) return z;
    const double off = m_params.value("inputGain").toDouble() - s.value("calib/gain").toDouble();
    z["room"] = s.value("calib/room").toDouble() + off;
    z["others"] = s.value("calib/others").toDouble() + off;
    z["you"] = s.value("calib/you").toDouble() + off;
    z["youTilt"] = s.value("calib/youTilt").toDouble();
    z["othersTilt"] = s.value("calib/othersTilt", -200.0).toDouble();
    return z;
}

// ---------------------------------------------------------------- audio engine lifecycle

void Controller::startEngine() {
    if (m_backend->start()) {
        if (m_engineAttempts > 0) qInfo("audio engine: connected to %s after %d retries", m_backend->name().c_str(), m_engineAttempts);
        else qInfo("audio engine: connected to %s", m_backend->name().c_str());
        m_engineAttempts = 0; // start() already (re)created the capture stream for the chosen microphone
    } else {
        if (m_engineAttempts == 0 || m_engineAttempts % 10 == 0)
            qWarning("audio engine: %s (attempt %d); will retry", m_backend->error().c_str(), m_engineAttempts + 1);
        scheduleEngineRetry(false);
    }
    emit statusChanged();
}

// Reconnect with a short back-off (0.5, 1, 2, 4 s, then every 5 s) instead of a fixed start-up delay:
// MicForge starts immediately and connects as soon as PipeWire is reachable.
void Controller::scheduleEngineRetry(bool immediate) {
    if (m_engineRetry.isActive()) return;
    const int steps[] = {500, 1000, 2000, 4000, 5000};
    const int ms = immediate ? 300 : steps[std::min(m_engineAttempts, 4)];
    ++m_engineAttempts;
    m_engineRetry.start(ms);
    emit statusChanged();
}

void Controller::restartEngine() {
    qInfo("audio engine: restart requested");
    m_engineRetry.stop();
    m_engineAttempts = 0;
    startEngine();
}

QString Controller::engineState() const {
    if (m_backend->connected()) return QStringLiteral("running");
    return m_engineRetry.isActive() ? QStringLiteral("reconnecting") : QStringLiteral("stopped");
}

QString Controller::appState() const {
    if (!m_backend->connected()) return m_engineRetry.isActive() ? "connecting" : "disconnected";
    if (!m_backend->inputRunning()) return "no-input";
    if (muted()) return "muted";
    if (m_params.value("bypass").toDouble() > 0.5) return "bypassed";
    return "processing";
}

QString Controller::appStateText() const {
    const QString s = appState();
    if (s == "disconnected") return tr("Audio engine stopped");
    if (s == "connecting") return tr("Connecting to %1…").arg(backendName());
    if (s == "no-input") return m_currentSource.isEmpty() ? tr("No microphone selected") : tr("Microphone not connected");
    if (s == "muted") return tr("Muted: apps hear silence");
    if (s == "bypassed") return tr("Bypassed: apps hear your raw mic");
    return virtualMicInUse() ? tr("Live: an app is using MicForge") : tr("Ready: pick “MicForge” as your mic in any app");
}

QString Controller::engineError() const { return QString::fromStdString(m_backend->error()); }

void Controller::openLogFolder() { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_startup.logPath).path())); }

void Controller::requestRestart(bool safeMode) { emit restartRequested(safeMode); }

