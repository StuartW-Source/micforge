#include "plugins/bridge.h"

#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace mf::bridge {

namespace {
long futex(std::atomic<uint32_t> &word, int op, uint32_t val, const timespec *ts) {
    return syscall(SYS_futex, reinterpret_cast<uint32_t *>(&word), op, val, ts, nullptr, 0);
}
int64_t nowNs() {
    timespec t{};
    clock_gettime(CLOCK_MONOTONIC, &t);
    return int64_t(t.tv_sec) * 1000000000LL + t.tv_nsec;
}
} // namespace

void wake(std::atomic<uint32_t> &word) { futex(word, FUTEX_WAKE, 1, nullptr); }

bool waitWhile(std::atomic<uint32_t> &word, uint32_t expected, int64_t timeoutNs) {
    const int64_t end = nowNs() + timeoutNs;
    while (word.load(std::memory_order_acquire) == expected) {
        const int64_t left = end - nowNs();
        if (left <= 0) return false;
        timespec ts{time_t(left / 1000000000LL), long(left % 1000000000LL)};
        futex(word, FUTEX_WAIT, expected, &ts); // relative timeout; spurious wake-ups just loop
    }
    return true;
}

std::string Segment::uniqueName() {
    static std::atomic<int> n{0};
    return "/micforge-bridge-" + std::to_string(getpid()) + "-" + std::to_string(n.fetch_add(1));
}

bool Segment::create(const std::string &name) {
    const int fd = shm_open(name.c_str(), O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) return false;
    if (ftruncate(fd, sizeof(Shared)) != 0) { close(fd); shm_unlink(name.c_str()); return false; }
    void *p = mmap(nullptr, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) { shm_unlink(name.c_str()); return false; }
    std::memset(p, 0, sizeof(Shared));
    m_shared = static_cast<Shared *>(p);
    m_shared->magic = kMagic;
    m_shared->version = kVersion;
    m_name = name;
    m_owner = true;
    return true;
}

bool Segment::open(const std::string &name) {
    const int fd = shm_open(name.c_str(), O_RDWR, 0600);
    if (fd < 0) return false;
    void *p = mmap(nullptr, sizeof(Shared), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (p == MAP_FAILED) return false;
    auto *s = static_cast<Shared *>(p);
    if (s->magic != kMagic || s->version != kVersion) { munmap(p, sizeof(Shared)); return false; }
    m_shared = s;
    m_name = name;
    return true;
}

Segment::~Segment() {
    if (m_shared) munmap(m_shared, sizeof(Shared));
    if (m_owner) shm_unlink(m_name.c_str());
}

bool processBlock(Shared &s, float *io, int n, int64_t deadlineNs) {
    // A previous block is still being processed (the helper was late): don't wait again, pass dry.
    uint32_t st = s.state.load(std::memory_order_acquire);
    if (st == Done) { s.state.store(Idle, std::memory_order_release); st = Idle; } // a late result: drop it
    if (st != Idle || n > kMaxBlock) return false;
    std::memcpy(s.audio, io, size_t(n) * sizeof(float));
    s.frames = n;
    s.state.store(Request, std::memory_order_release);
    wake(s.state);
    if (!waitWhile(s.state, Request, deadlineNs)) return false; // missed: the helper finishes it later
    std::memcpy(io, s.audio, size_t(n) * sizeof(float));
    s.state.store(Idle, std::memory_order_release);
    return true;
}

bool waitForRequest(Shared &s, int64_t pollNs) {
    for (;;) {
        if (s.quit.load(std::memory_order_acquire)) return false;
        s.helperAlive.fetch_add(1, std::memory_order_relaxed);
        if (s.state.load(std::memory_order_acquire) == Request) return true;
        // Sleep until the state changes (or poll again so quit and the heartbeat are seen).
        const uint32_t cur = s.state.load(std::memory_order_acquire);
        if (cur != Request) waitWhile(s.state, cur, pollNs);
    }
}

void finishRequest(Shared &s) {
    s.state.store(Done, std::memory_order_release);
    wake(s.state);
}

} // namespace mf::bridge
