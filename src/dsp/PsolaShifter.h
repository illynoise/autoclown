// Auto Clown — "Modern" engine: pitch-synchronous overlap-add (TD-PSOLA).
//
// Why PSOLA: for a monophonic voice it is the cheapest shifter that keeps the spectral
// envelope (formants) intact — each grain is one glottal period windowed by a Hann of
// two periods, and pitch changes only by re-spacing the grains. No FFT, no phase
// vocoder smearing, and the cost is O(1) per output sample per overlapping grain.
//
// Analysis marks: placed one detected period apart and snapped (±20 % of a period) to
// the maximum of a low-passed copy of the input, so marks stay locked to the same
// phase of the glottal cycle. Marks keep running through unvoiced audio (with the last
// period) so the engine never has to "restart" at a voicing onset.
//
// Synthesis: synthesis marks advance by T_analysis / ratio. When the output clock reaches
// the start of the next synthesis grain we take the closest *complete* analysis grain
// (its right half must already be in the input buffer). With the high-quality latency
// (>= 2 periods) that is the time-aligned mark; in low-latency mode it is simply the
// newest complete one, which bounds the time misalignment to under two periods instead
// of adding latency.
//
// Formant control: a grain may be resampled by f = ratio^(1 - amount). amount = 1 keeps
// formants exactly (classic PSOLA, f = 1); amount = 0 (or formant correction off) uses
// f = ratio, which moves formants with the pitch like a varispeed.
#pragma once

#include "DspUtils.h"

namespace ac
{
class PsolaShifter
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare (double sampleRate, int maxPeriodSamples);
    void reset (int64_t now) noexcept;

    /**
        Call once per input sample, after the sample at absolute index `n` has been written
        to the history buffers. Writes one output sample per channel for time n - latency.
    */
    void process (int64_t n, int latency,
                  const HistoryBuffer& markSignal, const HistoryBuffer* const* in, int numChannels,
                  float* out, float period, bool voiced, float ratio, float formantFactor) noexcept;

private:
    struct Mark { int64_t pos = 0; float period = 0.0f; };

    void placeMarks (int64_t n, const HistoryBuffer& markSignal, float period, bool voiced) noexcept;
    void triggerGrain (int64_t n, int64_t t, const HistoryBuffer* const* in, int numChannels,
                       float ratio, float formantFactor) noexcept;

    double fs = 44100.0;
    int maxPeriod = 1470;

    std::array<Mark, 128> marks {};
    int markHead = 0, markCount = 0;
    double nextMark = 0.0;

    double nextSynth = 0.0;
    std::array<HistoryBuffer, kMaxChannels> ola;
    bool started = false;
};

} // namespace ac
