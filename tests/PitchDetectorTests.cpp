// Pitch detector tests: steady tones, sweeps, octave traps, voicing, recorded samples.
#include "TestFramework.h"
#include "TestSignals.h"

#include "dsp/Analysis.h"

using namespace actest;

namespace
{
ac::PitchDetector makeDetector (double fs, ac::InputType type, float tracking = 0.5f)
{
    ac::PitchDetector d;
    d.prepare (fs);
    const auto r = ac::inputTypeRange (type);
    d.setRange (r.lo, r.hi);
    d.setTracking (tracking);
    return d;
}
} // namespace

TEST_CASE ("detector: steady sines across every input range and sample rate")
{
    const double rates[] = { 44100.0, 48000.0, 96000.0, 192000.0 };
    double worst = 0.0;
    for (double fs : rates)
    {
        for (int t = 0; t < (int) ac::InputType::NumTypes; ++t)
        {
            const auto r = ac::inputTypeRange ((ac::InputType) t);
            for (double hz : { r.lo * 1.15, std::sqrt ((double) r.lo * r.hi), r.hi * 0.85 })
            {
                auto d = makeDetector (fs, (ac::InputType) t);
                const auto tr = runDetector (d, sine (fs, hz, 0.35));
                const double med = medianVoicedHz (tr);
                CHECK (voicedFraction (tr) > 0.9);
                const double err = std::abs (cents (med, hz));
                worst = std::max (worst, err);
                if (err > 2.0) INFO ("fs=%.0f type=%d f=%.1f -> %.2f Hz (%.2f c)", fs, t, hz, med, err);
                CHECK (err < 2.0);
            }
        }
    }
    INFO ("worst steady-sine error: %.3f cents", worst);
}

TEST_CASE ("detector: log sine sweep is tracked continuously")
{
    const double fs = 48000.0;
    Buffer inst;
    const auto x = logSweep (fs, 90.0, 850.0, 4.0, &inst);
    auto d = makeDetector (fs, ac::InputType::AltoTenor);
    const auto tr = runDetector (d, x);
    int voiced = 0, total = 0, bad = 0;
    double maxErr = 0.0;
    for (size_t i = 20; i < tr.hz.size(); ++i)
    {
        ++total;
        if (! tr.voiced[i]) continue;
        ++voiced;
        // The estimate describes roughly the last window; compare against the frequency
        // ~ one window ago..now and take the closer one.
        const auto t1 = (size_t) tr.time[i];
        const auto t0 = (size_t) std::max<int64_t> (0, tr.time[i] - (int64_t) (0.012 * fs));
        const double e = std::min (std::abs (cents (tr.hz[i], inst[t1])), std::abs (cents (tr.hz[i], inst[t0])));
        const double eMid = std::abs (cents (tr.hz[i], std::sqrt (inst[t0] * inst[t1])));
        const double err = std::min (e, eMid);
        maxErr = std::max (maxErr, err);
        if (err > 25.0) ++bad;
    }
    INFO ("sweep: voiced %d/%d, frames >25c off: %d, max err %.1f c", voiced, total, bad, maxErr);
    CHECK (voiced > total * 0.95);
    CHECK (bad == 0);
}

TEST_CASE ("detector: octave traps (strong 2nd harmonic, missing fundamental, sawtooth)")
{
    const double fs = 44100.0;
    struct Case { const char* name; Buffer x; double f0; ac::InputType type; };
    std::vector<float> saw;
    for (int k = 1; k <= 40; ++k) saw.push_back (0.5f / (float) k);
    const std::vector<Case> cases = {
        { "2nd harmonic 4x fundamental", harmonics (fs, 196.0, 0.5, { 0.1f, 0.4f, 0.05f }), 196.0, ac::InputType::AltoTenor },
        { "missing fundamental", harmonics (fs, 150.0, 0.5, { 0.0f, 0.3f, 0.3f, 0.2f, 0.1f }), 150.0, ac::InputType::AltoTenor },
        { "sawtooth low male", harmonics (fs, 98.0, 0.5, saw), 98.0, ac::InputType::LowMale },
        { "strong 3rd harmonic", harmonics (fs, 330.0, 0.5, { 0.15f, 0.1f, 0.4f }), 330.0, ac::InputType::Soprano },
        { "bass guitar-like E1", harmonics (fs, 41.2, 0.8, { 0.3f, 0.4f, 0.3f, 0.2f, 0.1f }), 41.2, ac::InputType::BassInstrument },
    };
    for (const auto& c : cases)
    {
        auto d = makeDetector (fs, c.type);
        const auto tr = runDetector (d, c.x);
        int octaveErrors = 0, voiced = 0;
        for (size_t i = 30; i < tr.hz.size(); ++i)
        {
            if (! tr.voiced[i]) continue;
            ++voiced;
            if (std::abs (cents (tr.hz[i], c.f0)) > 300.0) ++octaveErrors;
        }
        INFO ("%-28s median %.2f Hz (expect %.2f), voiced frames %d, octave errors %d",
              c.name, medianVoicedHz (tr, 30), c.f0, voiced, octaveErrors);
        CHECK (voiced > 0);
        CHECK (octaveErrors == 0);
        CHECK (std::abs (cents (medianVoicedHz (tr, 30), c.f0)) < 3.0);
    }
}

TEST_CASE ("detector: sung vowel with vibrato, jitter-free octave stability")
{
    for (double hz : { 110.0, 196.0, 330.0, 523.25 })
    {
        const double fs = 48000.0;
        auto d = makeDetector (fs, hz > 300 ? ac::InputType::Soprano : ac::InputType::AltoTenor);
        const auto x = vowel (fs, hz, 2.0, 5.5, 50.0, -50.0);
        const auto tr = runDetector (d, x);
        int octave = 0;
        for (size_t i = 30; i < tr.hz.size(); ++i)
            if (tr.voiced[i] && std::abs (cents (tr.hz[i], hz)) > 300.0) ++octave;
        INFO ("vowel %.1f Hz: voiced %.1f%%, median %.2f Hz, octave errors %d", hz,
              voicedFraction (tr, 30) * 100.0, medianVoicedHz (tr, 30), octave);
        CHECK (voicedFraction (tr, 30) > 0.95);
        CHECK (octave == 0);
        CHECK (std::abs (cents (medianVoicedHz (tr, 30), hz)) < 8.0);
    }
}

TEST_CASE ("detector: unvoiced material is rejected")
{
    const double fs = 44100.0;
    {
        auto d = makeDetector (fs, ac::InputType::AltoTenor);
        const auto tr = runDetector (d, whiteNoise (fs, 1.0, 0.3f));
        INFO ("white noise voiced fraction %.3f", voicedFraction (tr));
        CHECK (voicedFraction (tr) < 0.03);
    }
    {
        auto d = makeDetector (fs, ac::InputType::AltoTenor, 1.0f); // even fully relaxed
        const auto tr = runDetector (d, fricative (fs, 1.0, 0.3f));
        INFO ("fricative voiced fraction %.3f (tracking=100)", voicedFraction (tr));
        CHECK (voicedFraction (tr) < 0.03);
    }
    {
        auto d = makeDetector (fs, ac::InputType::AltoTenor);
        const auto tr = runDetector (d, Buffer ((size_t) fs, 0.0f));
        CHECK (voicedFraction (tr) == 0.0);
    }
    {
        auto d = makeDetector (fs, ac::InputType::AltoTenor);
        const auto tr = runDetector (d, sine (fs, 220.0, 1.0, 0.0003f)); // -76 dBFS hum
        CHECK (voicedFraction (tr) < 0.03);
    }
}

TEST_CASE ("detector: tracking control trades strictness for breathy input")
{
    const double fs = 44100.0;
    const auto breathy = vowel (fs, 220.0, 1.5, 5.0, 30.0, -24.0); // heavy breath noise
    auto strict = makeDetector (fs, ac::InputType::AltoTenor, 0.0f);
    auto relaxed = makeDetector (fs, ac::InputType::AltoTenor, 1.0f);
    const double vs = voicedFraction (runDetector (strict, breathy));
    const double vr = voicedFraction (runDetector (relaxed, breathy));
    INFO ("breathy vowel voiced: strict %.2f, relaxed %.2f", vs, vr);
    CHECK (vr >= vs);
    CHECK (vr > 0.9);
}

TEST_CASE ("detector: update rate and causality")
{
    for (double fs : { 44100.0, 96000.0 })
    {
        auto d = makeDetector (fs, ac::InputType::Soprano);
        const double hopMs = 1000.0 * d.getHopSize() / fs;
        INFO ("fs %.0f: hop %.2f ms", fs, hopMs);
        CHECK (hopMs <= 3.0);
    }
}

TEST_CASE ("detector: recorded vocal samples in tests/data (optional)")
{
    // Drop mono/stereo WAV files into tests/data. If "<name>.f0" exists next to a file it
    // must contain the expected median f0 in Hz; otherwise only octave stability is checked.
    namespace fs = std::filesystem;
    int files = 0;
    if (fs::exists (AC_TEST_DATA_DIR))
    {
        for (const auto& e : fs::directory_iterator (AC_TEST_DATA_DIR))
        {
            if (e.path().extension() != ".wav") continue;
            Buffer x;
            double rate = 0.0;
            if (! readWav (e.path(), x, rate)) { INFO ("could not read %s", e.path().c_str()); continue; }
            ++files;
            auto d = makeDetector (rate, ac::InputType::AltoTenor, 0.6f);
            const auto tr = runDetector (d, x);
            int jumps = 0;
            for (size_t i = 1; i < tr.hz.size(); ++i)
                if (tr.voiced[i] && tr.voiced[i - 1] && std::abs (cents (tr.hz[i], tr.hz[i - 1])) > 900.0) ++jumps;
            const double med = medianVoicedHz (tr, 0);
            INFO ("%s: voiced %.1f%%, median %.1f Hz, frame-to-frame octave jumps %d",
                  e.path().filename().c_str(), voicedFraction (tr, 0) * 100.0, med, jumps);
            CHECK (jumps <= (int) (tr.hz.size() / 200)); // < 0.5 % of frames
            auto ref = e.path();
            ref.replace_extension (".f0");
            if (fs::exists (ref))
            {
                std::ifstream in (ref);
                double expect = 0.0;
                in >> expect;
                CHECK (std::abs (cents (med, expect)) < 50.0);
            }
        }
    }
    if (files == 0) INFO ("no WAV files in tests/data — skipped (see tests/data/README.md)");
}
