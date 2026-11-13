#include "TestFramework.h"
#include "TestSignals.h"

#include "dsp/AutoClownEngine.h"

using namespace ac;

TEST_CASE ("learn: picks a fitting input type from the pitch distribution")
{
    struct Case { float lo, hi; InputType expect; };
    const Case cases[] = {
        { 260.0f, 880.0f, InputType::Soprano },
        { 110.0f, 440.0f, InputType::AltoTenor },
        { 70.0f, 300.0f, InputType::LowMale },
        { 35.0f, 200.0f, InputType::BassInstrument },
        { 60.0f, 1700.0f, InputType::Instrument },
    };
    for (const auto& c : cases)
    {
        RangeLearner l;
        for (int i = 0; i < 1000; ++i)
        {
            const float hz = c.lo * std::pow (c.hi / c.lo, (float) i / 999.0f);
            l.add (hz, 0.005f);
        }
        const int got = l.decide (4.0f);
        INFO ("%.0f-%.0f Hz -> %s", c.lo, c.hi, inputTypeName ((InputType) got));
        CHECK (got == (int) c.expect);
    }
    RangeLearner empty;
    CHECK (empty.decide (1.0f) == -1);
}

TEST_CASE ("auto key: melodies in several keys")
{
    struct Case { std::vector<int> melody; int key; bool minor; };
    // Scale-degree melodies weighted towards tonic/fifth, as real melodies are.
    const std::vector<int> majorTune { 0, 4, 7, 4, 0, 2, 4, 5, 7, 7, 9, 7, 5, 4, 2, 0, 11, 0, 7, 0 };
    const std::vector<int> minorTune { 0, 3, 7, 3, 0, 2, 3, 5, 7, 8, 7, 5, 3, 2, 0, 7, 0, 3, 0, 11 };
    for (int key : { 0, 2, 7, 10 })
        for (bool minor : { false, true })
        {
            KeyDetector k;
            for (int rep = 0; rep < 5; ++rep)
                for (int d : (minor ? minorTune : majorTune))
                    k.add (60.0f + (float) key + (float) d + 0.05f, 0.25f);
            const auto r = k.estimate();
            INFO ("%s %s -> %s %s (r=%.2f)", pitchClassName (key), minor ? "minor" : "major",
                  pitchClassName (r.key), r.minor ? "minor" : "major", r.score);
            CHECK (r.key == key);
            CHECK (r.minor == minor);
        }
}

TEST_CASE ("engine: Learn runs end-to-end on audio")
{
    const double fs = 44100.0;
    AutoClownEngine e;
    e.prepare (fs, 512, 1, LatencyMode::Low);
    EngineParams p;
    p.inputType = InputType::Soprano; // wrong on purpose
    e.setParams (p);
    e.requestLearn();
    // A low male line: notes between 90 and 220 Hz.
    actest::Buffer x;
    for (double hz : { 98.0, 110.0, 147.0, 196.0, 123.0, 165.0, 104.0 })
    {
        auto n = actest::vowel (fs, hz, 0.8);
        x.insert (x.end(), n.begin(), n.end());
    }
    for (size_t i = 0; i + 512 <= x.size(); i += 512)
    {
        float* ch[1] = { x.data() + i };
        e.process (ch, 1, 512);
    }
    const int learned = e.learnedInputType.load();
    INFO ("learned: %s", learned >= 0 ? inputTypeName ((InputType) learned) : "(none)");
    CHECK (learned == (int) InputType::LowMale || learned == (int) InputType::AltoTenor);
}
