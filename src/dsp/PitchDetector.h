// Auto Clown — monophonic pitch detector.
//
// DSP choice: YIN (de Cheveigné & Kawahara 2002) run in two stages.
//   1. Coarse: the signal is high-passed (DC/rumble), low-passed (4th-order Butterworth)
//      and decimated to ~11 kHz. YIN's cumulative-mean-normalised difference function
//      (CMNDF) is evaluated there; its cost is O(W * tauMax) with W, tauMax < ~400,
//      which is cheap and fully deterministic (no FFT, no allocation).
//   2. Fine: the coarse lag is refined at the full sample rate by evaluating the
//      plain difference function for a handful of lags around it, followed by
//      parabolic interpolation -> sub-cent accuracy at any sample rate.
//
// Octave-error handling:
//   * YIN's "first dip below threshold" rule already prevents most octave-low errors.
//   * Sub-harmonic check: if a dip at tau/2 or tau/3 is nearly as deep, prefer it.
//   * Continuity guard: an octave jump relative to the previous voiced frame is only
//     accepted when it persists for several analysis hops, unless the old-octave lag
//     no longer shows a periodic dip.
//
// Voiced/unvoiced: a frame is voiced only if (a) the CMNDF dip is below the tracking
// threshold (with hysteresis), (b) the level is above a gate and (c) enough of the
// energy sits below ~2 kHz (rejects fricatives such as "s"/"sh", which are noisy but
// can produce spurious periodicity).
//
// Latency: the detector is causal and adds no audio latency; it reports the pitch of the
// most recent window, updated every ~2.5 ms.
#pragma once

#include "DspUtils.h"

namespace ac
{
struct PitchEstimate
{
    float hz = 0.0f;            // refined fundamental (valid when > 0, even if unvoiced)
    float aperiodicity = 1.0f;  // CMNDF value at the chosen lag (0 = perfectly periodic)
    float rmsDb = -120.0f;      // level of the analysis window (low band)
    float lowBandRatio = 0.0f;  // energy below ~2 kHz relative to full band
    bool voiced = false;
    int64_t sampleTime = 0;     // absolute input sample index of the newest analysed sample
};

class PitchDetector
{
public:
    static constexpr float kAbsMinHz = 30.0f;
    static constexpr float kAbsMaxHz = 2000.0f;

    /** Allocates all buffers. Not real-time safe. */
    void prepare (double sampleRate);
    void reset() noexcept;

    /** Real-time safe. Range is clamped to [kAbsMinHz, kAbsMaxHz]. */
    void setRange (float minHz, float maxHz) noexcept;
    /** 0 = strict (clean input), 1 = relaxed (noisy / breathy input). */
    void setTracking (float tracking01) noexcept;

    /** Feed one input sample. Returns true when a new estimate is available. */
    bool pushSample (float x) noexcept;
    const PitchEstimate& getEstimate() const noexcept { return estimate; }

    int getHopSize() const noexcept { return hopDec * decim; }
    double getSampleRate() const noexcept { return sampleRate; }
    float getThreshold() const noexcept { return threshold; }

private:
    void configureWindow() noexcept;
    void analyse() noexcept;
    float refineFullRate (float coarseLagFull) noexcept;
    float dipDepth (int tau) const noexcept; // parabola-interpolated CMNDF minimum
    int localMinNear (float lag, float tolerance, int tauMin, int tauMax) const noexcept;

    double sampleRate = 44100.0, decRate = 11025.0;
    int decim = 4, decPhase = 0;

    Biquad hp, lp1, lp2;
    float fullPow = 0.0f, lowPow = 0.0f, powCoeff = 0.999f;

    HistoryBuffer fullRing, decRing;
    int64_t fullCount = 0, decCount = 0;

    std::vector<float> scratch, diff, cmnd;
    int maxTauDec = 0;

    // configuration
    float minHz = 80.0f, maxHz = 1000.0f;
    float threshold = 0.15f, gateDb = -50.0f, minLowRatio = 0.2f;
    int tauMinDec = 2, tauMaxDec = 100, windowDec = 100, hopDec = 28, hopCounter = 0;

    // octave continuity
    float lastVoicedHz = 0.0f;
    int octaveJumpFrames = 0, unvoicedFrames = 0;

    PitchEstimate estimate;
};

} // namespace ac
