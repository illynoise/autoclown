// End-to-end engine tests: pitch accuracy of both shifters, formant behaviour,
// unvoiced pass-through, latency alignment, block-size independence, RT safety.
#include "TestFramework.h"
#include "TestSignals.h"

#include "dsp/AutoClownEngine.h"

using namespace actest;
using namespace ac;

namespace
{
EngineParams baseParams()
{
    EngineParams p;
    p.inputType = InputType::AltoTenor;
    p.key = 0;
    p.scale = ScaleType::Chromatic;
    p.retune.retuneMs = 0.0f;
    p.noteModes.fill (NoteMode::Scale);
    return p;
}

Buffer runEngine (const Buffer& x, double fs, const EngineParams& p, LatencyMode lm, int block = 256,
                  AutoClownEngine* external = nullptr)
{
    AutoClownEngine local;
    AutoClownEngine& e = external ? *external : local;
    e.prepare (fs, block, 1, lm);
    e.setParams (p);
    Buffer y = x;
    for (size_t i = 0; i < y.size(); i += (size_t) block)
    {
        float* ch[1] = { y.data() + i };
        e.process (ch, 1, (int) std::min<size_t> ((size_t) block, y.size() - i));
    }
    return y;
}

double outputPitch (const Buffer& y, double fs, InputType type = InputType::AltoTenor)
{
    PitchDetector d;
    d.prepare (fs);
    const auto r = inputTypeRange (type);
    d.setRange (r.lo, r.hi);
    d.setTracking (0.5f);
    Buffer tail (y.begin() + (long) (0.3 * fs), y.end());
    return medianVoicedHz (runDetector (d, tail), 10);
}

/** Power-weighted spectral centroid between 250 Hz and 3.5 kHz (naive DFT, Hann). */
double centroid (const Buffer& y, double fs)
{
    const size_t N = 8192, start = y.size() - N - 100;
    double num = 0.0, den = 0.0;
    for (double f = 250.0; f < 3500.0; f += fs / (double) N)
    {
        double re = 0.0, im = 0.0;
        for (size_t i = 0; i < N; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2.0 * kPiD * (double) i / (double) N);
            const double ph = 2.0 * kPiD * f * (double) i / fs;
            re += w * y[start + i] * std::cos (ph);
            im -= w * y[start + i] * std::sin (ph);
        }
        const double pw = re * re + im * im;
        num += pw * f; den += pw;
    }
    return num / den;
}
} // namespace

TEST_CASE ("engine: hard-tune corrects a sharp note to pitch (both engines, both latency modes)")
{
    const double fs = 48000.0;
    const double inHz = 220.0 * std::exp2 (38.0 / 1200.0); // A3 + 38 cents
    const auto x = vowel (fs, inHz, 1.2);
    for (auto mode : { EngineMode::Modern, EngineMode::Classic })
        for (auto lm : { LatencyMode::Low, LatencyMode::HighQuality })
        {
            auto p = baseParams();
            p.engine = mode;
            const auto y = runEngine (x, fs, p, lm);
            const double out = outputPitch (y, fs);
            INFO ("%s / %s latency: %.2f Hz -> %.2f Hz (%.1f cents from A3)",
                  mode == EngineMode::Modern ? "Modern " : "Classic", lm == LatencyMode::Low ? "low " : "HQ  ",
                  inHz, out, cents (out, 220.0));
            CHECK (std::abs (cents (out, 220.0)) < 5.0);
        }
}

TEST_CASE ("engine: transpose by +7 and -12 semitones")
{
    const double fs = 44100.0;
    const auto x = vowel (fs, 196.0, 1.2); // G3, in tune
    for (int st : { 7, -12 })
    {
        auto p = baseParams();
        p.retune.transpose = (float) st;
        p.inputType = InputType::AltoTenor;
        const auto y = runEngine (x, fs, p, LatencyMode::HighQuality);
        const double expect = 196.0 * std::exp2 (st / 12.0);
        const double out = outputPitch (y, fs, st < 0 ? InputType::LowMale : InputType::AltoTenor);
        INFO ("transpose %+d: %.2f Hz (expect %.2f, %.1f c)", st, out, expect, cents (out, expect));
        CHECK (std::abs (cents (out, expect)) < 6.0);
    }
}

TEST_CASE ("engine: formant correction keeps the spectral envelope, off moves it")
{
    const double fs = 48000.0;
    const auto x = vowel (fs, 150.0, 1.5, 0.0, 0.0, -80.0, { 600.0, 1100.0, 2500.0 });
    const double c0 = centroid (x, fs);
    auto p = baseParams();
    p.retune.transpose = 5.0f;
    p.formantOn = true; p.formantAmount = 1.0f;
    const double cOn = centroid (runEngine (x, fs, p, LatencyMode::HighQuality), fs);
    p.formantOn = false;
    const double cOff = centroid (runEngine (x, fs, p, LatencyMode::HighQuality), fs);
    INFO ("centroid: dry %.0f Hz, +5 st formant-on %.0f Hz (x%.3f), formant-off %.0f Hz (x%.3f)",
          c0, cOn, cOn / c0, cOff, cOff / c0);
    CHECK (std::abs (cOn / c0 - 1.0) < 0.10);
    CHECK (cOff / c0 > 1.2);
}

TEST_CASE ("engine: unvoiced input is passed through unshifted and latency-aligned")
{
    const double fs = 44100.0;
    const auto x = fricative (fs, 1.0, 0.3f);
    for (auto lm : { LatencyMode::Low, LatencyMode::HighQuality })
    {
        auto p = baseParams();
        p.retune.transpose = 5.0f; // would be audible if applied
        AutoClownEngine e;
        const auto y = runEngine (x, fs, p, lm, 128, &e);
        const int L = e.getLatencySamples();
        double err = 0.0;
        for (size_t i = (size_t) L + 2000; i < x.size(); ++i) err = std::max (err, (double) std::abs (y[i] - x[i - (size_t) L]));
        INFO ("latency %d samples (%.2f ms), max deviation from delayed dry %.2e", L, 1000.0 * L / fs, err);
        CHECK (err < 1.0e-6);
        if (lm == LatencyMode::Low) CHECK (1000.0 * L / fs < 10.0);
    }
}

TEST_CASE ("engine: in-tune voice at ratio 1 stays close to the dry signal (HQ)")
{
    const double fs = 48000.0;
    const auto x = vowel (fs, 220.0, 1.0); // exactly A3
    auto p = baseParams();
    AutoClownEngine e;
    const auto y = runEngine (x, fs, p, LatencyMode::HighQuality, 256, &e);
    const int L = e.getLatencySamples();
    double num = 0, dx = 0, dy = 0;
    for (size_t i = (size_t) (0.3 * fs); i < x.size(); ++i)
    {
        const double a = x[i - (size_t) L], b = y[i];
        num += a * b; dx += a * a; dy += b * b;
    }
    const double corr = num / std::sqrt (dx * dy);
    INFO ("correlation with delayed dry: %.4f, level ratio %.3f", corr, std::sqrt (dy / dx));
    CHECK (corr > 0.97);
}

TEST_CASE ("engine: output is identical for any block size (incl. 1 and 32)")
{
    const double fs = 44100.0;
    const auto x = vowel (fs, 233.0, 0.6, 5.0, 40.0);
    auto p = baseParams();
    p.scale = ScaleType::Major;
    p.retune.retuneMs = 30.0f;
    const auto ref = runEngine (x, fs, p, LatencyMode::Low, 512);
    for (int bs : { 1, 32, 333 })
    {
        const auto y = runEngine (x, fs, p, LatencyMode::Low, bs);
        double err = 0.0;
        for (size_t i = 0; i < y.size(); ++i) err = std::max (err, (double) std::abs (y[i] - ref[i]));
        INFO ("block %d: max diff %.2e", bs, err);
        CHECK (err < 1.0e-6);
    }
}

TEST_CASE ("engine: process() performs no heap allocation")
{
    const double fs = 96000.0;
    auto x = vowel (fs, 300.0, 0.5, 5.0, 60.0);
    const auto noise = whiteNoise (fs, 0.2, 0.2f);
    x.insert (x.end(), noise.begin(), noise.end());
    AutoClownEngine e;
    e.prepare (fs, 32, 2, LatencyMode::Low);
    auto p = baseParams();
    p.engine = EngineMode::Classic;
    e.setParams (p);
    Buffer l = x, r = x;
    AllocationCounter::start();
    for (size_t i = 0; i + 32 <= x.size(); i += 32)
    {
        if (i == x.size() / 2) { p.engine = EngineMode::Modern; e.setParams (p); e.noteOn (60); e.requestLearn(); e.requestAutoKey(); }
        float* ch[2] = { l.data() + i, r.data() + i };
        e.process (ch, 2, 32);
        MeterFrame f;
        while (e.meterFifo.pop (f)) {}
    }
    const long n = AllocationCounter::stop();
    INFO ("allocations during processing: %ld", n);
    CHECK (n == 0);
}

TEST_CASE ("engine: MIDI targets override the scale")
{
    const double fs = 44100.0;
    const auto x = vowel (fs, 233.08, 1.2); // A#3, in tune
    auto p = baseParams();
    p.midiEnabled = true;
    AutoClownEngine e;
    e.prepare (fs, 256, 1, LatencyMode::HighQuality);
    e.setParams (p);
    e.noteOn (57); // A3 -> pitch class A only
    e.setParams (p);
    Buffer y = x;
    for (size_t i = 0; i < y.size(); i += 256)
    {
        float* ch[1] = { y.data() + i };
        e.process (ch, 1, (int) std::min<size_t> (256, y.size() - i));
    }
    const double out = outputPitch (y, fs);
    INFO ("A#3 with MIDI A held -> %.2f Hz", out);
    CHECK (std::abs (cents (out, 220.0)) < 6.0);
}

TEST_CASE ("engine: stereo channels are shifted coherently")
{
    const double fs = 48000.0;
    const auto x = vowel (fs, 210.0, 0.8);
    AutoClownEngine e;
    e.prepare (fs, 128, 2, LatencyMode::Low);
    e.setParams (baseParams());
    Buffer l = x, r = x;
    for (size_t i = 0; i < x.size(); i += 128)
    {
        float* ch[2] = { l.data() + i, r.data() + i };
        e.process (ch, 2, (int) std::min<size_t> (128, x.size() - i));
    }
    double err = 0.0;
    for (size_t i = 0; i < l.size(); ++i) err = std::max (err, (double) std::abs (l[i] - r[i]));
    CHECK (err < 1.0e-6);
}
