// Third-party plugin hosting (LV2 and VST3) for MicForge's "Plugins" stage.
//
// Threading model
//   * Scanning, creating, activating and destroying plugins happens on the UI thread.
//   * PluginInstance::process() runs on the audio thread and must not block or allocate.
//   * Parameter changes from the UI go through a lock-free queue (or atomic values) and are
//     applied by the audio thread at the start of the next block.
#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace mf {

struct PluginInfo {
    QString format;   // "LV2" or "VST3"
    QString id;       // LV2: plugin URI.  VST3: class ID (hex string)
    QString path;     // VST3: bundle path (empty for LV2)
    QString name;
    QString vendor;
    QString category;
    QVariantMap toMap() const;
    static PluginInfo fromMap(const QVariantMap &m);
};

struct PluginParam {
    QString id;        // LV2: port symbol.  VST3: parameter ID as text
    QString name;
    QString unit;
    float min = 0, max = 1, def = 0;
    bool toggle = false, integer = false, logScale = false;
    QStringList choices;          // enumeration labels (value = index into choiceValues)
    std::vector<float> choiceValues;
};

// Single-producer / single-consumer queue of parameter changes (UI -> audio).
class ParamQueue {
public:
    struct Change { int index; float value; };
    bool push(Change c) {
        const unsigned w = wr.load(std::memory_order_relaxed);
        if (w - rd.load(std::memory_order_acquire) >= buf.size()) return false;
        buf[w % buf.size()] = c;
        wr.store(w + 1, std::memory_order_release);
        return true;
    }
    bool pop(Change &c) {
        const unsigned r = rd.load(std::memory_order_relaxed);
        if (r == wr.load(std::memory_order_acquire)) return false;
        c = buf[r % buf.size()];
        rd.store(r + 1, std::memory_order_release);
        return true;
    }
private:
    std::array<Change, 1024> buf{};
    std::atomic<unsigned> wr{0}, rd{0};
};

class PluginInstance {
public:
    virtual ~PluginInstance() = default;
    // Audio thread: process `n` mono samples in place (n <= maxBlock given at creation).
    virtual void process(float *io, int n) = 0;

    // UI thread.
    int paramCount() const { return int(params.size()); }
    const PluginParam &param(int i) const { return params[size_t(i)]; }
    float value(int i) const { return values[size_t(i)].load(std::memory_order_relaxed); }
    void setValue(int i, float v);              // clamps, stores, queues for the audio thread
    virtual QString valueText(int i, float v) const; // human-readable value
    int indexOf(const QString &paramId) const;

    PluginInfo info;
    std::atomic<bool> bypass{false};
protected:
    virtual void onValueSet(int, float) {} // e.g. keep a VST3 edit controller in sync
public:
    int latency = 0; // samples, if the plugin reports it

protected:
    std::vector<PluginParam> params;
    std::unique_ptr<std::atomic<float>[]> values;
    ParamQueue queue;
    void initValues();
};

// Discovery. LV2 reads metadata only (safe). VST3 loads each bundle in a separate
// "micforge --scan-vst3 <bundle>" process so a crashing plugin can't take MicForge down.
bool lv2Available(QString *why = nullptr); // false if the optional lilv library isn't installed
std::vector<PluginInfo> scanLv2();
std::vector<QString> vst3Bundles();
std::vector<PluginInfo> scanVst3Bundle(const QString &bundlePath); // used by the --scan-vst3 helper

// Creation (UI thread). Returns nullptr and fills `error` on failure.
std::unique_ptr<PluginInstance> createPlugin(const PluginInfo &info, double sampleRate, int maxBlock, QString *error);

// The chain's Plugins stage: up to kSlots instances, processed in order on the audio thread.
class PluginRack {
public:
    static constexpr int kSlots = 8;
    std::array<std::atomic<PluginInstance *>, kSlots> slot{};
    std::atomic<bool> busy{false}; // true while the audio thread is inside process()

    static constexpr int kMaxBlock = 2048; // plugins are created with this block size

    // Audio thread.
    void process(float *io, int n) {
        busy.store(true, std::memory_order_release);
        for (int off = 0; off < n; off += kMaxBlock) {
            const int len = std::min(kMaxBlock, n - off);
            for (auto &s : slot) {
                PluginInstance *p = s.load(std::memory_order_acquire);
                if (p && !p->bypass.load(std::memory_order_relaxed)) p->process(io + off, len);
            }
        }
        busy.store(false, std::memory_order_release);
    }
    static void hook(void *ctx, float *io, int n) { static_cast<PluginRack *>(ctx)->process(io, n); }
    // UI thread: wait until the audio thread has finished any block that might use an old instance.
    void waitIdle() const;
};

} // namespace mf
