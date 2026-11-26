// Minimal portable run-time library loader (dlopen on Linux, LoadLibrary on Windows).
//
// Optional features (AI noise removal, LV2 hosting) are loaded at run time instead of being
// linked, so a missing library disables only that feature instead of stopping MicForge from
// starting at all. See docs/ARCHITECTURE.md, "Optional run-time dependencies".
#pragma once

#include <initializer_list>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace mf {

class DynLib {
public:
    DynLib() = default;
    DynLib(const DynLib &) = delete;
    DynLib &operator=(const DynLib &) = delete;
    // Libraries stay loaded for the life of the process (their functions may be in use by the
    // audio thread until shutdown), so there is deliberately no close().

    // Try each candidate file name in turn. Returns true on success; otherwise `error` explains why.
    bool open(std::initializer_list<const char *> names, std::string *error = nullptr) {
        std::string last;
        for (const char *n : names) {
#ifdef _WIN32
            handle = reinterpret_cast<void *>(LoadLibraryA(n));
            if (!handle) last = std::string(n) + ": not found";
#else
            handle = dlopen(n, RTLD_NOW | RTLD_LOCAL);
            if (!handle) last = dlerror();
#endif
            if (handle) return true;
        }
        if (error) *error = last;
        return false;
    }

    template <typename Fn> bool get(const char *name, Fn &out, std::string *missing = nullptr) {
#ifdef _WIN32
        out = reinterpret_cast<Fn>(GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
#else
        out = reinterpret_cast<Fn>(dlsym(handle, name));
#endif
        if (!out && missing) *missing = name;
        return out != nullptr;
    }

    bool isOpen() const { return handle != nullptr; }

private:
    void *handle = nullptr;
};

} // namespace mf
