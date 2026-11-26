#include "mictest.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <cmath>

using namespace mf;

MicTestSession::MicTestSession(AudioCore &core, QObject *parent) : QObject(parent), m_core(core) {
    m_timer.setInterval(50);
    connect(&m_timer, &QTimer::timeout, this, &MicTestSession::tick);
}

void MicTestSession::setSeconds(int s) {
    s = std::clamp(s, 3, 30);
    if (s == m_seconds) return;
    m_seconds = s;
    emit secondsChanged();
}

void MicTestSession::setMatchLevels(bool on) {
    if (on == m_match) return;
    m_match = on;
    emit secondsChanged();
}

void MicTestSession::setState(const QString &s, int playing) {
    m_state = s;
    m_playing = playing;
    emit stateChanged();
}

void MicTestSession::record() {
    if (m_state == "recording") return;
    if (m_state == "playing") stop();
    m_core.prepareTest(size_t(m_seconds) * AudioCore::kRate);
    m_clipsAtStart = m_core.stats.inputClips.load();
    m_core.startTestRecording();
    m_progress = 0;
    setState("recording");
    m_timer.start();
}

void MicTestSession::stop() {
    if (m_state == "recording") {
        m_core.stopTestRecording();
        finishRecording();
    } else if (m_state == "playing") {
        m_core.stopPlayback();
        m_timer.stop();
        if (onNeedMonitorStream) onNeedMonitorStream(false);
        m_progress = 0;
        emit progressChanged();
        setState(m_frames ? "ready" : "idle");
    }
}

void MicTestSession::play(int which) {
    if (m_frames == 0 || m_state == "recording" || (which != 1 && which != 2)) return;
    float gain = 1.0f;
    // Level-matched comparison: play the raw take at the processed take's loudness, so "louder"
    // doesn't get mistaken for "better".
    if (m_match && which == 1 && m_raw.valid && m_proc.valid)
        gain = std::pow(10.0f, std::clamp(m_proc.activeRmsDb - m_raw.activeRmsDb, -30.0f, 30.0f) / 20.0f);
    if (onNeedMonitorStream) onNeedMonitorStream(true);
    m_core.startPlayback(which, gain);
    m_progress = 0;
    setState("playing", which);
    m_timer.start();
}

void MicTestSession::discard() {
    stop();
    m_core.prepareTest(0);
    m_frames = 0;
    m_rawWave.clear();
    m_procWave.clear();
    m_analysis.clear();
    emit takeChanged();
    setState("idle");
}

void MicTestSession::tick() {
    MicTest &t = m_core.test;
    if (m_state == "recording") {
        m_progress = double(t.pos.load()) / double(std::max<size_t>(1, t.raw.size()));
        emit progressChanged();
        if (!t.recording.load()) finishRecording();
    } else if (m_state == "playing") {
        m_progress = double(t.playPos.load()) / double(std::max<size_t>(1, t.playLen));
        emit progressChanged();
        if (t.playing.load() == 0) stop();
    }
}

void MicTestSession::finishRecording() {
    m_timer.stop();
    MicTest &t = m_core.test;
    m_frames = std::min(t.pos.load(), t.raw.size());
    m_clipsDuring = m_core.stats.inputClips.load() - m_clipsAtStart;
    m_raw = analyseLevels(t.raw.data(), m_frames, AudioCore::kRate);
    m_proc = analyseLevels(t.processed.data(), m_frames, AudioCore::kRate);
    auto wave = [&](const std::vector<float> &x) {
        QVariantList out;
        for (float v : peakEnvelope(x.data(), m_frames, 480)) out << v;
        return out;
    };
    m_rawWave = wave(t.raw);
    m_procWave = wave(t.processed);

    auto side = [](const LevelStats &s) {
        return QVariantMap{{"valid", s.valid}, {"peak", s.peakDb}, {"speech", s.speechDb}, {"floor", s.noiseFloorDb},
                           {"active", s.activeRmsDb}, {"range", s.dynamicRangeDb}, {"clipped", qulonglong(s.clipped)}};
    };
    QVariantMap a;
    a["raw"] = side(m_raw);
    a["processed"] = side(m_proc);
    a["inputClips"] = qulonglong(m_clipsDuring);
    a["matchGain"] = m_raw.valid && m_proc.valid ? double(m_proc.activeRmsDb - m_raw.activeRmsDb) : 0.0;

    // Plain-English findings.
    QStringList notes;
    if (!m_raw.valid) notes << tr("The recording is too short to analyse.");
    else {
        if (m_clipsDuring > 0)
            notes << tr("Your microphone clipped (distorted) %n time(s). Turn down the gain on your interface or mic, "
                        "then run the test again.", nullptr, int(std::min<uint64_t>(m_clipsDuring, 1u << 30)));
        else if (m_raw.peakDb > -3)
            notes << tr("Your loudest moments came close to clipping. A little less gain gives you safety room.");
        if (m_raw.speechDb < -45)
            notes << tr("Your voice is very quiet. Turn up the gain on your interface, or move closer to the mic.");
        if (m_raw.dynamicRangeDb < 20)
            notes << tr("There's little difference between your voice and the background (%1 dB). "
                        "Did you speak during the test? A closer mic position or Noise Removal helps.")
                         .arg(qRound(m_raw.dynamicRangeDb));
        else if (m_proc.valid && m_proc.noiseFloorDb < -90.0f)
            notes << tr("Processing silences the background between your words completely (it was %1 dB below your voice).")
                         .arg(qRound(m_raw.dynamicRangeDb));
        else if (m_proc.valid && m_proc.dynamicRangeDb > m_raw.dynamicRangeDb + 6.0f)
            notes << tr("Processing widens the gap between your voice and the background from %1 dB to %2 dB.")
                         .arg(qRound(m_raw.dynamicRangeDb)).arg(qRound(m_proc.dynamicRangeDb));
        if (m_proc.valid && m_proc.peakDb > -0.5)
            notes << tr("The processed output reaches full scale: lower Output gain a little.");
        if (notes.isEmpty()) notes << tr("Levels look healthy.");
    }
    a["notes"] = notes;
    m_analysis = a;
    m_progress = 0;
    emit progressChanged();
    emit takeChanged();
    setState(m_frames ? "ready" : "idle");
}

void MicTestSession::loadDemoTake() {
    const size_t n = size_t(m_seconds) * AudioCore::kRate;
    m_core.prepareTest(n);
    MicTest &t = m_core.test;
    unsigned seed = 7;
    for (size_t i = 0; i < n; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const float noise = (float(seed >> 8) / float(1u << 24) - 0.5f) * 0.02f;
        const double sec = double(i) / AudioCore::kRate;
        const bool talk = std::fmod(sec, 1.6) < 1.0;
        const float env = talk ? float(0.5 + 0.5 * std::sin(sec * 9.0)) : 0.0f;
        const float voice = env * 0.25f * float(std::sin(2 * M_PI * 180 * sec) + 0.4 * std::sin(2 * M_PI * 540 * sec));
        t.raw[i] = voice + noise;
        t.processed[i] = talk ? 1.6f * voice + 0.1f * noise : 0.0005f * noise;
    }
    t.pos.store(n);
    m_clipsAtStart = m_core.stats.inputClips.load();
    finishRecording();
}

namespace {
bool writeWav(const QString &path, const std::vector<float> &x, size_t n, int rate) {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    auto u32 = [&](uint32_t v) { char b[4] = {char(v), char(v >> 8), char(v >> 16), char(v >> 24)}; f.write(b, 4); };
    auto u16 = [&](uint16_t v) { char b[2] = {char(v), char(v >> 8)}; f.write(b, 2); };
    const uint32_t bytes = uint32_t(n * 2);
    f.write("RIFF", 4); u32(36 + bytes); f.write("WAVE", 4);
    f.write("fmt ", 4); u32(16); u16(1); u16(1); u32(uint32_t(rate)); u32(uint32_t(rate) * 2); u16(2); u16(16);
    f.write("data", 4); u32(bytes);
    QByteArray pcm(int(bytes), Qt::Uninitialized);
    for (size_t i = 0; i < n; ++i) {
        const int v = int(std::lround(std::clamp(x[i], -1.0f, 1.0f) * 32767.0f));
        pcm[int(2 * i)] = char(v & 0xff);
        pcm[int(2 * i + 1)] = char((v >> 8) & 0xff);
    }
    return f.write(pcm) == pcm.size();
}
} // namespace

QString MicTestSession::exportWav(const QUrl &folder) {
    if (m_frames == 0) return tr("Record a test first.");
    const QString dir = folder.isLocalFile() ? folder.toLocalFile() : folder.toString();
    if (!QDir(dir).exists()) return tr("That folder doesn't exist.");
    const QString stem = dir + "/micforge-test-" + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
    const bool ok = writeWav(stem + "-raw.wav", m_core.test.raw, m_frames, AudioCore::kRate)
                 && writeWav(stem + "-processed.wav", m_core.test.processed, m_frames, AudioCore::kRate);
    return ok ? tr("Saved %1-raw.wav and -processed.wav").arg(QFileInfo(stem).fileName())
              : tr("Couldn't write the files to %1").arg(dir);
}
