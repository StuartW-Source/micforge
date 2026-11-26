// AudioCore tests: engine statistics, clip detection, mic-test recording and playback routing.
// Build: see docs/TESTING.md (needs Qt6Core headers only because the plugin rack uses QString).
#include "audio/audiocore.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>
#include <vector>

using namespace mf;

static int failures = 0;
static void check(bool ok, const char *what, double val) {
    std::printf("[%s] %-62s %10.2f\n", ok ? " OK " : "FAIL", what, val);
    if (!ok) ++failures;
}

static std::vector<float> stereoTone(float amp, int frames, float hz = 440) {
    std::vector<float> v(size_t(frames) * 2);
    for (int i = 0; i < frames; ++i) v[2 * i] = v[2 * i + 1] = amp * std::sin(2 * 3.14159265f * hz * float(i) / 48000);
    return v;
}

int main() {
    { // Statistics: blocks, block size, processing time.
        AudioCore c;
        auto in = stereoTone(0.1f, 256);
        for (int i = 0; i < 10; ++i) c.processInput(in.data(), 2, 256);
        check(c.stats.blocks == 10 && c.stats.blockFrames == 256, "stats: blocks and block size counted", double(c.stats.blocks));
        check(c.stats.procUsAvg > 0 && c.stats.procUsMax >= c.stats.procUsAvg * 0.5f, "stats: processing time measured (µs)", c.stats.procUsAvg);
        check(c.stats.lateCallbacks == 0, "stats: back-to-back blocks are not 'late'", double(c.stats.lateCallbacks));
        std::this_thread::sleep_for(std::chrono::milliseconds(30)); // ~6 blocks of 5.3 ms missing
        c.processInput(in.data(), 2, 256);
        check(c.stats.lateCallbacks == 1, "stats: a 30 ms gap counts as one late block", double(c.stats.lateCallbacks));
        c.resetChain();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        c.processInput(in.data(), 2, 256);
        check(c.stats.lateCallbacks == 1, "stats: no false alarm after a stream restart", double(c.stats.lateCallbacks));
    }
    { // Clipping is detected before input gain, on the selected channel only.
        AudioCore c;
        c.params.set(InputGain, -20);
        auto in = stereoTone(0.2f, 480);
        for (int i = 0; i < 480; i += 48) in[2 * i + 1] = 1.0f;          // right channel clips 10 times
        c.processInput(in.data(), 2, 480);
        check(c.stats.inputClips == 0, "clip: other channel's clipping ignored (input 1 selected)", double(c.stats.inputClips));
        c.params.set(InputChannel, 1);
        c.processInput(in.data(), 2, 480);
        check(c.stats.inputClips == 10, "clip: counted on the selected channel, despite -20 dB gain", double(c.stats.inputClips));
    }
    { // Mic test: raw = channel + gain only; processed = chain output; both the same length.
        AudioCore c;
        c.params.set(InputGain, 6);
        c.params.set(Bypass, 1); // processed == raw when bypassed, so the two takes must match exactly
        c.prepareTest(4800);
        c.startTestRecording();
        auto in = stereoTone(0.25f, 6000);
        c.processInput(in.data(), 2, 3000);
        c.processInput(in.data() + 6000, 2, 3000);
        check(!c.test.recording && c.test.pos == 4800, "mic test: stops by itself when full", double(c.test.pos));
        float maxDiff = 0, rawPk = 0;
        for (size_t i = 0; i < 4800; ++i) {
            maxDiff = std::max(maxDiff, std::fabs(c.test.raw[i] - c.test.processed[i]));
            rawPk = std::max(rawPk, std::fabs(c.test.raw[i]));
        }
        check(std::fabs(20 * std::log10(rawPk) - (20 * std::log10(0.25f) + 6)) < 0.1, "mic test: raw take has input gain applied (dBFS)", 20 * std::log10(rawPk));
        check(maxDiff < 1e-6f, "mic test: raw and processed takes are time-aligned", maxDiff);
    }
    { // Playback goes to the monitor only, never to the virtual mic, and the live monitor pauses meanwhile.
        AudioCore c;
        c.params.set(Bypass, 1);
        c.params.set(InputGain, 0); // the default input gain is +15 dB
        c.setMonitorEnabled(true);
        c.prepareTest(4800);
        c.startTestRecording();
        auto loud = stereoTone(0.5f, 4800);
        c.processInput(loud.data(), 2, 4800);
        c.startPlayback(1, 0.5f);
        auto quiet = std::vector<float>(2 * 1024, 0.0f);
        std::vector<float> vmic(256), mon(256);
        float vPk = 0, mPk = 0;
        for (int k = 0; k < 4; ++k) {
            c.processInput(quiet.data(), 2, 256);                         // live input is silent
            c.readVirtualMic(vmic.data(), 256);
            c.readMonitor(mon.data(), 256);
            for (int i = 0; i < 256; ++i) { vPk = std::max(vPk, std::fabs(vmic[i])); mPk = std::max(mPk, std::fabs(mon[i])); }
        }
        check(mPk > 0.24f && mPk < 0.26f, "playback: monitor plays the take at the chosen gain", mPk);
        check(vPk < 0.501f, "playback: virtual mic carries only live audio", vPk);
        bool vmicHasTake = false;
        // The virtual mic still holds the recorded (loud) block from before playback; after draining it,
        // only silence follows, proving playback is never routed there.
        for (int k = 0; k < 40; ++k) {
            c.processInput(quiet.data(), 2, 256);
            c.readVirtualMic(vmic.data(), 256);
            if (k > 30) for (float v : vmic) vmicHasTake |= std::fabs(v) > 1e-6f;
        }
        check(!vmicHasTake, "playback: nothing from the take reaches the virtual mic", 0);
        for (int k = 0; k < 40 && c.test.playing; ++k) c.readMonitor(mon.data(), 256);
        check(c.test.playing == 0, "playback: ends by itself at the end of the take", 0);
    }
    { // Virtual-mic underruns are counted only after audio has started flowing.
        AudioCore c;
        std::vector<float> out(256);
        c.readVirtualMic(out.data(), 256);
        check(c.stats.vmicUnderruns == 0, "underrun: not counted while filling up", double(c.stats.vmicUnderruns));
        auto in = stereoTone(0.1f, 512);
        c.processInput(in.data(), 2, 512);
        c.readVirtualMic(out.data(), 256);
        c.readVirtualMic(out.data(), 256);
        c.readVirtualMic(out.data(), 256);
        check(c.stats.vmicUnderruns == 1, "underrun: counted when an app reads faster than audio arrives", double(c.stats.vmicUnderruns));
    }
    std::printf("\naudiocore tests: %s (%d failure%s)\n", failures ? "FAILED" : "ALL PASSED", failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
