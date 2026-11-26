// Plugin bridge: correctness across processes, round-trip time, and the deadline fallback.
#include "plugins/bridge.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace mf::bridge;

static int failures = 0;
static void check(bool ok, const char *what, double v) {
    std::printf("[%s] %-60s %10.2f\n", ok ? " OK " : "FAIL", what, v);
    if (!ok) ++failures;
}

int main() {
    const std::string name = Segment::uniqueName();
    Segment host;
    if (!host.create(name)) { std::puts("can't create shared memory"); return 1; }
    Shared &s = *host.get();

    const pid_t child = fork();
    if (child == 0) { // the "helper": gain x0.5, but every 50th block it's late by 20 ms
        Segment seg;
        if (!seg.open(name)) _exit(2);
        int count = 0;
        while (waitForRequest(*seg.get(), 50'000'000)) {
            Shared &c = *seg.get();
            if (++count % 50 == 0) usleep(20000);
            for (int i = 0; i < c.frames; ++i) c.audio[i] *= 0.5f;
            finishRequest(c);
        }
        _exit(0);
    }

    std::vector<float> buf(256);
    std::vector<double> rtt;
    int ok = 0, missed = 0, wrong = 0;
    for (int b = 0; b < 600; ++b) {
        for (int i = 0; i < 256; ++i) buf[size_t(i)] = float(b % 7 + 1);
        const auto t0 = std::chrono::steady_clock::now();
        const bool done = processBlock(s, buf.data(), 256, 2'600'000); // half of a 5.3 ms block
        const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
        if (done) {
            ++ok; rtt.push_back(us);
            if (std::fabs(buf[0] - float(b % 7 + 1) * 0.5f) > 1e-6f) ++wrong;
        } else {
            ++missed;
            if (buf[0] != float(b % 7 + 1)) ++wrong; // a missed block must come back untouched (dry)
        }
        usleep(1000); // blocks arrive every few ms, like the audio thread
    }
    s.quit = 1;
    s.state = Request;
    wake(s.state);
    int status = 0;
    waitpid(child, &status, 0);

    std::sort(rtt.begin(), rtt.end());
    const double median = rtt[rtt.size() / 2], p99 = rtt[size_t(double(rtt.size()) * 0.99)];
    check(wrong == 0, "every block is either processed or passed through dry, never mixed", wrong);
    // 12 stalls of 20 ms; a block arrives every ~1 ms here, so each stall costs about 10-25 blocks
    // (at a real 5.3 ms block size, about 4). Everything else must be processed.
    const double perStall = missed / 12.0;
    check(perStall >= 5 && perStall <= 25, "a 20 ms helper stall: blocks passed dry per stall", perStall);
    check(ok + missed == 600 && ok > 300, "all other blocks processed by the helper", ok);
    check(median < 300, "round trip to the helper, median (us)", median);
    std::printf("       round trip p99 %.1f us\n", p99);
    check(WIFEXITED(status) && WEXITSTATUS(status) == 0, "helper quits cleanly when asked", status);
    std::printf("\nbridge tests: %s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
