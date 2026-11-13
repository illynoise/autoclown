#include "ClassicShifter.h"

namespace ac
{
double ClassicShifter::targetWindow (int latency, float period) const noexcept
{
    const double maxW = std::min (0.030 * fs, 2.0 * latency - 8.0);
    const double T = std::max (16.0, (double) period);
    if (maxW < T) return std::max (16.0, maxW);
    return T * std::floor (maxW / T);
}

void ClassicShifter::process (int64_t n, int latency, const HistoryBuffer* const* in, int numChannels,
                              float* out, float period, float ratio) noexcept
{
    if (window <= 0.0) window = targetWindow (latency, period);

    const double prev = phase;
    phase += ((double) ratio - 1.0) / window;
    const bool wrapped = phase >= 1.0 || phase < 0.0;
    phase -= std::floor (phase);

    // Head A crossing its centre (0.5) or wrapping (head B at centre): safe to resize.
    const bool crossedCentre = (prev < 0.5) != (phase < 0.5) && ! wrapped;
    if (wrapped || crossedCentre) window = targetWindow (latency, period);

    const double phaseB = phase + 0.5 - std::floor (phase + 0.5);
    const double dA = latency + window * (0.5 - phase);
    const double dB = latency + window * (0.5 - phaseB);
    const float gA = std::sin (kPi * (float) phase);
    const float gB = std::sin (kPi * (float) phaseB);

    for (int ch = 0; ch < numChannels; ++ch)
        out[ch] = gA * gA * in[ch]->readHermite ((double) n - dA)
                + gB * gB * in[ch]->readHermite ((double) n - dB);
}

} // namespace ac
