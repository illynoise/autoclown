// Deterministic test-signal generators and small analysis helpers.
#pragma once

#include "dsp/DspUtils.h"
#include "dsp/PitchDetector.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace actest
{
using Buffer = std::vector<float>;

inline Buffer sine (double fs, double hz, double seconds, float amp = 0.5f)
{
    Buffer b ((size_t) (fs * seconds));
    for (size_t i = 0; i < b.size(); ++i) b[i] = amp * (float) std::sin (2.0 * ac::kPiD * hz * (double) i / fs);
    return b;
}

/** Additive harmonic tone. amps[k] is the amplitude of harmonic k+1. */
inline Buffer harmonics (double fs, double hz, double seconds, const std::vector<float>& amps)
{
    Buffer b ((size_t) (fs * seconds));
    for (size_t i = 0; i < b.size(); ++i)
    {
        double v = 0.0;
        for (size_t k = 0; k < amps.size(); ++k)
        {
            const double f = hz * (double) (k + 1);
            if (f < fs * 0.45) v += amps[k] * std::sin (2.0 * ac::kPiD * f * (double) i / fs);
        }
        b[i] = (float) v;
    }
    return b;
}

/** Logarithmic sine sweep; instFreq receives the instantaneous frequency per sample. */
inline Buffer logSweep (double fs, double f0, double f1, double seconds, Buffer* instFreq = nullptr)
{
    const size_t n = (size_t) (fs * seconds);
    Buffer b (n);
    if (instFreq) instFreq->resize (n);
    double phase = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double f = f0 * std::pow (f1 / f0, (double) i / (double) n);
        phase += 2.0 * ac::kPiD * f / fs;
        b[i] = 0.5f * (float) std::sin (phase);
        if (instFreq) (*instFreq)[i] = (float) f;
    }
    return b;
}

/**
    Synthetic sung vowel: band-limited glottal source (-12 dB/oct harmonics) through three
    formant resonators, optional vibrato (rate Hz / depth cents), jitter and breath noise.
    The fundamental is f(t) = hz * 2^(vib/1200).
*/
inline Buffer vowel (double fs, double hz, double seconds, double vibRate = 0.0, double vibCents = 0.0,
                     double noiseDb = -80.0, std::array<double, 3> formants = { 700.0, 1220.0, 2600.0 },
                     uint32_t seed = 1)
{
    const size_t n = (size_t) (fs * seconds);
    Buffer src (n), out (n);
    std::mt19937 rng (seed);
    std::normal_distribution<double> gauss (0.0, 1.0);
    double phase = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / fs;
        const double f = hz * std::pow (2.0, vibCents * std::sin (2.0 * ac::kPiD * vibRate * t) / 1200.0);
        phase += 2.0 * ac::kPiD * f / fs;
        double v = 0.0;
        for (int k = 1; k * f < fs * 0.45 && k < 60; ++k) v += std::sin (k * phase) / (double) (k * k) * 1.0;
        src[i] = (float) v;
    }
    ac::Biquad bp[3];
    const double q[3] = { 8.0, 10.0, 12.0 };
    for (int k = 0; k < 3; ++k)
    {
        const double w = 2.0 * ac::kPiD * formants[(size_t) k] / fs, alpha = std::sin (w) / (2.0 * q[k]);
        const double a0 = 1.0 + alpha;
        bp[k].b0 = (float) (alpha / a0); bp[k].b1 = 0.0f; bp[k].b2 = (float) (-alpha / a0);
        bp[k].a1 = (float) (-2.0 * std::cos (w) / a0); bp[k].a2 = (float) ((1.0 - alpha) / a0);
    }
    const double gains[3] = { 1.0, 0.6, 0.25 };
    float peak = 0.0f;
    for (size_t i = 0; i < n; ++i)
    {
        double v = 0.15 * src[i];
        for (int k = 0; k < 3; ++k) v += gains[k] * bp[k].process (src[i]);
        out[i] = (float) v;
        peak = std::max (peak, std::abs (out[i]));
    }
    const double noiseAmp = std::pow (10.0, noiseDb / 20.0);
    for (size_t i = 0; i < n; ++i)
        out[i] = 0.5f * out[i] / std::max (peak, 1.0e-6f) + (float) (noiseAmp * 0.5 * gauss (rng));
    return out;
}

inline Buffer whiteNoise (double fs, double seconds, float amp, uint32_t seed = 7)
{
    std::mt19937 rng (seed);
    std::uniform_real_distribution<float> u (-amp, amp);
    Buffer b ((size_t) (fs * seconds));
    for (auto& s : b) s = u (rng);
    return b;
}

/** "sss"-like fricative: noise high-passed at 4 kHz. */
inline Buffer fricative (double fs, double seconds, float amp)
{
    Buffer b = whiteNoise (fs, seconds, amp, 11);
    ac::Biquad h1, h2;
    h1.setHighpass (fs, 4000.0, 0.7071);
    h2.setHighpass (fs, 4000.0, 0.7071);
    for (auto& s : b) s = h2.process (h1.process (s)) * 2.0f;
    return b;
}

struct Track { std::vector<float> hz; std::vector<bool> voiced; std::vector<int64_t> time; };

inline Track runDetector (ac::PitchDetector& d, const Buffer& x)
{
    Track t;
    for (float s : x)
        if (d.pushSample (s))
        {
            const auto& e = d.getEstimate();
            t.hz.push_back (e.hz);
            t.voiced.push_back (e.voiced);
            t.time.push_back (e.sampleTime);
        }
    return t;
}

inline double cents (double a, double b) { return 1200.0 * std::log2 (a / b); }

/** Median detected frequency over voiced frames after `skip` frames. */
inline double medianVoicedHz (const Track& t, size_t skip = 20)
{
    std::vector<float> v;
    for (size_t i = skip; i < t.hz.size(); ++i)
        if (t.voiced[i]) v.push_back (t.hz[i]);
    if (v.empty()) return 0.0;
    std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
    return v[v.size() / 2];
}

inline double voicedFraction (const Track& t, size_t skip = 20)
{
    size_t n = 0, v = 0;
    for (size_t i = skip; i < t.hz.size(); ++i) { ++n; v += t.voiced[i] ? 1 : 0; }
    return n ? (double) v / (double) n : 0.0;
}

/** Simple RIFF/WAVE reader (PCM 16/24/32 and float32), returns first channel. */
inline bool readWav (const std::filesystem::path& path, Buffer& out, double& fs)
{
    std::ifstream f (path, std::ios::binary);
    if (! f) return false;
    auto rd32 = [&] { uint32_t v = 0; f.read ((char*) &v, 4); return v; };
    auto rd16 = [&] { uint16_t v = 0; f.read ((char*) &v, 2); return v; };
    char id[4];
    f.read (id, 4); rd32(); f.read (id, 4);
    uint16_t fmt = 0, ch = 0, bits = 0;
    uint32_t rate = 0;
    while (f.read (id, 4))
    {
        const uint32_t size = rd32();
        if (std::string (id, 4) == "fmt ")
        {
            fmt = rd16(); ch = rd16(); rate = rd32(); rd32(); rd16(); bits = rd16();
            f.seekg (size - 16, std::ios::cur);
        }
        else if (std::string (id, 4) == "data")
        {
            if (ch == 0) return false;
            const int bytes = bits / 8;
            const size_t frames = size / (size_t) (bytes * ch);
            out.resize (frames);
            std::vector<unsigned char> raw ((size_t) bytes * ch);
            for (size_t i = 0; i < frames; ++i)
            {
                f.read ((char*) raw.data(), (std::streamsize) raw.size());
                float v = 0.0f;
                if (fmt == 3 && bits == 32) std::memcpy (&v, raw.data(), 4);
                else if (bits == 16) v = (float) (int16_t) (raw[0] | (raw[1] << 8)) / 32768.0f;
                else if (bits == 24) v = (float) ((int32_t) ((raw[0] << 8) | (raw[1] << 16) | (raw[2] << 24)) >> 8) / 8388608.0f;
                else if (bits == 32) { int32_t iv; std::memcpy (&iv, raw.data(), 4); v = (float) iv / 2147483648.0f; }
                out[i] = v;
            }
            fs = rate;
            return true;
        }
        else f.seekg (size + (size & 1), std::ios::cur);
    }
    return false;
}

} // namespace actest
