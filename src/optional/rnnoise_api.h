// RNNoise (AI noise removal), loaded at run time. If librnnoise isn't installed, MicForge still
// starts and the "AI Noise Removal" effect reports itself as unavailable.
#pragma once

#include <string>

struct DenoiseState; // opaque, owned by librnnoise

namespace mf::rnnoise {

bool available();              // tries to load the library on first call (thread-safe)
const std::string &error();    // why it isn't available (empty when it is)

// Valid only when available() is true.
DenoiseState *create();
void destroy(DenoiseState *st);
float processFrame(DenoiseState *st, float *out, const float *in); // 480 samples, returns voice probability
constexpr int kFrameSize = 480;

} // namespace mf::rnnoise
