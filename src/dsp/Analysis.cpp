#include "Analysis.h"

#include "DspUtils.h"

namespace ac
{
namespace
{
    constexpr FreqRange kRanges[] = {
        { 140.0f, 1400.0f }, // Soprano
        {  85.0f,  900.0f }, // Alto/Tenor
        {  55.0f,  520.0f }, // Low Male
        {  55.0f, 2000.0f }, // Instrument
        {  30.0f,  420.0f }, // Bass Instrument
    };
    constexpr const char* kNames[] = { "Soprano", "Alto/Tenor", "Low Male", "Instrument", "Bass Instrument" };

    // Krumhansl & Kessler (1982) probe-tone profiles.
    constexpr float kMajor[12] = { 6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f };
    constexpr float kMinor[12] = { 6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f };

    float pearson (const float* a, const float* b) noexcept
    {
        float ma = 0, mb = 0;
        for (int i = 0; i < 12; ++i) { ma += a[i]; mb += b[i]; }
        ma /= 12.0f; mb /= 12.0f;
        float num = 0, da = 0, db = 0;
        for (int i = 0; i < 12; ++i)
        {
            num += (a[i] - ma) * (b[i] - mb);
            da += (a[i] - ma) * (a[i] - ma);
            db += (b[i] - mb) * (b[i] - mb);
        }
        return (da > 0 && db > 0) ? num / std::sqrt (da * db) : 0.0f;
    }
} // namespace

FreqRange inputTypeRange (InputType t) noexcept
{
    return kRanges[std::clamp ((int) t, 0, (int) InputType::NumTypes - 1)];
}

const char* inputTypeName (InputType t) noexcept
{
    return kNames[std::clamp ((int) t, 0, (int) InputType::NumTypes - 1)];
}

void RangeLearner::add (float hz, float w) noexcept
{
    if (! (hz > 0.0f) || w <= 0.0f) return;
    const int bin = std::clamp ((int) std::lround (hzToMidi (hz)), 0, 127);
    hist[(size_t) bin] += w;
    total += w;
}

float RangeLearner::percentileHz (float p) const noexcept
{
    if (total <= 0.0f) return 0.0f;
    const float goal = p * total;
    float acc = 0.0f;
    for (int i = 0; i < 128; ++i)
    {
        acc += hist[(size_t) i];
        if (acc >= goal) return midiToHz ((float) i);
    }
    return midiToHz (127.0f);
}

int RangeLearner::decide (float minWeight) const noexcept
{
    if (total < minWeight) return -1;
    const float lo = percentileHz (0.05f) / 1.06f; // one semitone of headroom
    const float hi = percentileHz (0.95f) * 1.06f;

    // Prefer vocal ranges, then the bass range, then the wide instrument range.
    constexpr InputType order[] = { InputType::Soprano, InputType::AltoTenor, InputType::LowMale,
                                    InputType::BassInstrument, InputType::Instrument };
    // Among ranges that cover the singer, pick the one whose (log) centre is closest to
    // the singer's centre; the wide Instrument range carries a penalty.
    const float centre = std::log2 (std::sqrt (lo * hi));
    int best = -1;
    float bestScore = 1.0e9f;
    for (auto t : order)
    {
        const auto r = inputTypeRange (t);
        if (lo >= r.lo && hi <= r.hi)
        {
            float score = std::abs (centre - std::log2 (std::sqrt (r.lo * r.hi)));
            if (t == InputType::Instrument) score += 0.5f;
            if (score < bestScore) { bestScore = score; best = (int) t; }
        }
    }
    if (best < 0) best = lo < 55.0f ? (int) InputType::BassInstrument : (int) InputType::Instrument;
    return best;
}

void KeyDetector::add (float midi, float w) noexcept
{
    if (! std::isfinite (midi) || w <= 0.0f) return;
    const float nearest = std::round (midi);
    const float cents = std::abs (midi - nearest) * 100.0f;
    if (cents > 40.0f) return; // ignore glides between notes
    const int pc = (((int) nearest % 12) + 12) % 12;
    const float wt = w * (1.0f - cents / 80.0f);
    chroma[(size_t) pc] += wt;
    total += wt;
}

KeyDetector::Result KeyDetector::estimate() const noexcept
{
    Result r;
    if (total <= 0.0f) return r;
    float rotated[12];
    r.score = -2.0f;
    for (int key = 0; key < 12; ++key)
    {
        for (int i = 0; i < 12; ++i) rotated[i] = chroma[(size_t) ((i + key) % 12)];
        const float maj = pearson (rotated, kMajor);
        const float mnr = pearson (rotated, kMinor);
        if (maj > r.score) { r.score = maj; r.key = key; r.minor = false; }
        if (mnr > r.score) { r.score = mnr; r.key = key; r.minor = true; }
    }
    return r;
}

} // namespace ac
