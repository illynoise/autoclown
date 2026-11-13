#include "PitchDetector.h"

namespace ac
{
void PitchDetector::prepare (double fs)
{
    sampleRate = fs;
    decim = std::max (1, (int) std::floor (fs / 11000.0));
    decRate = fs / decim;

    hp.setHighpass (fs, 20.0, 0.7071);
    const double fc = 0.2 * decRate; // ~2.2 kHz: keeps several harmonics, kills aliasing
    lp1.setLowpass (fs, fc, 0.5412);
    lp2.setLowpass (fs, fc, 1.3066);
    powCoeff = std::exp (-1.0f / (0.012f * (float) fs));

    maxTauDec = (int) std::ceil (decRate / kAbsMinHz) + 2;
    decRing.allocate (4 * maxTauDec + 64);
    const int maxLagFull = (int) std::ceil (fs / kAbsMinHz) + 4 * decim + 8;
    fullRing.allocate (2 * maxLagFull + 64);

    scratch.assign ((size_t) (3 * maxTauDec + 8), 0.0f);
    diff.assign ((size_t) (maxTauDec + 2), 0.0f);
    cmnd.assign ((size_t) (maxTauDec + 2), 1.0f);

    configureWindow();
    reset();
}

void PitchDetector::reset() noexcept
{
    hp.reset(); lp1.reset(); lp2.reset();
    fullRing.clear(); decRing.clear();
    fullCount = decCount = 0;
    decPhase = hopCounter = 0;
    fullPow = lowPow = 0.0f;
    lastVoicedHz = 0.0f;
    octaveJumpFrames = unvoicedFrames = 0;
    estimate = {};
}

void PitchDetector::setRange (float lo, float hi) noexcept
{
    lo = std::clamp (lo, kAbsMinHz, kAbsMaxHz * 0.5f);
    hi = std::clamp (hi, lo * 2.0f, kAbsMaxHz);
    if (lo == minHz && hi == maxHz) return;
    minHz = lo; maxHz = hi;
    configureWindow();
}

void PitchDetector::setTracking (float t) noexcept
{
    t = std::clamp (t, 0.0f, 1.0f);
    threshold   = 0.06f + 0.34f * std::pow (t, 1.3f); // CMNDF threshold
    gateDb      = -42.0f - 22.0f * t;                 // level gate
    minLowRatio = 0.28f - 0.18f * t;                  // fricative rejection
}

void PitchDetector::configureWindow() noexcept
{
    tauMaxDec = std::min (maxTauDec, (int) std::ceil (decRate / minHz) + 1);
    tauMinDec = std::max (2, (int) std::floor (decRate / maxHz));
    windowDec = std::max (tauMaxDec, (int) std::lround (0.008 * decRate));
    windowDec = std::min (windowDec, 2 * maxTauDec);
    hopDec = std::max ((int) std::lround (0.0025 * decRate), tauMaxDec / 8);
}

bool PitchDetector::pushSample (float x) noexcept
{
    const float h = hp.process (x);
    fullRing.write (fullCount++, h);
    const float l = lp2.process (lp1.process (h));

    fullPow = h * h + powCoeff * (fullPow - h * h);
    lowPow  = l * l + powCoeff * (lowPow - l * l);

    if (++decPhase < decim) return false;
    decPhase = 0;
    decRing.write (decCount++, l);

    if (++hopCounter < hopDec) return false;
    hopCounter = 0;
    analyse();
    return true;
}

float PitchDetector::dipDepth (int t) const noexcept
{
    if (t <= 1 || t >= tauMaxDec) return cmnd[(size_t) t];
    const float a = cmnd[(size_t) t - 1], b = cmnd[(size_t) t], c = cmnd[(size_t) t + 1];
    const float den = a - 2.0f * b + c;
    if (den <= 1.0e-9f) return b;
    return std::max (0.0f, b - (a - c) * (a - c) / (8.0f * den));
}

int PitchDetector::localMinNear (float lag, float tol, int tauMin, int tauMax) const noexcept
{
    const int lo = std::max (tauMin, (int) std::floor (lag * (1.0f - tol)));
    const int hi = std::min (tauMax, (int) std::ceil (lag * (1.0f + tol)));
    if (lo > hi) return -1;
    int best = lo;
    for (int t = lo + 1; t <= hi; ++t)
        if (cmnd[(size_t) t] < cmnd[(size_t) best]) best = t;
    return best;
}

void PitchDetector::analyse() noexcept
{
    const int W = windowDec, tauMax = tauMaxDec, tauMin = tauMinDec;
    const int64_t start = decCount - (W + tauMax);
    estimate.sampleTime = fullCount - 1;
    if (start < 0) { estimate.voiced = false; return; }

    float* s = scratch.data();
    for (int i = 0; i < W + tauMax; ++i)
        s[i] = decRing.read (start + i);

    // Level of the most recent W samples (low band).
    double e = 0.0;
    for (int i = tauMax; i < tauMax + W; ++i) e += (double) s[i] * s[i];
    const float rms = (float) std::sqrt (e / W);
    estimate.rmsDb = 20.0f * std::log10 (rms + 1.0e-7f);
    estimate.lowBandRatio = lowPow / (fullPow + 1.0e-12f);

    // YIN difference function + CMNDF.
    diff[0] = 0.0f; cmnd[0] = 1.0f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau)
    {
        float acc = 0.0f;
        const float* a = s;
        const float* b = s + tau;
        for (int j = 0; j < W; ++j) { const float d = a[j] - b[j]; acc += d * d; }
        diff[(size_t) tau] = acc;
        running += acc;
        cmnd[(size_t) tau] = running > 0.0 ? (float) ((double) acc * tau / running) : 1.0f;
    }

    // 1) first dip below threshold, followed down to its local minimum.
    int best = -1;
    for (int tau = tauMin; tau <= tauMax; ++tau)
    {
        if (cmnd[(size_t) tau] < threshold)
        {
            while (tau + 1 <= tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
            best = tau;
            break;
        }
    }
    // 2) otherwise global minimum (frame will most likely be unvoiced).
    if (best < 0)
    {
        best = tauMin;
        for (int tau = tauMin + 1; tau <= tauMax; ++tau)
            if (cmnd[(size_t) tau] < cmnd[(size_t) best]) best = tau;
    }

    // 3a) octave-down check (octave-HIGH errors, e.g. a weak fundamental under a strong
    //     2nd harmonic): a truly periodic tau has nearly equal dips at tau and 2*tau, so a
    //     much deeper dip at 2*tau means tau was only a harmonic.
    //     Depths are compared after parabolic interpolation: at very short lags an
    //     integer-lag sample can sit well off the true dip.
    if (best >= 10 && 2 * best + 1 <= tauMax) // below ~1.1 kHz; high notes have strong fundamentals
    {
        const float here = dipDepth (best);
        const int c = localMinNear (2.0f * (float) best, 0.04f, tauMin, tauMax);
        if (here > 0.03f && c > 0 && dipDepth (c) < 0.35f * here) best = c;
    }

    // 3b) sub-harmonic check (octave-low errors): prefer tau/k if nearly as periodic.
    for (int k = 3; k >= 2; --k)
    {
        const float cand = (float) best / (float) k;
        if (cand < (float) tauMin) continue;
        const int c = localMinNear (cand, 0.06f, tauMin, tauMax);
        if (c > 0 && cmnd[(size_t) c] < threshold * 1.5f && cmnd[(size_t) c] <= cmnd[(size_t) best] * 1.15f + 0.02f
            && ! (dipDepth (best) < 0.35f * dipDepth (c)))
        {
            best = c;
            break;
        }
    }

    // 4) octave continuity guard.
    if (lastVoicedHz > 0.0f)
    {
        const float newHz = (float) decRate / (float) best;
        const float r = newHz / lastVoicedHz;
        const bool octaveJump = std::abs (r - 2.0f) < 0.16f || std::abs (r - 0.5f) < 0.04f
                             || std::abs (r - 3.0f) < 0.24f || std::abs (r - 1.0f / 3.0f) < 0.027f;
        if (octaveJump)
        {
            const int prev = localMinNear ((float) decRate / lastVoicedHz, 0.05f, tauMin, tauMax);
            if (prev > 0 && cmnd[(size_t) prev] < threshold * 1.6f && ++octaveJumpFrames < 4)
                best = prev;
            else
                octaveJumpFrames = 0;
        }
        else
        {
            octaveJumpFrames = 0;
        }
    }

    // Parabolic interpolation on the CMNDF.
    float lag = (float) best;
    if (best > 1 && best < tauMax)
    {
        const float a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
        const float den = a - 2.0f * b + c;
        if (std::abs (den) > 1.0e-9f) lag += std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f);
    }

    const float aper = std::clamp (cmnd[(size_t) best], 0.0f, 1.0f);
    const float fullLag = refineFullRate (lag * (float) decim);
    const float hz = (float) sampleRate / fullLag;

    const bool wasVoiced = estimate.voiced;
    const float thr = wasVoiced ? threshold * 1.3f : threshold;
    const bool periodic = aper < thr;
    const bool loud = estimate.rmsDb > gateDb;
    const bool tonal = estimate.lowBandRatio > minLowRatio;
    const bool inRange = hz >= minHz * 0.97f && hz <= maxHz * 1.03f;

    estimate.hz = hz;
    estimate.aperiodicity = aper;
    estimate.voiced = periodic && loud && tonal && inRange;

    if (estimate.voiced)
    {
        lastVoicedHz = hz;
        unvoicedFrames = 0;
    }
    else if (++unvoicedFrames > 20) // ~50 ms of silence: forget octave history
    {
        lastVoicedHz = 0.0f;
    }
}

float PitchDetector::refineFullRate (float coarse) noexcept
{
    if (decim == 1) return coarse;

    const int maxLag = (int) std::ceil (sampleRate / kAbsMinHz) + 2 * decim + 4;
    const int centre = std::clamp ((int) std::lround (coarse), 2 * decim + 2, maxLag - 2 * decim - 2);
    const int span = decim / 2 + 2; // coarse lag is already parabolic-interpolated
    const int lo = centre - span, hi = centre + span;
    const int W = std::clamp (centre, 32, std::max (32, (int) (0.01 * sampleRate)));
    const int64_t newest = fullCount - 1;
    const int64_t base = newest - (W + hi) + 1;
    if (base < 0) return coarse;

    // d(tau) over the most recent window; small lag set, so direct evaluation is cheap.
    float bestVal = 0.0f;
    int bestTau = -1;
    float vals[64];
    const int count = std::min (hi - lo + 1, 64);
    for (int i = 0; i < count; ++i)
    {
        const int tau = lo + i;
        float acc = 0.0f;
        for (int j = 0; j < W; ++j)
        {
            const float d = fullRing.read (base + j) - fullRing.read (base + j + tau);
            acc += d * d;
        }
        vals[i] = acc;
        if (bestTau < 0 || acc < bestVal) { bestVal = acc; bestTau = i; }
    }
    if (bestTau <= 0 || bestTau >= count - 1) return coarse; // edge: trust the coarse value

    const float a = vals[bestTau - 1], b = vals[bestTau], c = vals[bestTau + 1];
    const float den = a - 2.0f * b + c;
    float off = 0.0f;
    if (std::abs (den) > 1.0e-12f) off = std::clamp (0.5f * (a - c) / den, -0.5f, 0.5f);
    return (float) (lo + bestTau) + off;
}

} // namespace ac
