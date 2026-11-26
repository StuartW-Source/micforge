#include "pluginmanager.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace mf;

PluginManager::PluginManager(PluginRack *rack, double sampleRate, QObject *parent)
    : QObject(parent), m_rack(rack), m_rate(sampleRate) {
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(800);
    connect(&m_saveTimer, &QTimer::timeout, this, &PluginManager::save);
}

PluginManager::~PluginManager() {
    save();
    for (auto &s : m_rack->slot) s.store(nullptr);
    m_rack->waitIdle();
    m_entries.clear();
}

PluginManager::Entry *PluginManager::entry(int slot) {
    return slot >= 0 && size_t(slot) < m_entries.size() ? &m_entries[size_t(slot)] : nullptr;
}
const PluginManager::Entry *PluginManager::entry(int slot) const {
    return slot >= 0 && size_t(slot) < m_entries.size() ? &m_entries[size_t(slot)] : nullptr;
}

// ---------------------------------------------------------------- discovery

int PluginManager::runScanHelper(const QString &bundle) {
    QJsonArray arr;
    for (const auto &i : scanVst3Bundle(bundle)) arr.append(QJsonObject::fromVariantMap(i.toMap()));
    std::fputs(QJsonDocument(arr).toJson(QJsonDocument::Compact).constData(), stdout);
    std::fflush(stdout);
    return 0;
}

void PluginManager::rescan(bool force) {
    if (m_scanning) return;
    m_scanning = true;
    m_forceScan = force;
    m_scanStatus = tr("Looking for LV2 plugins…");
    emit scanningChanged();
    // Let the UI show the status first.
    QTimer::singleShot(30, this, [this] {
        m_found = scanLv2();
        const auto b = vst3Bundles();
        m_pendingBundles.assign(b.begin(), b.end());
        scanVst3Next();
    });
}

// VST3 bundles are opened by a helper process (one per bundle), so a broken plugin can't crash
// MicForge. Results are cached by bundle path + modification time.
void PluginManager::scanVst3Next() {
    if (m_pendingBundles.empty()) {
        finishScan();
        return;
    }
    const QString bundle = m_pendingBundles.back();
    m_pendingBundles.pop_back();
    m_scanStatus = tr("Checking %1…").arg(QFileInfo(bundle).fileName());
    emit scanningChanged();

    QSettings s;
    const QString key = "vst3cache/" + QString(bundle.toUtf8().toBase64(QByteArray::Base64UrlEncoding));
    const qint64 mtime = QFileInfo(bundle).lastModified().toSecsSinceEpoch();
    if (!m_forceScan && s.value(key + "/mtime").toLongLong() == mtime) {
        for (const auto &v : s.value(key + "/plugins").toList()) m_found.push_back(PluginInfo::fromMap(v.toMap()));
        QTimer::singleShot(0, this, &PluginManager::scanVst3Next);
        return;
    }
    auto *proc = new QProcess(this);
    connect(proc, &QProcess::finished, this, [this, proc, key, mtime](int code, QProcess::ExitStatus st) {
        QVariantList list;
        if (st == QProcess::NormalExit && code == 0) {
            for (const auto &v : QJsonDocument::fromJson(proc->readAllStandardOutput()).array()) {
                list << v.toObject().toVariantMap();
                m_found.push_back(PluginInfo::fromMap(v.toObject().toVariantMap()));
            }
        }
        QSettings s;
        s.setValue(key + "/mtime", mtime);
        s.setValue(key + "/plugins", list); // a crashed bundle is cached as empty (not retried every scan)
        proc->deleteLater();
        scanVst3Next();
    });
    proc->start(QCoreApplication::applicationFilePath(), {"--scan-vst3", bundle});
    QTimer::singleShot(15000, proc, [proc] { if (proc->state() != QProcess::NotRunning) proc->kill(); });
}

void PluginManager::finishScan() {
    std::sort(m_found.begin(), m_found.end(), [](const PluginInfo &a, const PluginInfo &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    m_available.clear();
    for (const auto &i : m_found) m_available << i.toMap();
    int lv2 = 0, vst3 = 0;
    for (const auto &i : m_found) (i.format == "LV2" ? lv2 : vst3)++;
    m_scanning = false;
    m_scanStatus = tr("%1 plugins found (%2 VST3, %3 LV2)").arg(m_found.size()).arg(vst3).arg(lv2);
    emit availableChanged();
    emit scanningChanged();
}

// ---------------------------------------------------------------- chain

bool PluginManager::instantiate(Entry &e) {
    // Crash guard: remember which plugin is being loaded. If MicForge dies during loading,
    // the next start sees the marker and leaves that plugin switched off.
    QSettings s;
    s.setValue("plugins/loading", e.info.format + ":" + e.info.id);
    s.sync();
    QString err;
    e.inst = createPlugin(e.info, m_rate, PluginRack::kMaxBlock, &err);
    s.remove("plugins/loading");
    s.sync();
    if (!e.inst) {
        e.error = tr("This plugin %1.").arg(err.isEmpty() ? tr("couldn't be loaded") : err);
        return false;
    }
    e.error.clear();
    e.inst->bypass = e.bypass;
    for (auto it = e.saved.begin(); it != e.saved.end(); ++it) {
        const int idx = e.inst->indexOf(it.key());
        if (idx >= 0) e.inst->setValue(idx, it.value().toFloat());
    }
    return true;
}

void PluginManager::publish() {
    for (int i = 0; i < PluginRack::kSlots; ++i) {
        PluginInstance *p = size_t(i) < m_entries.size() ? m_entries[size_t(i)].inst.get() : nullptr;
        m_rack->slot[size_t(i)].store(p, std::memory_order_release);
    }
    emit chainChanged();
    m_saveTimer.start();
}

QVariantList PluginManager::chain() const {
    QVariantList out;
    for (const auto &e : m_entries) {
        QVariantMap m = e.info.toMap();
        m["bypass"] = e.bypass;
        m["error"] = e.error;
        m["loaded"] = bool(e.inst);
        m["paramCount"] = e.inst ? e.inst->paramCount() : 0;
        m["latency"] = e.inst ? e.inst->latency : 0;
        out << m;
    }
    return out;
}

bool PluginManager::add(const QString &format, const QString &id) {
    if (int(m_entries.size()) >= PluginRack::kSlots) return false;
    auto it = std::find_if(m_found.begin(), m_found.end(), [&](const PluginInfo &i) { return i.format == format && i.id == id; });
    if (it == m_found.end()) return false;
    Entry e;
    e.info = *it;
    const bool ok = instantiate(e);
    m_entries.push_back(std::move(e));
    publish();
    return ok;
}

void PluginManager::remove(int slot) {
    if (!entry(slot)) return;
    // Take it away from the audio thread first, wait for any block in flight, then destroy it.
    auto victim = std::move(m_entries[size_t(slot)].inst);
    m_entries.erase(m_entries.begin() + slot);
    publish();
    m_rack->waitIdle();
    victim.reset();
}

void PluginManager::move(int slot, int delta) {
    const int to = slot + delta;
    if (!entry(slot) || !entry(to)) return;
    std::swap(m_entries[size_t(slot)], m_entries[size_t(to)]);
    publish();
}

void PluginManager::setBypass(int slot, bool bypass) {
    Entry *e = entry(slot);
    if (!e) return;
    e->bypass = bypass;
    if (e->inst) e->inst->bypass = bypass;
    emit chainChanged();
    m_saveTimer.start();
}

void PluginManager::retry(int slot) {
    Entry *e = entry(slot);
    if (!e || e->inst) return;
    instantiate(*e);
    publish();
}

// ---------------------------------------------------------------- parameters

QVariantList PluginManager::params(int slot) const {
    QVariantList out;
    const Entry *e = entry(slot);
    if (!e || !e->inst) return out;
    for (int i = 0; i < e->inst->paramCount(); ++i) {
        const auto &p = e->inst->param(i);
        QVariantMap m;
        m["index"] = i;
        m["id"] = p.id;
        m["name"] = p.name;
        m["min"] = p.min;
        m["max"] = p.max;
        m["def"] = p.def;
        m["toggle"] = p.toggle;
        m["integer"] = p.integer;
        m["log"] = p.logScale;
        m["choices"] = p.choices;
        QVariantList cv;
        for (float v : p.choiceValues) cv << v;
        m["choiceValues"] = cv;
        out << m;
    }
    return out;
}

double PluginManager::value(int slot, int index) const {
    const Entry *e = entry(slot);
    return e && e->inst ? e->inst->value(index) : 0.0;
}

void PluginManager::setValue(int slot, int index, double v) {
    Entry *e = entry(slot);
    if (!e || !e->inst || index < 0 || index >= e->inst->paramCount()) return;
    e->inst->setValue(index, float(v));
    e->saved[e->inst->param(index).id] = e->inst->value(index);
    ++m_revision;
    emit valuesChanged();
    m_saveTimer.start();
}

void PluginManager::resetValue(int slot, int index) {
    Entry *e = entry(slot);
    if (e && e->inst && index >= 0 && index < e->inst->paramCount()) setValue(slot, index, e->inst->param(index).def);
}

QString PluginManager::text(int slot, int index, double v) const {
    const Entry *e = entry(slot);
    return e && e->inst && index >= 0 && index < e->inst->paramCount() ? e->inst->valueText(index, float(v)) : QString();
}

// ---------------------------------------------------------------- presets

QVariantList PluginManager::exportChain() const {
    QVariantList out;
    for (const auto &e : m_entries) {
        QVariantMap vals = e.saved;
        if (e.inst)
            for (int i = 0; i < e.inst->paramCount(); ++i) vals[e.inst->param(i).id] = e.inst->value(i);
        out << QVariantMap{{"format", e.info.format}, {"id", e.info.id}, {"name", e.info.name},
                           {"bypass", e.bypass}, {"params", vals}};
    }
    return out;
}

QStringList PluginManager::importChain(const QVariantList &chain) {
    if (m_readOnly) return {};
    // Resolve every plugin first (scan results, then the current chain; LV2 needs only its URI).
    std::vector<Entry> fresh;
    QStringList missing;
    for (const QVariant &v : chain) {
        if (int(fresh.size()) >= PluginRack::kSlots) break;
        const QVariantMap m = v.toMap();
        const QString format = m.value("format").toString(), id = m.value("id").toString();
        PluginInfo info;
        bool found = false;
        for (const auto &i : m_found) if (i.format == format && i.id == id) { info = i; found = true; break; }
        for (const auto &e : m_entries) if (!found && e.info.format == format && e.info.id == id) { info = e.info; found = true; }
        if (!found && format == "LV2") { info.format = format; info.id = id; info.name = m.value("name").toString(); found = true; }
        if (!found) { missing << (m.value("name").toString().isEmpty() ? id : m.value("name").toString()); continue; }
        Entry e;
        e.info = info;
        e.bypass = m.value("bypass").toBool();
        e.saved = m.value("params").toMap();
        fresh.push_back(std::move(e));
    }
    // Take the old chain away from the audio thread, wait for any block in flight, then replace it.
    std::vector<Entry> old;
    old.swap(m_entries);
    publish();
    m_rack->waitIdle();
    old.clear();
    for (auto &e : fresh) {
        if (!instantiate(e) && e.error.isEmpty()) e.error = tr("This plugin couldn't be loaded.");
        m_entries.push_back(std::move(e));
    }
    publish();
    save();
    return missing;
}

// ---------------------------------------------------------------- persistence

void PluginManager::save() const {
    if (m_readOnly) return;
    QJsonArray arr;
    for (const auto &e : m_entries) {
        QJsonObject o = QJsonObject::fromVariantMap(e.info.toMap());
        o["bypass"] = e.bypass;
        QVariantMap vals = e.saved;
        if (e.inst)
            for (int i = 0; i < e.inst->paramCount(); ++i) vals[e.inst->param(i).id] = e.inst->value(i);
        o["params"] = QJsonObject::fromVariantMap(vals);
        arr.append(o);
    }
    QSettings().setValue("plugins/chain", QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

void PluginManager::restore() {
    QSettings s;
    const QString crashed = s.value("plugins/loading").toString();
    s.remove("plugins/loading");
    const auto arr = QJsonDocument::fromJson(s.value("plugins/chain").toString().toUtf8()).array();
    for (const auto &v : arr) {
        if (int(m_entries.size()) >= PluginRack::kSlots) break;
        const QJsonObject o = v.toObject();
        Entry e;
        e.info = PluginInfo::fromMap(o.toVariantMap());
        e.bypass = o["bypass"].toBool();
        e.saved = o["params"].toObject().toVariantMap();
        if (!crashed.isEmpty() && crashed == e.info.format + ":" + e.info.id)
            e.error = tr("MicForge closed unexpectedly while loading this plugin last time, so it's been switched "
                         "off. Click Retry to try again.");
        else
            instantiate(e);
        m_entries.push_back(std::move(e));
    }
    publish();
}
