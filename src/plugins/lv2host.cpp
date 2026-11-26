// LV2 plugin hosting with lilv.
#include "plugins.h"

#include "optional/lilv_api.h" // lilv is loaded at run time (optional dependency)
#include <lv2/atom/atom.h>
#include <lv2/buf-size/buf-size.h>
#include <lv2/core/lv2.h>
#include <lv2/log/log.h>
#include <lv2/options/options.h>
#include <lv2/parameters/parameters.h>
#include <lv2/port-props/port-props.h>
#include <lv2/units/units.h>
#include <lv2/urid/urid.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace mf {
namespace {

// ---------------------------------------------------------------- shared world + URID map

LilvWorld *world() {
    static LilvWorld *w = [] {
        LilvWorld *nw = lilv_world_new();
        lilv_world_load_all(nw);
        return nw;
    }();
    return w;
}

struct UridMap {
    std::mutex m;
    std::unordered_map<std::string, LV2_URID> ids;
    std::vector<std::string> uris{""};
    LV2_URID map(const char *uri) {
        std::lock_guard<std::mutex> l(m);
        auto it = ids.find(uri);
        if (it != ids.end()) return it->second;
        uris.emplace_back(uri);
        const LV2_URID id = LV2_URID(uris.size() - 1);
        ids.emplace(uri, id);
        return id;
    }
    const char *unmap(LV2_URID id) {
        std::lock_guard<std::mutex> l(m);
        return id < uris.size() ? uris[id].c_str() : nullptr;
    }
};
UridMap &uridMap() { static UridMap u; return u; }
LV2_URID mapUri(LV2_URID_Map_Handle, const char *uri) { return uridMap().map(uri); }
const char *unmapUri(LV2_URID_Unmap_Handle, LV2_URID id) { return uridMap().unmap(id); }

int logVPrintf(LV2_Log_Handle, LV2_URID, const char *fmt, va_list ap) {
    std::fprintf(stderr, "[lv2] ");
    return std::vfprintf(stderr, fmt, ap);
}
int logPrintf(LV2_Log_Handle h, LV2_URID t, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    const int r = logVPrintf(h, t, fmt, ap);
    va_end(ap);
    return r;
}

struct Node {
    LilvNode *n;
    explicit Node(const char *uri) : n(lilv_new_uri(world(), uri)) {}
    Node(const Node &) = delete;            // owning wrapper: never copy (e.g. into C varargs)
    Node &operator=(const Node &) = delete;
    ~Node() { lilv_node_free(n); }
    operator const LilvNode *() const { return n; }
};

const LilvPlugin *findPlugin(const QString &uri) {
    LilvNode *u = lilv_new_uri(world(), uri.toUtf8().constData());
    const LilvPlugin *p = lilv_plugins_get_by_uri(lilv_world_get_all_plugins(world()), u);
    lilv_node_free(u);
    return p;
}

// ---------------------------------------------------------------- instance

class Lv2Instance final : public PluginInstance {
public:
    ~Lv2Instance() override {
        if (inst) {
            lilv_instance_deactivate(inst);
            lilv_instance_free(inst);
        }
    }

    bool create(const LilvPlugin *plug, double fs, int maxBlock, QString *error) {
        this->maxBlock = maxBlock;
        Node audio(LV2_CORE__AudioPort), control(LV2_CORE__ControlPort), input(LV2_CORE__InputPort),
            atomPort(LV2_ATOM__AtomPort), toggled(LV2_CORE__toggled), integer(LV2_CORE__integer),
            enumeration(LV2_CORE__enumeration), logarithmic(LV2_PORT_PROPS__logarithmic),
            notOnGui(LV2_PORT_PROPS__notOnGUI), unitSym(LV2_UNITS__symbol), unitProp(LV2_UNITS__unit);

        // Refuse plugins that need host features we don't provide (rather than crashing later).
        static const char *supported[] = {LV2_URID__map, LV2_URID__unmap, LV2_OPTIONS__options,
                                          LV2_BUF_SIZE__boundedBlockLength, LV2_LOG__log, nullptr};
        LilvNodes *req = lilv_plugin_get_required_features(plug);
        LILV_FOREACH (nodes, i, req) {
            const char *f = lilv_node_as_uri(lilv_nodes_get(req, i));
            bool ok = false;
            for (int k = 0; supported[k]; ++k) ok |= std::strcmp(supported[k], f) == 0;
            if (!ok) {
                if (error) *error = QString("needs a host feature MicForge doesn't support yet (%1)").arg(f);
                lilv_nodes_free(req);
                return false;
            }
        }
        lilv_nodes_free(req);

        const uint32_t nPorts = lilv_plugin_get_num_ports(plug);
        portBuf.assign(nPorts, 0.0f);
        std::vector<float> mins(nPorts), maxs(nPorts), defs(nPorts);
        lilv_plugin_get_port_ranges_float(plug, mins.data(), maxs.data(), defs.data());

        for (uint32_t i = 0; i < nPorts; ++i) {
            const LilvPort *port = lilv_plugin_get_port_by_index(plug, i);
            const bool isIn = lilv_port_is_a(plug, port, input);
            if (lilv_port_is_a(plug, port, audio)) {
                (isIn ? audioIn : audioOut).push_back({i, std::vector<float>(size_t(maxBlock), 0.0f)});
            } else if (lilv_port_is_a(plug, port, control) && isIn) {
                PluginParam p;
                p.id = lilv_node_as_string(lilv_port_get_symbol(plug, port));
                LilvNode *nm = lilv_port_get_name(plug, port);
                p.name = nm ? QString::fromUtf8(lilv_node_as_string(nm)) : p.id;
                lilv_node_free(nm);
                p.min = std::isnan(mins[i]) ? 0.0f : mins[i];
                p.max = std::isnan(maxs[i]) ? 1.0f : maxs[i];
                p.def = std::isnan(defs[i]) ? p.min : defs[i];
                p.toggle = lilv_port_has_property(plug, port, toggled);
                p.integer = lilv_port_has_property(plug, port, integer);
                p.logScale = lilv_port_has_property(plug, port, logarithmic) && p.min > 0;
                if (lilv_port_has_property(plug, port, enumeration)) {
                    LilvScalePoints *sp = lilv_port_get_scale_points(plug, port);
                    LILV_FOREACH (scale_points, k, sp) {
                        const LilvScalePoint *pt = lilv_scale_points_get(sp, k);
                        p.choices << QString::fromUtf8(lilv_node_as_string(lilv_scale_point_get_label(pt)));
                        p.choiceValues.push_back(lilv_node_as_float(lilv_scale_point_get_value(pt)));
                    }
                    lilv_scale_points_free(sp);
                }
                LilvNodes *units = lilv_port_get_value(plug, port, unitProp);
                if (units && lilv_nodes_size(units) > 0) {
                    LilvNodes *sym = lilv_world_find_nodes(world(), lilv_nodes_get_first(units), unitSym, nullptr);
                    if (sym && lilv_nodes_size(sym) > 0) p.unit = QString::fromUtf8(lilv_node_as_string(lilv_nodes_get_first(sym)));
                    lilv_nodes_free(sym);
                }
                lilv_nodes_free(units);
                if (lilv_port_has_property(plug, port, notOnGui)) p.name.prepend("· ");
                portBuf[i] = p.def;
                paramPorts.push_back(i);
                params.push_back(p);
            } else if (lilv_port_is_a(plug, port, atomPort)) {
                atoms.push_back({i, isIn, std::vector<uint64_t>(2048, 0)}); // 16 KiB, 8-byte aligned
            } else if (lilv_port_is_a(plug, port, control)) {
                // control output: portBuf[i] is enough
            } else {
                other.push_back({i, std::vector<float>(size_t(maxBlock), 0.0f)}); // CV etc.
            }
        }
        if (audioIn.empty() || audioOut.empty()) {
            if (error) *error = "has no audio input or output";
            return false;
        }
        initValues();

        // Features.
        urid.handle = nullptr; urid.map = mapUri;
        unmap.handle = nullptr; unmap.unmap = unmapUri;
        log.handle = nullptr; log.printf = logPrintf; log.vprintf = logVPrintf;
        blockLen = maxBlock;
        sampleRate = float(fs);
        const LV2_URID tInt = mapUri(nullptr, LV2_ATOM__Int), tFloat = mapUri(nullptr, LV2_ATOM__Float);
        options[0] = {LV2_OPTIONS_INSTANCE, 0, mapUri(nullptr, LV2_BUF_SIZE__maxBlockLength), sizeof(int32_t), tInt, &blockLen};
        options[1] = {LV2_OPTIONS_INSTANCE, 0, mapUri(nullptr, LV2_BUF_SIZE__minBlockLength), sizeof(int32_t), tInt, &zero};
        options[2] = {LV2_OPTIONS_INSTANCE, 0, mapUri(nullptr, LV2_BUF_SIZE__nominalBlockLength), sizeof(int32_t), tInt, &blockLen};
        options[3] = {LV2_OPTIONS_INSTANCE, 0, mapUri(nullptr, LV2_PARAMETERS__sampleRate), sizeof(float), tFloat, &sampleRate};
        options[4] = {LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, nullptr};
        fUrid = {LV2_URID__map, &urid};
        fUnmap = {LV2_URID__unmap, &unmap};
        fOptions = {LV2_OPTIONS__options, options};
        fBounded = {LV2_BUF_SIZE__boundedBlockLength, nullptr};
        fLog = {LV2_LOG__log, &log};
        features = {&fUrid, &fUnmap, &fOptions, &fBounded, &fLog, nullptr};

        inst = lilv_plugin_instantiate(plug, fs, features.data());
        if (!inst) {
            if (error) *error = "failed to start";
            return false;
        }
        for (uint32_t i = 0; i < nPorts; ++i) lilv_instance_connect_port(inst, i, &portBuf[i]);
        for (auto &a : audioIn) lilv_instance_connect_port(inst, a.port, a.buf.data());
        for (auto &a : audioOut) lilv_instance_connect_port(inst, a.port, a.buf.data());
        for (auto &a : other) lilv_instance_connect_port(inst, a.port, a.buf.data());
        for (auto &a : atoms) lilv_instance_connect_port(inst, a.port, a.buf.data());
        seqType = mapUri(nullptr, LV2_ATOM__Sequence);
        chunkType = mapUri(nullptr, LV2_ATOM__Chunk);
        resetAtoms();
        lilv_instance_activate(inst);
        return true;
    }

    void process(float *io, int n) override {
        ParamQueue::Change c;
        while (queue.pop(c)) portBuf[paramPorts[size_t(c.index)]] = c.value;
        resetAtoms();
        for (auto &a : audioIn) std::memcpy(a.buf.data(), io, size_t(n) * sizeof(float));
        lilv_instance_run(inst, uint32_t(n));
        const float k = 1.0f / float(audioOut.size()); // stereo plugins: fold back to mono
        for (int s = 0; s < n; ++s) {
            float sum = 0;
            for (auto &a : audioOut) sum += a.buf[size_t(s)];
            io[s] = sum * k;
        }
    }

private:
    void resetAtoms() {
        for (auto &a : atoms) {
            auto *atom = reinterpret_cast<LV2_Atom *>(a.buf.data());
            if (a.input) { // empty sequence
                auto *seq = reinterpret_cast<LV2_Atom_Sequence *>(a.buf.data());
                seq->atom.size = sizeof(LV2_Atom_Sequence_Body);
                seq->atom.type = seqType;
                seq->body.unit = 0;
                seq->body.pad = 0;
            } else { // space for the plugin to write into
                atom->size = uint32_t(a.buf.size() * sizeof(uint64_t) - sizeof(LV2_Atom));
                atom->type = chunkType;
            }
        }
    }

    struct Buf { uint32_t port; std::vector<float> buf; };
    struct AtomBuf { uint32_t port; bool input; std::vector<uint64_t> buf; };
    LilvInstance *inst = nullptr;
    int maxBlock = 0;
    std::vector<float> portBuf;
    std::vector<uint32_t> paramPorts;
    std::vector<Buf> audioIn, audioOut, other;
    std::vector<AtomBuf> atoms;
    LV2_URID seqType = 0, chunkType = 0;

    LV2_URID_Map urid{};
    LV2_URID_Unmap unmap{};
    LV2_Log_Log log{};
    int32_t blockLen = 0, zero = 0;
    float sampleRate = 48000;
    LV2_Options_Option options[5]{};
    LV2_Feature fUrid{}, fUnmap{}, fOptions{}, fBounded{}, fLog{};
    std::array<const LV2_Feature *, 6> features{};
};

} // namespace

bool lv2Available(QString *why) {
    std::string err;
    const bool ok = lilvapi::load(&err);
    if (!ok && why) *why = QString::fromStdString(err);
    return ok;
}

std::vector<PluginInfo> scanLv2() {
    std::vector<PluginInfo> out;
    if (!lv2Available(nullptr)) return out;
    Node audio(LV2_CORE__AudioPort), input(LV2_CORE__InputPort), output(LV2_CORE__OutputPort);
    const LilvPlugins *all = lilv_world_get_all_plugins(world());
    LILV_FOREACH (plugins, i, all) {
        const LilvPlugin *p = lilv_plugins_get(all, i);
        if (lilv_plugin_get_num_ports_of_class(p, audio.n, input.n, nullptr) < 1 ||
            lilv_plugin_get_num_ports_of_class(p, audio.n, output.n, nullptr) < 1)
            continue; // instruments, analysers without output, etc.
        PluginInfo info;
        info.format = "LV2";
        info.id = QString::fromUtf8(lilv_node_as_uri(lilv_plugin_get_uri(p)));
        LilvNode *nm = lilv_plugin_get_name(p);
        info.name = nm ? QString::fromUtf8(lilv_node_as_string(nm)) : info.id;
        lilv_node_free(nm);
        LilvNode *au = lilv_plugin_get_author_name(p);
        info.vendor = au ? QString::fromUtf8(lilv_node_as_string(au)) : QString();
        lilv_node_free(au);
        const LilvNode *cls = lilv_plugin_class_get_label(lilv_plugin_get_class(p));
        info.category = cls ? QString::fromUtf8(lilv_node_as_string(cls)) : QString();
        out.push_back(info);
    }
    return out;
}

std::unique_ptr<PluginInstance> createLv2(const PluginInfo &info, double fs, int maxBlock, QString *error) {
    QString why;
    if (!lv2Available(&why)) {
        if (error) *error = "can't be loaded because " + why;
        return nullptr;
    }
    const LilvPlugin *p = findPlugin(info.id);
    if (!p) {
        if (error) *error = "is not installed any more";
        return nullptr;
    }
    auto inst = std::make_unique<Lv2Instance>();
    inst->info = info;
    if (!inst->create(p, fs, maxBlock, error)) return nullptr;
    return inst;
}

} // namespace mf
