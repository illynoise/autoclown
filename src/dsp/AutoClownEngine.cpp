#include "AutoClownEngine.h"

namespace ac
{
namespace
{
    constexpr float kLearnSeconds = 4.0f;   // voiced audio needed for "Learn"
    constexpr float kLearnTimeout = 20.0f;
    constexpr float kKeySeconds = 8.0f;     // voiced audio needed for "Auto Key"
    constexpr float kKeyTimeout = 45.0f;
} // namespace

int AutoClownEngine::latencyFor (double sampleRate, LatencyMode mode) noexcept
{
    // Low: 5 ms (well under the 10 ms target). High quality: 30 ms, which covers two
    // periods down to ~67 Hz so PSOLA grains are always perfectly time-aligned.
    const double seconds = mode == LatencyMode::Low ? 0.005 : 0.030;
    return (int) std::lround (seconds * sampleRate);
}

void AutoClownEngine::prepare (double sampleRate, int /*maxBlockSize*/, int numChannels, LatencyMode mode)
{
    fs = sampleRate;
    numCh = std::clamp (numChannels, 1, kMaxChannels);
    latencyMode = mode;
    latency = latencyFor (fs, mode);
    maxPeriod = (int) std::ceil (fs / PitchDetector::kAbsMinHz);

    detector.prepare (fs);
    retune.prepare ((float) (fs / detector.getHopSize()));
    hopSeconds = (float) (detector.getHopSize() / fs);
    psola.prepare (fs, maxPeriod);
    classic.prepare (fs);

    const int ringSize = latencyFor (fs, LatencyMode::HighQuality) + 8 * maxPeriod + 1024;
    for (auto& r : rings) r.allocate (ringSize);
    markRing.allocate (ringSize);
    markLp1.setLowpass (fs, 900.0, 0.5412);
    markLp2.setLowpass (fs, 900.0, 1.3066);

    voicedGate.prepare (fs, 0.006f);
    mixGain.prepare (fs, 0.02f);
    activeGain.prepare (fs, 0.02f);
    engineGain.prepare (fs, 0.008f);
    classicCoeff = std::exp (-1.0f / (0.002f * (float) fs));

    reset();
}

void AutoClownEngine::setLatencyMode (LatencyMode mode)
{
    latencyMode = mode;
    latency = latencyFor (fs, mode);
    reset();
}

void AutoClownEngine::reset()
{
    now = 0;
    for (auto& r : rings) r.clear();
    markRing.clear();
    markLp1.reset(); markLp2.reset();
    detector.reset();
    retune.reset();
    psola.reset (0);
    classic.reset();
    ratio = classicRatio = formantFactor = 1.0f;
    period = (float) (fs / 200.0);
    voiced = false;
    voicedGate.snap (0.0f);
    mixGain.snap (params.mix);
    activeGain.snap (params.bypass ? 0.0f : 1.0f);
    engineGain.snap (1.0f);
    activeEngine = params.engine;
    learning = keyListening = false;
    learnProgress.store (-1.0f);
    keyProgress.store (-1.0f);
}

void AutoClownEngine::setParams (const EngineParams& p) noexcept
{
    params = p;
    detector.setTracking (p.tracking);
    if (! learning)
    {
        const auto r = inputTypeRange (p.inputType);
        detector.setRange (r.lo, r.hi);
    }
    mapper.configure (p.key, p.scale, p.noteModes, p.midiEnabled ? heldMask() : (uint16_t) 0);
    mixGain.setTarget (std::clamp (p.mix, 0.0f, 1.0f));
    activeGain.setTarget (p.bypass ? 0.0f : 1.0f);
}

uint16_t AutoClownEngine::heldMask() const noexcept
{
    uint16_t m = 0;
    for (int n = 0; n < 128; ++n)
        if (heldNotes[(size_t) n] > 0) m = (uint16_t) (m | (1u << (n % 12)));
    return m;
}

void AutoClownEngine::noteOn (int note) noexcept
{
    if (note < 0 || note > 127) return;
    ++heldNotes[(size_t) note];
    if (params.midiEnabled) mapper.configure (params.key, params.scale, params.noteModes, heldMask());
}

void AutoClownEngine::noteOff (int note) noexcept
{
    if (note < 0 || note > 127) return;
    heldNotes[(size_t) note] = std::max (0, heldNotes[(size_t) note] - 1);
    if (params.midiEnabled) mapper.configure (params.key, params.scale, params.noteModes, heldMask());
}

void AutoClownEngine::allNotesOff() noexcept
{
    heldNotes.fill (0);
    mapper.configure (params.key, params.scale, params.noteModes, 0);
}

void AutoClownEngine::onEstimate() noexcept
{
    const PitchEstimate& est = detector.getEstimate();

    // --- Learn ------------------------------------------------------------------
    if (learnRequest.exchange (false))
    {
        learning = true;
        learnElapsed = 0.0f;
        learner.reset();
        detector.setRange (50.0f, 1600.0f); // listen wide while learning
    }
    if (learning)
    {
        learnElapsed += hopSeconds;
        if (est.voiced) learner.add (est.hz, hopSeconds);
        learnProgress.store (std::min (1.0f, learner.totalWeight() / kLearnSeconds));
        const int decision = learner.decide (kLearnSeconds);
        if (decision >= 0 || learnElapsed > kLearnTimeout)
        {
            learning = false;
            learnProgress.store (-1.0f);
            if (decision >= 0) learnedInputType.store (decision);
            const auto r = inputTypeRange (decision >= 0 ? (InputType) decision : params.inputType);
            detector.setRange (r.lo, r.hi);
        }
    }

    // --- Auto Key ---------------------------------------------------------------
    if (keyRequest.exchange (false))
    {
        keyListening = true;
        keyElapsed = 0.0f;
        keyDetector.reset();
    }
    if (keyListening)
    {
        keyElapsed += hopSeconds;
        if (est.voiced)
            keyDetector.add (hzToMidi (est.hz, params.refHz), hopSeconds * (1.0f - est.aperiodicity));
        keyProgress.store (std::min (1.0f, keyDetector.totalWeight() / kKeySeconds));
        if (keyDetector.totalWeight() >= kKeySeconds || keyElapsed > kKeyTimeout)
        {
            keyListening = false;
            keyProgress.store (-1.0f);
            const auto r = keyDetector.estimate();
            if (r.key >= 0 && keyDetector.totalWeight() > 1.0f)
                detectedKey.store (r.key * 2 + (r.minor ? 1 : 0));
        }
    }

    // --- Correction ---------------------------------------------------------------
    const float midiIn = est.hz > 0.0f ? hzToMidi (est.hz, params.refHz) : 0.0f;
    const RetuneResult rr = retune.update (est.voiced, midiIn, mapper, params.retune);

    voiced = rr.voiced;
    ratio = rr.ratio;
    if (est.voiced) period = (float) (fs / est.hz);
    const float amount = params.formantOn ? std::clamp (params.formantAmount, 0.0f, 1.0f) : 0.0f;
    formantFactor = std::pow (ratio, 1.0f - amount);
    voicedGate.setTarget (voiced ? 1.0f : 0.0f);

    MeterFrame f;
    f.inputHz = est.voiced ? est.hz : 0.0f;
    f.inputMidi = rr.inputMidi;
    f.outputMidi = rr.outputMidi;
    f.correctionCents = rr.correctionCents;
    f.deviationCents = rr.deviationCents;
    f.levelDb = est.rmsDb;
    f.targetNote = (int16_t) rr.targetNote;
    f.targetMask = mapper.targetMask();
    f.heldMidiMask = heldMask();
    f.voiced = rr.voiced ? 1 : 0;
    f.bypassNote = rr.bypassNote ? 1 : 0;
    meterFifo.push (f);
}

void AutoClownEngine::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    numChannels = std::clamp (numChannels, 1, numCh);
    const HistoryBuffer* in[kMaxChannels] = { &rings[0], &rings[1] };
    const float invCh = 1.0f / (float) numChannels;

    for (int i = 0; i < numSamples; ++i)
    {
        float mono = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float x = channels[ch][i];
            rings[(size_t) ch].write (now, x);
            mono += x;
        }
        mono *= invCh;
        markRing.write (now, markLp2.process (markLp1.process (mono)));

        if (detector.pushSample (mono)) onEstimate();

        // Engine switch: fade the wet path out, swap, fade back in.
        if (params.engine != activeEngine)
        {
            engineGain.setTarget (0.0f);
            if (engineGain.get() <= 0.0f)
            {
                activeEngine = params.engine;
                psola.reset (now);
                classic.reset();
                engineGain.setTarget (1.0f);
            }
        }

        float wet[kMaxChannels] = { 0.0f, 0.0f };
        if (activeEngine == EngineMode::Modern)
        {
            psola.process (now, latency, markRing, in, numChannels, wet, period, voiced, ratio, formantFactor);
        }
        else
        {
            classicRatio = ratio + classicCoeff * (classicRatio - ratio);
            classic.process (now, latency, in, numChannels, wet, period, classicRatio);
        }

        const float g = voicedGate.next() * engineGain.next() * mixGain.next() * activeGain.next();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float dry = rings[(size_t) ch].read (now - latency);
            channels[ch][i] = dry + g * (wet[ch] - dry);
        }
        ++now;
    }
}

} // namespace ac
