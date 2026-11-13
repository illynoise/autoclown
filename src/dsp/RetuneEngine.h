// Auto Clown — turns a detected pitch track into an output pitch track.
//
// Runs once per detector hop (~2.5 ms). Pipeline:
//
//   detected pitch p (MIDI, relative to the Detune reference)
//     -> vibrato split:  centre = 2-pole critically-damped LP (~3 Hz) of p
//                        p' = centre + g * (p - centre), g from Natural Vibrato (dB)
//     -> note choice:    ScaleMapper with hysteresis on p'
//     -> Flex Tune:      correction strength s(|p' - note|) fades from 1 to 0 between an
//                        inner and outer window (narrower as Flex rises) so scoops/bends
//                        far from the note pass through untouched
//     -> retune glide:   the *correction offset* c = s * (note - p') is smoothed with a
//                        one-pole whose time constant is Retune Speed, lengthened by
//                        Humanize as the note is sustained. Smoothing the offset (not the
//                        absolute pitch) means a slow retune keeps the singer's natural
//                        movement while still pulling towards the note; 0 ms = hard snap.
//     -> output pitch:   p' + c + transpose   (or held value while Hold is on)
//
// Output is a pitch ratio for the shifter = 2^((out - p) / 12).
#pragma once

#include "ScaleMapper.h"

namespace ac
{
struct RetuneSettings
{
    float retuneMs = 20.0f;     // 0..400
    float flex = 0.0f;          // 0..1
    float humanize = 0.0f;      // 0..1
    float vibratoDb = 0.0f;     // -12..+12
    float transpose = 0.0f;     // semitones
    bool hold = false;
};

struct RetuneResult
{
    float ratio = 1.0f;           // output/input frequency ratio (1 when unvoiced)
    float inputMidi = 0.0f;       // detected pitch
    float outputMidi = 0.0f;      // corrected pitch (incl. transpose)
    int targetNote = -1;          // chosen note (-1 none)
    bool bypassNote = false;      // nearest note is a Bypass note
    float correctionCents = 0.0f; // applied correction excluding transpose
    float deviationCents = 0.0f;  // singer's deviation from the chosen note
    bool voiced = false;
};

class RetuneEngine
{
public:
    void prepare (float updateRateHz) noexcept { rate = updateRateHz; reset(); }
    void reset() noexcept;

    RetuneResult update (bool voiced, float detectedMidi, const ScaleMapper& mapper,
                         const RetuneSettings& settings) noexcept;

    /** Hysteresis in semitones used when switching notes. */
    static constexpr float kHysteresis = 0.18f;

private:
    float rate = 400.0f;
    bool wasVoiced = false;
    float centre1 = 0.0f, centre2 = 0.0f; // 2-pole vibrato centre tracker
    float correction = 0.0f;
    int currentNote = -1;
    float noteAge = 0.0f;
    bool holdActive = false;
    float heldOutput = 0.0f;
    float lastOutput = 0.0f;
};

} // namespace ac
