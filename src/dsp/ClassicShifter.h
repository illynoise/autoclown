// Auto Clown — "Classic" engine: two-tap crossfading delay-line shifter.
//
// Two read heads sweep through a delay line centred on the plugin latency; each head's
// gain is sin^2 of its sweep phase, the heads are half a sweep apart so the gains sum to
// one. Formants move with pitch (no formant correction) and the head splices give it a
// slightly gritty, vintage character — deliberately different from the Modern engine.
//
// The sweep window is a whole number of detected periods ("pitch-synchronous"), which
// keeps splices phase-coherent on steady notes. The window length is only changed when
// a head is exactly at the centre of the line (where its delay doesn't depend on the
// window) and the other is silent, so changing it never clicks.
#pragma once

#include "DspUtils.h"

namespace ac
{
class ClassicShifter
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() noexcept { phase = 0.0; window = 0.0; }

    void process (int64_t n, int latency, const HistoryBuffer* const* in, int numChannels, float* out,
                  float period, float ratio) noexcept;

private:
    double targetWindow (int latency, float period) const noexcept;

    double fs = 44100.0;
    double phase = 0.0;  // head A phase in [0, 1)
    double window = 0.0; // sweep length in samples
};

} // namespace ac
