#include "TestFramework.h"

#include "dsp/DspUtils.h"
#include "dsp/RetuneEngine.h"

using namespace ac;

namespace
{
constexpr float kRate = 400.0f; // updates per second (2.5 ms hop)

ScaleMapper cMajor()
{
    ScaleMapper m;
    std::array<NoteMode, 12> modes {};
    m.configure (0, ScaleType::Major, modes);
    return m;
}

RetuneResult runConstant (RetuneEngine& e, const ScaleMapper& m, const RetuneSettings& s, float midi, float seconds)
{
    RetuneResult r;
    for (int i = 0; i < (int) (seconds * kRate); ++i) r = e.update (true, midi, m, s);
    return r;
}
} // namespace

TEST_CASE ("retune: 0 ms is a hard snap to the note")
{
    RetuneEngine e; e.prepare (kRate);
    RetuneSettings s; s.retuneMs = 0.0f;
    const auto m = cMajor();
    const auto r = e.update (true, 60.37f, m, s);
    CHECK (r.targetNote == 60);
    CHECK_NEAR (r.outputMidi, 60.0, 1e-4);
    CHECK_NEAR (r.ratio, std::exp2 (-0.37 / 12.0), 1e-4);
}

TEST_CASE ("retune: glide time constant follows Retune Speed")
{
    RetuneEngine e; e.prepare (kRate);
    RetuneSettings s; s.retuneMs = 100.0f;
    const auto m = cMajor();
    // Start in tune on C, then the singer jumps to a flat D (61.7): the correction moves
    // from 0 towards +0.3 with a 100 ms time constant.
    runConstant (e, m, s, 60.0f, 0.2f);
    RetuneResult r;
    for (int i = 0; i < (int) (0.1f * kRate); ++i) r = e.update (true, 61.7f, m, s);
    const float fraction = (r.outputMidi - 61.7f) / 0.3f;
    INFO ("after one time constant: %.2f of the correction applied", fraction);
    CHECK (fraction > 0.55f && fraction < 0.72f);
    r = runConstant (e, m, s, 61.7f, 1.0f);
    CHECK_NEAR (r.outputMidi, 62.0, 0.01);
}

TEST_CASE ("retune: Flex Tune leaves far-off bends alone and corrects near notes")
{
    RetuneEngine e; e.prepare (kRate);
    RetuneSettings s; s.retuneMs = 0.0f; s.flex = 1.0f; // outer window 12 cents
    const auto m = cMajor();
    auto r = runConstant (e, m, s, 60.45f, 0.1f); // 45 cents sharp -> untouched
    CHECK_NEAR (r.outputMidi, 60.45, 1e-3);
    r = runConstant (e, m, s, 60.04f, 0.1f);       // 4 cents -> corrected
    CHECK_NEAR (r.outputMidi, 60.0, 1e-3);
    s.flex = 0.0f;
    r = runConstant (e, m, s, 60.45f, 0.1f);
    CHECK_NEAR (r.outputMidi, 60.0, 1e-3);
}

TEST_CASE ("retune: Humanize relaxes sustained notes but not short ones")
{
    const auto m = cMajor();
    auto measure = [&] (float humanize, float sustainSeconds)
    {
        RetuneEngine e; e.prepare (kRate);
        RetuneSettings s; s.retuneMs = 10.0f; s.humanize = humanize;
        runConstant (e, m, s, 62.0f, sustainSeconds);           // in tune on D
        // A pitch drift of 30 cents sharp on the same note; measure how much is left after 20 ms.
        RetuneResult r;
        for (int i = 0; i < (int) (0.02f * kRate); ++i) r = e.update (true, 62.3f, m, s);
        return r.outputMidi - 62.0f; // residual error (semitones)
    };
    const float shortPlain = measure (0.0f, 0.05f), shortHuman = measure (1.0f, 0.05f);
    const float longPlain = measure (0.0f, 1.0f), longHuman = measure (1.0f, 1.0f);
    INFO ("residual after 20 ms: short %.3f/%.3f, sustained %.3f/%.3f (plain/humanized)",
          shortPlain, shortHuman, longPlain, longHuman);
    CHECK_NEAR (shortHuman, shortPlain, 0.01);
    CHECK (longHuman > longPlain * 2.0f);
}

TEST_CASE ("retune: Natural Vibrato scales the singer's vibrato depth")
{
    const auto m = cMajor();
    auto depth = [&] (float db)
    {
        RetuneEngine e; e.prepare (kRate);
        RetuneSettings s; s.retuneMs = 400.0f; s.vibratoDb = db; // slow retune: vibrato passes
        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < (int) (3.0f * kRate); ++i)
        {
            const float t = (float) i / kRate;
            const float p = 69.0f + 0.4f * std::sin (2.0f * kPi * 5.5f * t);
            const auto r = e.update (true, p, m, s);
            if (t > 1.5f) { lo = std::min (lo, r.outputMidi); hi = std::max (hi, r.outputMidi); }
        }
        return hi - lo;
    };
    const float d0 = depth (0.0f), dm = depth (-12.0f), dp = depth (6.0f);
    INFO ("vibrato p-p: 0 dB %.3f, -12 dB %.3f, +6 dB %.3f semitones", d0, dm, dp);
    CHECK (dm < d0 * 0.45f);
    CHECK (dp > d0 * 1.5f);
}

TEST_CASE ("retune: Hold freezes the output pitch, Transpose offsets it")
{
    RetuneEngine e; e.prepare (kRate);
    RetuneSettings s; s.retuneMs = 0.0f;
    const auto m = cMajor();
    runConstant (e, m, s, 64.1f, 0.1f);
    s.hold = true;
    auto r = runConstant (e, m, s, 67.2f, 0.2f);
    CHECK_NEAR (r.outputMidi, 64.0, 1e-3);
    s.hold = false; s.transpose = 12.0f;
    r = runConstant (e, m, s, 67.2f, 0.05f);
    CHECK_NEAR (r.outputMidi, 79.0, 1e-3);
    CHECK_NEAR (r.ratio, std::exp2 ((79.0 - 67.2) / 12.0), 1e-3);
}

TEST_CASE ("retune: Bypass note passes the singer through, unvoiced ratio is 1")
{
    ScaleMapper m;
    std::array<NoteMode, 12> modes {};
    modes[0] = NoteMode::Bypass;
    m.configure (0, ScaleType::Major, modes);
    RetuneEngine e; e.prepare (kRate);
    RetuneSettings s; s.retuneMs = 0.0f;
    auto r = runConstant (e, m, s, 60.3f, 0.1f);
    CHECK (r.bypassNote);
    CHECK_NEAR (r.outputMidi, 60.3, 1e-3);
    r = e.update (false, 0.0f, m, s);
    CHECK (r.ratio == 1.0f);
}
