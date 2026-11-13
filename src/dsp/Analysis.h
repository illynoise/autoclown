// Auto Clown — input-range learning and key estimation (real-time safe, fixed size).
#pragma once

#include <array>

namespace ac
{
enum class InputType : int { Soprano = 0, AltoTenor, LowMale, Instrument, BassInstrument, NumTypes };

struct FreqRange { float lo, hi; };
FreqRange inputTypeRange (InputType t) noexcept;
const char* inputTypeName (InputType t) noexcept;

/** "Learn": histogram of voiced pitch; picks the narrowest input type that covers it. */
class RangeLearner
{
public:
    void reset() noexcept { hist.fill (0.0f); total = 0.0f; }
    void add (float hz, float weight) noexcept;
    float totalWeight() const noexcept { return total; }
    /** Returns -1 if there is not enough data yet. */
    int decide (float minWeight) const noexcept;
    /** Percentile of the pitch distribution in Hz (p in 0..1). */
    float percentileHz (float p) const noexcept;

private:
    std::array<float, 128> hist {};
    float total = 0.0f;
};

/** "Auto Key": chroma profile correlated against Krumhansl–Kessler key profiles. */
class KeyDetector
{
public:
    struct Result { int key = -1; bool minor = false; float score = 0.0f; };

    void reset() noexcept { chroma.fill (0.0f); total = 0.0f; }
    /** midi is the detected pitch (fractional); weight ~ seconds * confidence. */
    void add (float midi, float weight) noexcept;
    float totalWeight() const noexcept { return total; }
    Result estimate() const noexcept;

private:
    std::array<float, 12> chroma {};
    float total = 0.0f;
};

} // namespace ac
