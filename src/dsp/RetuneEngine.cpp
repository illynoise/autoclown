#include "RetuneEngine.h"

#include "DspUtils.h"

namespace ac
{
void RetuneEngine::reset() noexcept
{
    wasVoiced = false;
    centre1 = centre2 = 0.0f;
    correction = 0.0f;
    currentNote = -1;
    noteAge = 0.0f;
    holdActive = false;
    heldOutput = lastOutput = 0.0f;
}

RetuneResult RetuneEngine::update (bool voiced, float p, const ScaleMapper& mapper,
                                   const RetuneSettings& s) noexcept
{
    RetuneResult r;
    r.voiced = voiced;
    r.inputMidi = p;
    const float dt = 1.0f / rate;

    if (! voiced || ! std::isfinite (p))
    {
        wasVoiced = false;
        currentNote = -1;
        noteAge = 0.0f;
        // Hold survives short gaps so a frozen note can be re-entered.
        holdActive = holdActive && s.hold;
        r.ratio = 1.0f;
        r.inputMidi = 0.0f;
        return r;
    }

    // --- vibrato split -------------------------------------------------------
    const float fc = 3.0f; // Hz: below typical vibrato rates (4.5–7 Hz)
    const float a = 1.0f - std::exp (-2.0f * kPi * fc * dt);
    if (! wasVoiced) { centre1 = centre2 = p; }
    centre1 += a * (p - centre1);
    centre2 += a * (centre1 - centre2);
    const float vibGain = dbToGain (std::clamp (s.vibratoDb, -12.0f, 12.0f));
    // Clamp the modulation so huge gains on fast glides can't run away.
    const float pv = centre2 + std::clamp (vibGain * (p - centre2), -3.0f, 3.0f);

    // --- note choice ---------------------------------------------------------
    const NoteChoice choice = mapper.choose (pv, currentNote, kHysteresis);
    float target = pv;
    if (choice.valid)
    {
        if (choice.note != currentNote) { currentNote = choice.note; noteAge = 0.0f; }
        else noteAge += dt;
        r.targetNote = choice.note;
        r.bypassNote = choice.bypass;
        r.deviationCents = (pv - (float) choice.note) * 100.0f;

        if (! choice.bypass)
        {
            const float dev = pv - (float) choice.note;
            float strength = 1.0f;
            if (s.flex > 0.0f)
            {
                // Flex 0+ -> outer 60 cents, Flex 1 -> outer 12 cents.
                const float outer = (0.60f - 0.48f * s.flex);
                const float inner = outer * 0.5f;
                strength = std::clamp (1.0f - (std::abs (dev) - inner) / (outer - inner), 0.0f, 1.0f);
            }
            target = pv - dev * strength;
        }
    }
    else
    {
        currentNote = -1;
    }

    // --- retune glide (on the correction offset) -------------------------------
    const float desired = target - pv;
    float tau = std::clamp (s.retuneMs, 0.0f, 400.0f) * 0.001f;
    if (s.humanize > 0.0f)
    {
        // Short notes (<120 ms) keep the base speed; sustained notes relax over ~400 ms.
        const float sustain = std::clamp ((noteAge - 0.12f) / 0.4f, 0.0f, 1.0f);
        tau += s.humanize * sustain * std::max (3.0f * tau, 0.15f);
    }

    if (! wasVoiced || tau <= 1.0e-4f)
        correction = desired; // onsets start in tune; 0 ms is a hard snap
    else
        correction += (1.0f - std::exp (-dt / tau)) * (desired - correction);

    float out = pv + correction + s.transpose;

    // --- hold / freeze --------------------------------------------------------
    if (s.hold)
    {
        if (! holdActive) { holdActive = true; heldOutput = wasVoiced ? lastOutput : out; }
        out = heldOutput;
    }
    else
    {
        holdActive = false;
    }

    wasVoiced = true;
    lastOutput = out;

    r.outputMidi = out;
    r.correctionCents = (out - s.transpose - p) * 100.0f;
    r.ratio = std::clamp (semitonesToRatio (out - p), 0.25f, 4.0f);
    return r;
}

} // namespace ac
