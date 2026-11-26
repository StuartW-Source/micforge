#include "rnnoise_api.h"

#include "platform/dynlib.h"

#include <mutex>

namespace mf::rnnoise {
namespace {

// Signatures from rnnoise.h (declared here so building MicForge doesn't require rnnoise headers).
struct RNNModel;
using CreateFn = DenoiseState *(*)(RNNModel *);
using DestroyFn = void (*)(DenoiseState *);
using ProcessFn = float (*)(DenoiseState *, float *, const float *);

struct Api {
    DynLib lib;
    CreateFn create = nullptr;
    DestroyFn destroy = nullptr;
    ProcessFn process = nullptr;
    bool ok = false;
    std::string error;
};

Api &api() {
    static Api a;
    static std::once_flag once;
    std::call_once(once, [] {
#ifdef _WIN32
        const auto names = {"rnnoise.dll", "librnnoise.dll"};
#else
        const auto names = {"librnnoise.so.0", "librnnoise.so"};
#endif
        std::string err, missing;
        if (!a.lib.open(names, &err)) {
            a.error = "the rnnoise library isn't installed (" + err + ")";
            return;
        }
        if (!a.lib.get("rnnoise_create", a.create, &missing) || !a.lib.get("rnnoise_destroy", a.destroy, &missing) ||
            !a.lib.get("rnnoise_process_frame", a.process, &missing)) {
            a.error = "the installed rnnoise library is missing " + missing;
            return;
        }
        a.ok = true;
    });
    return a;
}

} // namespace

bool available() { return api().ok; }
const std::string &error() { return api().error; }
DenoiseState *create() { return api().ok ? api().create(nullptr) : nullptr; }
void destroy(DenoiseState *st) { if (st && api().ok) api().destroy(st); }
float processFrame(DenoiseState *st, float *out, const float *in) { return api().process(st, out, in); }

} // namespace mf::rnnoise
