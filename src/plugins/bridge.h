// Out-of-process plugin bridge: the audio block exchange between MicForge's audio thread and a
// "micforge --plugin-host" helper process. Linux: POSIX shared memory + futexes (real-time safe: no
// allocation, no locks, one syscall to wake and one bounded wait). Control messages (parameters,
// editor, errors) go over the helper's stdin/stdout instead (see pluginhost.*). No Qt.
#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace mf::bridge {

constexpr uint32_t kMagic = 0x4d464252; // "MFBR"
constexpr uint32_t kVersion = 1;
constexpr int kMaxBlock = 2048;

enum State : uint32_t { Idle = 0, Request = 1, Done = 2 };

struct Shared {
    uint32_t magic, version;
    std::atomic<uint32_t> state;      // Idle -> Request (host) -> Done (helper) -> Idle (host)
    std::atomic<uint32_t> helperAlive; // the helper bumps this every loop (hang detection)
    std::atomic<int32_t> latency;     // samples, reported by the plugin
    std::atomic<uint32_t> quit;       // host asks the helper's audio loop to stop
    int32_t frames;
    float audio[kMaxBlock];           // processed in place
};
static_assert(std::atomic<uint32_t>::is_always_lock_free, "futexes need plain 32-bit atomics");

// Futex helpers on a shared (non-private) 32-bit word.
void wake(std::atomic<uint32_t> &word);
// Wait while word == expected, at most `timeoutNs`. Returns false on timeout.
bool waitWhile(std::atomic<uint32_t> &word, uint32_t expected, int64_t timeoutNs);

// Shared memory segment (created by the host, opened by the helper). Unlinked by the creator's
// destructor; the mapping stays valid until both sides unmap it.
class Segment {
public:
    static std::string uniqueName();                // "/micforge-bridge-<pid>-<n>"
    bool create(const std::string &name);
    bool open(const std::string &name);
    ~Segment();
    Shared *get() const { return m_shared; }
    const std::string &name() const { return m_name; }

private:
    Shared *m_shared = nullptr;
    std::string m_name;
    bool m_owner = false;
};

// Host side, audio thread: hand one block to the helper and wait for it, up to `deadlineNs`.
// Returns true if `io` now holds the processed audio; false = missed or helper busy (io untouched).
bool processBlock(Shared &s, float *io, int n, int64_t deadlineNs);

// Helper side: wait for the next request (or quit); returns false when asked to quit.
bool waitForRequest(Shared &s, int64_t pollNs);
void finishRequest(Shared &s);

} // namespace mf::bridge
