// Auto Clown — shared DSP utilities (no JUCE dependency).
// Everything here is real-time safe once constructed/prepared.
#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

namespace ac
{
inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr double kPiD = 3.14159265358979323846;

inline float hzToMidi (float hz, float refA4 = 440.0f) noexcept { return 69.0f + 12.0f * std::log2 (hz / refA4); }
inline float midiToHz (float midi, float refA4 = 440.0f) noexcept { return refA4 * std::exp2 ((midi - 69.0f) / 12.0f); }
inline float semitonesToRatio (float st) noexcept { return std::exp2 (st / 12.0f); }
inline float dbToGain (float db) noexcept { return std::pow (10.0f, db / 20.0f); }

inline int nextPow2 (int v) noexcept
{
    int p = 1;
    while (p < v) p <<= 1;
    return p;
}

/** One-pole lowpass used as an exponential glide. setTime(0) = instant. */
class OnePole
{
public:
    void setTimeConstant (float seconds, float updateRateHz) noexcept
    {
        coeff = (seconds <= 1.0e-5f) ? 0.0f : std::exp (-1.0f / (seconds * updateRateHz));
    }
    float process (float x) noexcept { state = x + coeff * (state - x); return state; }
    void reset (float v) noexcept { state = v; }
    float get() const noexcept { return state; }

private:
    float coeff = 0.0f, state = 0.0f;
};

/** Linear ramp smoother for gains (mix, bypass, voicing crossfade). */
class LinearSmoother
{
public:
    void prepare (double sampleRate, float rampSeconds) noexcept
    {
        steps = std::max (1, (int) std::lround (sampleRate * rampSeconds));
    }
    void setTarget (float t) noexcept
    {
        if (std::abs (t - target) < 1.0e-9f) return;
        target = t;
        remaining = steps;
        inc = (target - current) / (float) steps;
    }
    void snap (float v) noexcept { current = target = v; remaining = 0; }
    float next() noexcept
    {
        if (remaining > 0) { current += inc; if (--remaining == 0) current = target; }
        return current;
    }
    float get() const noexcept { return current; }
    bool isSmoothing() const noexcept { return remaining > 0; }

private:
    float current = 0.0f, target = 0.0f, inc = 0.0f;
    int steps = 1, remaining = 0;
};

/** Transposed direct-form II biquad. */
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowpass (double fs, double fc, double q) noexcept
    {
        const double w = 2.0 * kPiD * fc / fs, c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 - c) * 0.5 / a0); b1 = (float) ((1.0 - c) / a0); b2 = b0;
        a1 = (float) (-2.0 * c / a0);        a2 = (float) ((1.0 - alpha) / a0);
    }
    void setHighpass (double fs, double fc, double q) noexcept
    {
        const double w = 2.0 * kPiD * fc / fs, c = std::cos (w), s = std::sin (w), alpha = s / (2.0 * q);
        const double a0 = 1.0 + alpha;
        b0 = (float) ((1.0 + c) * 0.5 / a0); b1 = (float) (-(1.0 + c) / a0); b2 = b0;
        a1 = (float) (-2.0 * c / a0);        a2 = (float) ((1.0 - alpha) / a0);
    }
    float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0.0f; }
};

/** Power-of-two circular buffer addressed by an absolute 64-bit sample index. */
class HistoryBuffer
{
public:
    void allocate (int minSize)
    {
        const int n = nextPow2 (std::max (minSize, 16));
        data.assign ((size_t) n, 0.0f);
        mask = (int64_t) n - 1;
    }
    void clear() noexcept { std::fill (data.begin(), data.end(), 0.0f); }
    int size() const noexcept { return (int) data.size(); }

    void write (int64_t pos, float v) noexcept { data[(size_t) (pos & mask)] = v; }
    void add (int64_t pos, float v) noexcept { data[(size_t) (pos & mask)] += v; }
    float read (int64_t pos) const noexcept { return data[(size_t) (pos & mask)]; }
    float take (int64_t pos) noexcept
    {
        float& s = data[(size_t) (pos & mask)];
        const float v = s;
        s = 0.0f;
        return v;
    }
    /** Linear interpolation at a fractional absolute position. */
    float readFrac (double pos) const noexcept
    {
        const double fl = std::floor (pos);
        const auto i = (int64_t) fl;
        const auto f = (float) (pos - fl);
        const float a = read (i), b = read (i + 1);
        return a + f * (b - a);
    }
    /** 4-point, 3rd-order Hermite interpolation (cleaner for the Classic shifter). */
    float readHermite (double pos) const noexcept
    {
        const double fl = std::floor (pos);
        const auto i = (int64_t) fl;
        const auto t = (float) (pos - fl);
        const float xm1 = read (i - 1), x0 = read (i), x1 = read (i + 1), x2 = read (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

private:
    std::vector<float> data;
    int64_t mask = 0;
};

/**
    Wait-free single-producer / single-consumer FIFO.
    Producer: audio thread. Consumer: UI thread. Capacity must be a power of two.
    When full, the producer drops the new item (the UI only needs recent frames).
*/
template <typename T, size_t Capacity>
class SpscFifo
{
    static_assert ((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
    static_assert (std::is_trivially_copyable_v<T>, "FIFO items must be trivially copyable");

public:
    bool push (const T& item) noexcept
    {
        const auto w = writeIndex.load (std::memory_order_relaxed);
        const auto r = readIndex.load (std::memory_order_acquire);
        if (w - r >= Capacity) return false;
        buffer[w & (Capacity - 1)] = item;
        writeIndex.store (w + 1, std::memory_order_release);
        return true;
    }
    bool pop (T& item) noexcept
    {
        const auto r = readIndex.load (std::memory_order_relaxed);
        const auto w = writeIndex.load (std::memory_order_acquire);
        if (r == w) return false;
        item = buffer[r & (Capacity - 1)];
        readIndex.store (r + 1, std::memory_order_release);
        return true;
    }
    size_t available() const noexcept
    {
        return writeIndex.load (std::memory_order_acquire) - readIndex.load (std::memory_order_acquire);
    }

private:
    std::array<T, Capacity> buffer {};
    alignas (64) std::atomic<size_t> writeIndex { 0 };
    alignas (64) std::atomic<size_t> readIndex { 0 };
};

} // namespace ac
