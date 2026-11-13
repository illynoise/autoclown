#include "PsolaShifter.h"

namespace ac
{
void PsolaShifter::prepare (double sampleRate, int maxPeriodSamples)
{
    fs = sampleRate;
    maxPeriod = maxPeriodSamples;
    // Grain half-length can reach maxPeriod / fMin (fMin = 0.5) ahead of the output clock.
    for (auto& o : ola) o.allocate (8 * maxPeriod + 1024);
    reset (0);
}

void PsolaShifter::reset (int64_t now) noexcept
{
    for (auto& o : ola) o.clear();
    markHead = markCount = 0;
    nextMark = (double) now;
    nextSynth = 0.0;
    started = false;
}

void PsolaShifter::placeMarks (int64_t n, const HistoryBuffer& sig, float period, bool voiced) noexcept
{
    const double T = std::clamp ((double) period, 16.0, (double) maxPeriod);
    const auto look = (int64_t) std::ceil (0.2 * T);

    // Never fall hopelessly behind (e.g. after a long period followed by a short one).
    if ((double) n - nextMark > 4.0 * T) nextMark = (double) n - 2.0 * T;

    while ((double) n >= nextMark + (double) look)
    {
        auto pos = (int64_t) std::llround (nextMark);
        if (voiced)
        {
            const int64_t lo = pos - look, hi = std::min (pos + look, n);
            float best = -1.0e30f;
            for (int64_t i = lo; i <= hi; ++i)
            {
                const float v = sig.read (i);
                if (v > best) { best = v; pos = i; }
            }
        }

        if (markCount > 0)
        {
            const Mark& last = marks[(size_t) ((markHead + 127) & 127)];
            const auto minGap = (int64_t) (0.5 * T);
            if (pos - last.pos < minGap) pos = last.pos + minGap;
        }

        marks[(size_t) markHead] = { pos, (float) T };
        markHead = (markHead + 1) & 127;
        markCount = std::min (markCount + 1, 128);
        nextMark = (double) pos + T;
    }
}

void PsolaShifter::triggerGrain (int64_t n, int64_t t, const HistoryBuffer* const* in, int numChannels,
                                 float ratio, float f) noexcept
{
    // Pick the complete analysis mark closest to the synthesis position.
    const Mark* best = nullptr;
    double bestDist = 1.0e30;
    for (int k = 1; k <= markCount; ++k)
    {
        const Mark& m = marks[(size_t) ((markHead - k + 128) & 127)];
        if ((double) m.pos + m.period + 2.0 > (double) n) continue; // right half not available yet
        const double d = std::abs ((double) m.pos - nextSynth);
        if (d < bestDist) { bestDist = d; best = &m; }
        else if ((double) m.pos < nextSynth) break; // marks are time-ordered
    }

    if (best == nullptr)
    {
        nextSynth += (markCount > 0 ? marks[(size_t) ((markHead + 127) & 127)].period : 256.0f) / ratio;
        return;
    }

    const double Ta = best->period;
    const double hs = Ta / f;                       // output half-length of this grain
    const float gain = std::clamp (std::sqrt (f / ratio), 0.5f, 2.0f);
    const auto first = std::max ((int64_t) std::ceil (nextSynth - hs), t);
    const auto last = (int64_t) std::floor (nextSynth + hs);
    const double invHs = 1.0 / hs;

    for (int64_t pos = first; pos <= last; ++pos)
    {
        const double k = (double) pos - nextSynth;
        const float w = gain * 0.5f * (1.0f + std::cos (kPi * (float) (k * invHs)));
        const double src = (double) best->pos + k * f;
        for (int ch = 0; ch < numChannels; ++ch)
            ola[(size_t) ch].add (pos, w * in[ch]->readFrac (src));
    }

    nextSynth += Ta / ratio;

    // Phase lock: with no pitch change, nudge the synthesis grid onto the analysis marks
    // so the wet path lines up sample-accurately with the dry path (no comb filtering at
    // Mix < 100 % or during voiced/unvoiced crossfades). Only when the aligned mark is
    // available (HQ latency, or short periods in low-latency mode).
    if (std::abs (ratio - 1.0f) < 0.002f && bestDist < 0.5 * Ta)
        nextSynth += 0.15 * ((double) best->pos - (nextSynth - Ta / ratio));
}

void PsolaShifter::process (int64_t n, int latency, const HistoryBuffer& markSignal,
                            const HistoryBuffer* const* in, int numChannels, float* out,
                            float period, bool voiced, float ratio, float formantFactor) noexcept
{
    placeMarks (n, markSignal, period, voiced);

    const int64_t t = n - latency;
    ratio = std::clamp (ratio, 0.25f, 4.0f);
    const float f = std::clamp (formantFactor, 0.5f, 2.0f);

    if (! started) { nextSynth = (double) t; started = true; }
    // If the synthesis clock fell behind (e.g. after a reset), resynchronise.
    if (nextSynth < (double) t - 4.0 * maxPeriod) nextSynth = (double) t;

    for (int guard = 0; guard < 16; ++guard)
    {
        const double hsEst = std::clamp ((double) period, 16.0, (double) maxPeriod) / f;
        if ((double) t < nextSynth - hsEst) break;
        triggerGrain (n, t, in, numChannels, ratio, f);
    }

    for (int ch = 0; ch < numChannels; ++ch)
        out[ch] = ola[(size_t) ch].take (t);
}

} // namespace ac
