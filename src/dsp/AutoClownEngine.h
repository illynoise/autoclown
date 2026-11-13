// Auto Clown — complete real-time processing chain (mono or stereo, in place).
//
//   in ─┬─> mono sum ─> PitchDetector ─(per hop)─> RetuneEngine ─> ratio, voicing
//       │                 └─> Learn / Auto Key accumulators
//       ├─> history rings ─> PSOLA (Modern) or delay-line (Classic) ─> wet
//       └─> history rings ─> dry delayed by the reported latency
//   out = dry + bypassGain * mix * voicedGate * (wet - dry)
//
// Unvoiced audio (consonants, breaths, silence) crossfades to the delayed dry signal
// over ~6 ms, so it is passed through unshifted and phase-aligned with the wet path.
//
// Threading: prepare()/setLatencyMode() allocate and must not run concurrently with
// process(). Everything else is real-time safe. Results for the UI are published
// through a wait-free SPSC FIFO and atomics.
#pragma once

#include "Analysis.h"
#include "ClassicShifter.h"
#include "PitchDetector.h"
#include "PsolaShifter.h"
#include "RetuneEngine.h"

namespace ac
{
enum class EngineMode : int { Modern = 0, Classic };
enum class LatencyMode : int { Low = 0, HighQuality };

struct EngineParams
{
    float tracking = 0.5f;            // 0..1
    InputType inputType = InputType::AltoTenor;
    int key = 0;                      // 0 = C
    ScaleType scale = ScaleType::Major;
    RetuneSettings retune;
    bool formantOn = true;
    float formantAmount = 1.0f;       // 0..1
    float refHz = 440.0f;             // detune reference for A4
    float mix = 1.0f;                 // 0..1
    EngineMode engine = EngineMode::Modern;
    bool midiEnabled = false;
    bool bypass = false;
    std::array<NoteMode, 12> noteModes {};
};

struct MeterFrame
{
    float inputHz = 0.0f;
    float inputMidi = 0.0f;
    float outputMidi = 0.0f;
    float correctionCents = 0.0f;
    float deviationCents = 0.0f;
    float levelDb = -120.0f;
    int16_t targetNote = -1;
    uint16_t targetMask = 0;
    uint16_t heldMidiMask = 0;
    uint8_t voiced = 0;
    uint8_t bypassNote = 0;
};

class AutoClownEngine
{
public:
    static constexpr int kMaxChannels = PsolaShifter::kMaxChannels;

    void prepare (double sampleRate, int maxBlockSize, int numChannels, LatencyMode mode);
    void setLatencyMode (LatencyMode mode); // not RT-safe (resets state)
    void reset();

    int getLatencySamples() const noexcept { return latency; }
    static int latencyFor (double sampleRate, LatencyMode mode) noexcept;
    double getSampleRate() const noexcept { return fs; }

    void setParams (const EngineParams& p) noexcept;
    void noteOn (int note) noexcept;
    void noteOff (int note) noexcept;
    void allNotesOff() noexcept;

    /** In-place processing. channels.size() must be <= the prepared channel count. */
    void process (float* const* channels, int numChannels, int numSamples) noexcept;

    // --- UI feeds -------------------------------------------------------------
    SpscFifo<MeterFrame, 512> meterFifo;

    void requestLearn() noexcept { learnRequest.store (true); }
    void requestAutoKey() noexcept { keyRequest.store (true); }
    std::atomic<int> learnedInputType { -1 };   // consumed (reset to -1) by the plugin
    std::atomic<int> detectedKey { -1 };        // key * 2 + minor, consumed by the plugin
    std::atomic<float> learnProgress { -1.0f }; // -1 idle, 0..1 listening
    std::atomic<float> keyProgress { -1.0f };

private:
    void onEstimate() noexcept;
    uint16_t heldMask() const noexcept;

    double fs = 44100.0;
    int numCh = 1, latency = 0, maxPeriod = 1470;
    LatencyMode latencyMode = LatencyMode::Low;
    int64_t now = 0;

    PitchDetector detector;
    ScaleMapper mapper;
    RetuneEngine retune;
    PsolaShifter psola;
    ClassicShifter classic;
    RangeLearner learner;
    KeyDetector keyDetector;

    std::array<HistoryBuffer, kMaxChannels> rings;
    HistoryBuffer markRing;
    Biquad markLp1, markLp2;

    EngineParams params;
    EngineMode activeEngine = EngineMode::Modern;
    std::array<int, 128> heldNotes {};

    // per-hop state
    float ratio = 1.0f, period = 400.0f, formantFactor = 1.0f;
    bool voiced = false;
    float classicRatio = 1.0f, classicCoeff = 0.99f;

    LinearSmoother voicedGate, mixGain, activeGain, engineGain;

    // learn / key
    std::atomic<bool> learnRequest { false }, keyRequest { false };
    bool learning = false, keyListening = false;
    float learnElapsed = 0.0f, keyElapsed = 0.0f, hopSeconds = 0.0025f;
};

} // namespace ac
