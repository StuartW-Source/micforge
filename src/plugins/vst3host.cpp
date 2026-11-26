// VST3 plugin hosting with Steinberg's (MIT-licensed) VST3 SDK hosting helpers.
#include "plugins.h"

#include "pluginterfaces/gui/iplugview.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/vstspeaker.h"
#include "public.sdk/source/vst/hosting/hostclasses.h"
#include "public.sdk/source/vst/hosting/module.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"
#include "public.sdk/source/vst/hosting/plugprovider.h"
#include "public.sdk/source/vst/hosting/processdata.h"
#include "public.sdk/source/vst/utility/stringconvert.h"

#include <QSocketNotifier>
#include <QTimer>

#include <cstdio>
#include <map>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace mf {
namespace {

void ensureHostContext() {
    static HostApplication *host = [] {
        auto *h = new HostApplication();
        PluginContextFactory::instance().setPluginContext(h);
        return h;
    }();
    (void)host;
}

QString fromString128(const String128 s) { return QString::fromStdU16String(std::u16string(reinterpret_cast<const char16_t *>(s))); }

// Minimal reference counting for host-side VST3 interfaces.
#define MF_FUNKNOWN_REFCOUNT                                                     \
    std::atomic<int32> refs{1};                                                  \
    uint32 PLUGIN_API addRef() override { return uint32(++refs); }               \
    uint32 PLUGIN_API release() override { return uint32(--refs); } // owned by the instance

// Receives edits made in the plugin's own window.
class EditHandler final : public IComponentHandler {
public:
    std::function<void(ParamID, ParamValue)> edit;
    tresult PLUGIN_API beginEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API performEdit(ParamID id, ParamValue v) override { if (edit) edit(id, v); return kResultOk; }
    tresult PLUGIN_API endEdit(ParamID) override { return kResultOk; }
    tresult PLUGIN_API restartComponent(int32) override { return kResultOk; }
    tresult PLUGIN_API queryInterface(const TUID iid, void **obj) override {
        if (FUnknownPrivate::iidEqual(iid, IComponentHandler::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
            *obj = static_cast<IComponentHandler *>(this); addRef(); return kResultOk;
        }
        *obj = nullptr; return kNoInterface;
    }
    MF_FUNKNOWN_REFCOUNT
};

// The editor's frame: resize requests, and the Linux run loop (file descriptors + timers) on Qt's loop.
class EditorFrame final : public IPlugFrame, public Linux::IRunLoop {
public:
    std::function<void(int, int)> resize;
    ~EditorFrame() { for (auto &[h, n] : fds) delete n; for (auto &[h, t] : timers) delete t; }
    tresult PLUGIN_API resizeView(IPlugView *view, ViewRect *r) override {
        if (!r) return kInvalidArgument;
        if (resize) resize(r->getWidth(), r->getHeight());
        if (view) view->onSize(r);
        return kResultOk;
    }
    tresult PLUGIN_API registerEventHandler(Linux::IEventHandler *h, Linux::FileDescriptor fd) override {
        auto *n = new QSocketNotifier(fd, QSocketNotifier::Read);
        QObject::connect(n, &QSocketNotifier::activated, [h, fd] { h->onFDIsSet(fd); });
        fds.insert({h, n});
        return kResultOk;
    }
    tresult PLUGIN_API unregisterEventHandler(Linux::IEventHandler *h) override {
        auto range = fds.equal_range(h);
        for (auto it = range.first; it != range.second; ++it) delete it->second;
        fds.erase(h);
        return kResultOk;
    }
    tresult PLUGIN_API registerTimer(Linux::ITimerHandler *h, Linux::TimerInterval ms) override {
        auto *t = new QTimer();
        QObject::connect(t, &QTimer::timeout, [h] { h->onTimer(); });
        t->start(int(ms));
        timers.insert({h, t});
        return kResultOk;
    }
    tresult PLUGIN_API unregisterTimer(Linux::ITimerHandler *h) override {
        auto range = timers.equal_range(h);
        for (auto it = range.first; it != range.second; ++it) delete it->second;
        timers.erase(h);
        return kResultOk;
    }
    tresult PLUGIN_API queryInterface(const TUID iid, void **obj) override {
        if (FUnknownPrivate::iidEqual(iid, IPlugFrame::iid) || FUnknownPrivate::iidEqual(iid, FUnknown::iid)) {
            *obj = static_cast<IPlugFrame *>(this); addRef(); return kResultOk;
        }
        if (FUnknownPrivate::iidEqual(iid, Linux::IRunLoop::iid)) {
            *obj = static_cast<Linux::IRunLoop *>(this); addRef(); return kResultOk;
        }
        *obj = nullptr; return kNoInterface;
    }
    MF_FUNKNOWN_REFCOUNT
private:
    std::multimap<Linux::IEventHandler *, QSocketNotifier *> fds;
    std::multimap<Linux::ITimerHandler *, QTimer *> timers;
};

class Vst3Instance final : public PluginInstance {
public:
    ~Vst3Instance() override {
        closeEditor();
        if (controller) controller->setComponentHandler(nullptr);
        if (processor && processing) processor->setProcessing(false);
        if (component && active) component->setActive(false);
        data.unprepare();
        processor = nullptr;
        controller = nullptr;
        component = nullptr;
        provider = nullptr; // releases the plugin
        module = nullptr;   // unloads the library last
    }

    bool create(const PluginInfo &info, double fs, int maxBlock, QString *error) {
        ensureHostContext();
        std::string err;
        module = VST3::Hosting::Module::create(info.path.toStdString(), err);
        if (!module) {
            if (error) *error = "couldn't be loaded: " + QString::fromStdString(err);
            return false;
        }
        const auto factory = module->getFactory();
        for (const auto &ci : factory.classInfos()) {
            if (ci.category() != kVstAudioEffectClass || QString::fromStdString(ci.ID().toString()) != info.id) continue;
            provider = owned(new PlugProvider(factory, ci, true));
            break;
        }
        if (!provider || !provider->initialize()) {
            if (error) *error = "couldn't be started";
            return false;
        }
        component = provider->getComponentPtr();
        controller = provider->getControllerPtr();
        processor = FUnknownPtr<IAudioProcessor>(component);
        if (!component || !processor) {
            if (error) *error = "isn't an audio effect";
            return false;
        }
        if (component->getBusCount(kAudio, kInput) < 1 || component->getBusCount(kAudio, kOutput) < 1) {
            if (error) *error = "has no audio input or output";
            return false;
        }
        if (processor->canProcessSampleSize(kSample32) != kResultTrue) {
            if (error) *error = "doesn't support 32-bit processing";
            return false;
        }

        // Ask for mono in/out; fall back to stereo; otherwise accept what the plugin wants.
        auto trySetup = [&](SpeakerArrangement arr) {
            std::vector<SpeakerArrangement> ins(size_t(component->getBusCount(kAudio, kInput)), SpeakerArr::kEmpty);
            std::vector<SpeakerArrangement> outs(size_t(component->getBusCount(kAudio, kOutput)), SpeakerArr::kEmpty);
            ins[0] = outs[0] = arr;
            return processor->setBusArrangements(ins.data(), int32(ins.size()), outs.data(), int32(outs.size())) == kResultTrue;
        };
        if (!trySetup(SpeakerArr::kMono)) trySetup(SpeakerArr::kStereo);
        SpeakerArrangement inArr = 0, outArr = 0;
        processor->getBusArrangement(kInput, 0, inArr);
        processor->getBusArrangement(kOutput, 0, outArr);
        inChannels = SpeakerArr::getChannelCount(inArr);
        outChannels = SpeakerArr::getChannelCount(outArr);
        if (inChannels < 1 || outChannels < 1) {
            if (error) *error = "uses an unsupported channel layout";
            return false;
        }
        component->activateBus(kAudio, kInput, 0, true);
        component->activateBus(kAudio, kOutput, 0, true);

        ProcessSetup setup{kRealtime, kSample32, maxBlock, fs};
        if (processor->setupProcessing(setup) != kResultOk) {
            if (error) *error = "rejected the sample rate / block size";
            return false;
        }
        if (component->setActive(true) != kResultOk) {
            if (error) *error = "couldn't be activated";
            return false;
        }
        active = true;
        if (!data.prepare(*component, maxBlock, kSample32)) {
            if (error) *error = "couldn't allocate audio buffers";
            return false;
        }
        data.processContext = nullptr;

        // Parameters (normalised 0..1 in VST3).
        if (controller) {
            const int32 n = controller->getParameterCount();
            for (int32 i = 0; i < n; ++i) {
                ParameterInfo pi{};
                if (controller->getParameterInfo(i, pi) != kResultOk) continue;
                if (pi.flags & ParameterInfo::kIsReadOnly) continue;
                PluginParam p;
                p.id = QString::number(pi.id);
                p.name = fromString128(pi.title);
                p.unit = fromString128(pi.units);
                p.min = 0;
                p.max = 1;
                p.def = float(pi.defaultNormalizedValue);
                p.toggle = pi.stepCount == 1 || (pi.flags & ParameterInfo::kIsBypass);
                if (pi.stepCount > 1 && pi.stepCount <= 24) {
                    for (int32 k = 0; k <= pi.stepCount; ++k) {
                        const float v = float(k) / float(pi.stepCount);
                        String128 s{};
                        controller->getParamStringByValue(pi.id, v, s);
                        p.choices << fromString128(s);
                        p.choiceValues.push_back(v);
                    }
                }
                ids.push_back(pi.id);
                params.push_back(p);
            }
        }
        initValues();
        inChanges.setMaxParameters(int32(ids.size()) + 1);
        if (controller) {
            // Edits made in the plugin's own window: send them to the processor and tell MicForge.
            handler.edit = [this](ParamID id, ParamValue v) {
                for (size_t i = 0; i < ids.size(); ++i)
                    if (ids[i] == id) {
                        values[i].store(float(v), std::memory_order_relaxed);
                        queue.push({int(i), float(v)});
                        if (onEditorChange) onEditorChange(int(i), float(v));
                        break;
                    }
            };
            controller->setComponentHandler(&handler);
        }
        latency = int(processor->getLatencySamples());
        if (processor->setProcessing(true) == kResultOk) processing = true;
        this->maxBlock = maxBlock;
        return true;
    }

    void process(float *io, int n) override {
        inChanges.clearQueue();
        ParamQueue::Change c;
        while (queue.pop(c)) {
            int32 qi = 0, pi = 0;
            if (auto *q = inChanges.addParameterData(ids[size_t(c.index)], qi)) q->addPoint(0, c.value, pi);
        }
        for (int ch = 0; ch < inChannels; ++ch) std::memcpy(data.inputs[0].channelBuffers32[ch], io, size_t(n) * sizeof(float));
        data.numSamples = n;
        data.inputParameterChanges = &inChanges;
        if (processor->process(data) != kResultOk) return; // leave the audio untouched on failure
        const float k = 1.0f / float(outChannels);
        for (int s = 0; s < n; ++s) {
            float sum = 0;
            for (int ch = 0; ch < outChannels; ++ch) sum += data.outputs[0].channelBuffers32[ch][s];
            io[s] = sum * k;
        }
    }

    QString valueText(int i, float v) const override {
        if (controller && i >= 0 && size_t(i) < ids.size()) {
            String128 s{};
            if (controller->getParamStringByValue(ids[size_t(i)], v, s) == kResultOk) {
                const QString t = fromString128(s).trimmed();
                if (!t.isEmpty()) return params[size_t(i)].unit.isEmpty() ? t : t + " " + params[size_t(i)].unit;
            }
        }
        return PluginInstance::valueText(i, v);
    }

    bool hasEditor() override {
        if (!controller) return false;
        IPtr<IPlugView> v = owned(controller->createView(ViewType::kEditor));
        return v && v->isPlatformTypeSupported(kPlatformTypeX11EmbedWindowID) == kResultTrue;
    }

    bool openEditor(uintptr_t parent, std::function<void(int, int)> resize) override {
        if (!controller || view) return bool(view);
        view = owned(controller->createView(ViewType::kEditor));
        if (!view || view->isPlatformTypeSupported(kPlatformTypeX11EmbedWindowID) != kResultTrue) { view = nullptr; return false; }
        frame.resize = std::move(resize);
        view->setFrame(&frame);
        if (view->attached(reinterpret_cast<void *>(parent), kPlatformTypeX11EmbedWindowID) != kResultOk) {
            view->setFrame(nullptr);
            view = nullptr;
            return false;
        }
        ViewRect r{};
        if (view->getSize(&r) == kResultOk && frame.resize) frame.resize(r.getWidth(), r.getHeight());
        return true;
    }

    void closeEditor() override {
        if (!view) return;
        view->removed();
        view->setFrame(nullptr);
        view = nullptr;
    }

protected:
    void onValueSet(int i, float v) override {
        if (controller) controller->setParamNormalized(ids[size_t(i)], v);
    }

private:
    EditHandler handler;
    EditorFrame frame;
    IPtr<IPlugView> view;
    VST3::Hosting::Module::Ptr module;
    IPtr<PlugProvider> provider;
    IPtr<IComponent> component;
    IPtr<IEditController> controller;
    IPtr<IAudioProcessor> processor;
    HostProcessData data;
    ParameterChanges inChanges;
    std::vector<ParamID> ids;
    int inChannels = 0, outChannels = 0, maxBlock = 0;
    bool active = false, processing = false;
};

} // namespace

std::vector<PluginInfo> scanVst3Bundle(const QString &bundlePath) {
    std::vector<PluginInfo> out;
    std::string err;
    auto module = VST3::Hosting::Module::create(bundlePath.toStdString(), err);
    if (!module) {
        if (qEnvironmentVariableIsSet("MICFORGE_DEBUG")) std::fprintf(stderr, "VST3 load failed: %s\n", err.c_str());
        return out;
    }
    for (const auto &ci : module->getFactory().classInfos()) {
        if (qEnvironmentVariableIsSet("MICFORGE_DEBUG"))
            std::fprintf(stderr, "VST3 class: '%s' category '%s'\n", ci.name().c_str(), ci.category().c_str());
        if (ci.category() != kVstAudioEffectClass) continue;
        const QString subs = QString::fromStdString(ci.subCategoriesString());
        if (subs.contains("Instrument")) continue;
        PluginInfo info;
        info.format = "VST3";
        info.id = QString::fromStdString(ci.ID().toString());
        info.path = bundlePath;
        info.name = QString::fromStdString(ci.name());
        info.vendor = QString::fromStdString(ci.vendor());
        info.category = subs;
        out.push_back(info);
    }
    return out;
}

std::unique_ptr<PluginInstance> createVst3(const PluginInfo &info, double fs, int maxBlock, QString *error) {
    auto inst = std::make_unique<Vst3Instance>();
    inst->info = info;
    if (!inst->create(info, fs, maxBlock, error)) return nullptr;
    return inst;
}

std::unique_ptr<PluginInstance> createLv2(const PluginInfo &info, double fs, int maxBlock, QString *error);

std::unique_ptr<PluginInstance> createPlugin(const PluginInfo &info, double fs, int maxBlock, QString *error) {
    if (info.format == "LV2") return createLv2(info, fs, maxBlock, error);
    if (info.format == "VST3") return createVst3(info, fs, maxBlock, error);
    if (error) *error = "is in an unknown format";
    return nullptr;
}

} // namespace mf
