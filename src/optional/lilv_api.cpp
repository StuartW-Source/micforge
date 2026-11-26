#define MF_LILV_NO_REDIRECT // this file defines the pointers, so it must see lilv's real names
#include "optional/lilv_api.h"

#include "platform/dynlib.h"

#include <mutex>

namespace mf::lilvapi {

#define MF_LILV_DEFINE(fn) decltype(&::fn) fn = nullptr;
MF_LILV_FUNCTIONS(MF_LILV_DEFINE)
#undef MF_LILV_DEFINE

bool load(std::string *error) {
    static DynLib lib;
    static bool ok = false;
    static std::string err;
    static std::once_flag once;
    std::call_once(once, [] {
#ifdef _WIN32
        const auto names = {"lilv-0.dll", "liblilv-0.dll"};
#else
        const auto names = {"liblilv-0.so.0", "liblilv-0.so"};
#endif
        std::string why;
        if (!lib.open(names, &why)) {
            err = "the lilv library isn't installed (" + why + ")";
            return;
        }
        std::string missing;
#define MF_LILV_RESOLVE(fn) if (!lib.get(#fn, fn, &missing)) { err = "the installed lilv library is missing " + missing; return; }
        MF_LILV_FUNCTIONS(MF_LILV_RESOLVE)
#undef MF_LILV_RESOLVE
        ok = true;
    });
    if (!ok && error) *error = err;
    return ok;
}

} // namespace mf::lilvapi
