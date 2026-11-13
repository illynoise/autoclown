#include "ScaleMapper.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

namespace ac
{
namespace
{
    constexpr uint16_t bits (std::initializer_list<int> degrees)
    {
        uint16_t m = 0;
        for (int d : degrees) m = (uint16_t) (m | (1u << d));
        return m;
    }

    constexpr uint16_t kMasks[] = {
        0x0FFF,                              // Chromatic
        bits ({ 0, 2, 4, 5, 7, 9, 11 }),     // Major (Ionian)
        bits ({ 0, 2, 3, 5, 7, 8, 10 }),     // Natural minor (Aeolian)
        bits ({ 0, 2, 3, 5, 7, 8, 11 }),     // Harmonic minor
        bits ({ 0, 2, 3, 5, 7, 9, 11 }),     // Melodic minor (ascending)
        bits ({ 0, 2, 3, 5, 7, 9, 10 }),     // Dorian
        bits ({ 0, 1, 3, 5, 7, 8, 10 }),     // Phrygian
        bits ({ 0, 2, 4, 6, 7, 9, 11 }),     // Lydian
        bits ({ 0, 2, 4, 5, 7, 9, 10 }),     // Mixolydian
        bits ({ 0, 1, 3, 5, 6, 8, 10 }),     // Locrian
        bits ({ 0, 2, 4, 7, 9 }),            // Major pentatonic
        bits ({ 0, 3, 5, 7, 10 }),           // Minor pentatonic
        bits ({ 0, 3, 5, 6, 7, 10 }),        // Blues
    };

    constexpr const char* kScaleNames[] = {
        "Chromatic", "Major", "Minor", "Harmonic Minor", "Melodic Minor",
        "Dorian", "Phrygian", "Lydian", "Mixolydian", "Locrian",
        "Major Pentatonic", "Minor Pentatonic", "Blues",
    };

    constexpr const char* kPcNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
} // namespace

const char* scaleName (ScaleType s) noexcept
{
    const int i = std::clamp ((int) s, 0, (int) ScaleType::NumScales - 1);
    return kScaleNames[i];
}

const char* pitchClassName (int pc) noexcept { return kPcNames[((pc % 12) + 12) % 12]; }

uint16_t scaleMask (ScaleType s) noexcept
{
    const int i = std::clamp ((int) s, 0, (int) ScaleType::NumScales - 1);
    return kMasks[i];
}

void ScaleMapper::configure (int key, ScaleType scale, const std::array<NoteMode, 12>& modes,
                             uint16_t midiHeldMask) noexcept
{
    key = ((key % 12) + 12) % 12;
    const uint16_t mask = scaleMask (scale);

    anyCandidate = false;
    for (int pc = 0; pc < 12; ++pc)
    {
        const int degree = (pc - key + 12) % 12;
        const bool inScale = (mask >> degree) & 1u;
        NoteState st = inScale ? NoteState::Target : NoteState::Remove;

        switch (modes[(size_t) pc])
        {
            case NoteMode::Scale:  break;
            case NoteMode::Target: st = NoteState::Target; break;
            case NoteMode::Bypass: st = NoteState::Bypass; break;
            case NoteMode::Remove: st = NoteState::Remove; break;
        }

        if (midiHeldMask != 0)
        {
            const bool held = (midiHeldMask >> pc) & 1u;
            if (modes[(size_t) pc] == NoteMode::Remove || ! held) st = NoteState::Remove;
            else if (modes[(size_t) pc] != NoteMode::Bypass) st = NoteState::Target;
        }

        states[(size_t) pc] = st;
        anyCandidate = anyCandidate || st != NoteState::Remove;
    }
}

uint16_t ScaleMapper::targetMask() const noexcept
{
    uint16_t m = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (states[(size_t) pc] == NoteState::Target) m = (uint16_t) (m | (1u << pc));
    return m;
}

NoteChoice ScaleMapper::nearest (float midi) const noexcept
{
    NoteChoice c;
    if (! anyCandidate || ! std::isfinite (midi)) return c;

    const int centre = (int) std::lround (midi);
    float bestDist = 1.0e9f;
    for (int off = 0; off <= 12; ++off)
    {
        for (int sign : { -1, 1 })
        {
            const int n = centre + sign * off;
            if (! isCandidate (n)) continue;
            const float d = std::abs (midi - (float) n);
            if (d < bestDist) { bestDist = d; c.note = n; }
            if (off == 0) break;
        }
        if (c.note >= 0 && (float) off > bestDist + 1.0f) break;
    }
    c.valid = bestDist < 1.0e8f;
    if (c.valid) c.bypass = stateOf (c.note) == NoteState::Bypass;
    return c;
}

NoteChoice ScaleMapper::choose (float midi, int currentNote, float hysteresis) const noexcept
{
    NoteChoice best = nearest (midi);
    if (! best.valid || currentNote < 0 || best.note == currentNote || ! isCandidate (currentNote))
        return best;

    const float dBest = std::abs (midi - (float) best.note);
    const float dCur = std::abs (midi - (float) currentNote);
    if (dCur - dBest > hysteresis) return best;

    NoteChoice keep;
    keep.note = currentNote;
    keep.valid = true;
    keep.bypass = stateOf (currentNote) == NoteState::Bypass;
    return keep;
}

} // namespace ac
