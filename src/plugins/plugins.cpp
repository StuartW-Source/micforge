#include "plugins.h"

#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QThread>

#include <algorithm>
#include <cmath>

namespace mf {

QVariantMap PluginInfo::toMap() const {
    return {{"format", format}, {"id", id}, {"path", path}, {"name", name}, {"vendor", vendor}, {"category", category}};
}

PluginInfo PluginInfo::fromMap(const QVariantMap &m) {
    PluginInfo i;
    i.format = m.value("format").toString();
    i.id = m.value("id").toString();
    i.path = m.value("path").toString();
    i.name = m.value("name").toString();
    i.vendor = m.value("vendor").toString();
    i.category = m.value("category").toString();
    return i;
}

void PluginInstance::initValues() {
    values.reset(new std::atomic<float>[params.size() ? params.size() : 1]);
    for (size_t i = 0; i < params.size(); ++i) values[i].store(params[i].def);
}

void PluginInstance::setValue(int i, float v) {
    if (i < 0 || i >= paramCount()) return;
    const auto &p = params[size_t(i)];
    v = std::clamp(v, std::min(p.min, p.max), std::max(p.min, p.max));
    if (p.integer || p.toggle) v = std::round(v);
    values[size_t(i)].store(v, std::memory_order_relaxed);
    queue.push({i, v});
    onValueSet(i, v);
}

QString PluginInstance::valueText(int i, float v) const {
    const auto &p = params[size_t(i)];
    if (p.toggle) return v > 0.5f ? "On" : "Off";
    for (size_t k = 0; k < p.choiceValues.size(); ++k)
        if (std::fabs(p.choiceValues[k] - v) < 1e-4f) return p.choices[int(k)];
    const int decimals = p.integer ? 0 : (std::fabs(p.max - p.min) > 100 ? 0 : std::fabs(p.max - p.min) > 10 ? 1 : 2);
    return QString::number(double(v), 'f', decimals) + (p.unit.isEmpty() ? "" : " " + p.unit);
}

int PluginInstance::indexOf(const QString &paramId) const {
    for (int i = 0; i < paramCount(); ++i)
        if (params[size_t(i)].id == paramId) return i;
    return -1;
}

void PluginRack::waitIdle() const {
    // Audio blocks are a few milliseconds; give up after half a second (e.g. audio stopped mid-block).
    for (int i = 0; i < 500 && busy.load(std::memory_order_acquire); ++i) QThread::msleep(1);
}

std::vector<QString> vst3Bundles() {
    QStringList dirs = {QDir::homePath() + "/.vst3", "/usr/lib/vst3", "/usr/local/lib/vst3"};
    const QString extra = qEnvironmentVariable("VST3_PATH");
    if (!extra.isEmpty()) dirs = extra.split(':') + dirs;
    std::vector<QString> out;
    for (const QString &d : dirs) {
        QDirIterator it(d, {"*.vst3"}, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString p = it.next();
            // Skip bundles nested inside other bundles.
            if (p.indexOf(".vst3/") >= 0) continue;
            if (std::find(out.begin(), out.end(), p) == out.end()) out.push_back(p);
        }
    }
    return out;
}

} // namespace mf
