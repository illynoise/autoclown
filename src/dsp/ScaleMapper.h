// Auto Clown — key/scale -> allowed target notes, with hysteresis.
//
// Each of the 12 pitch classes resolves to one of:
//   Target : a legal correction target
//   Bypass : the singer is left alone when nearest to this note (still "catches" the
//            singer so neighbouring notes don't grab them)
//   Remove : never a target; the singer snaps to the neighbours instead
// The per-note keyboard mode "Scale" defers to the selected key/scale.
// When MIDI targeting is active and notes are held, the held pitch classes become the
// only targets (keyboard Remove still wins, so the user can veto notes).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ac
{
enum class NoteMode : uint8_t { Scale = 0, Target, Bypass, Remove };
enum class NoteState : uint8_t { Target = 0, Bypass, Remove };

enum class ScaleType : int
{
    Chromatic = 0, Major, Minor, HarmonicMinor, MelodicMinor,
    Dorian, Phrygian, Lydian, Mixolydian, Locrian,
    MajorPentatonic, MinorPentatonic, Blues,
    NumScales
};

const char* scaleName (ScaleType s) noexcept;
const char* pitchClassName (int pc) noexcept; // "C", "C#", ...
/** 12-bit mask of the scale's pitch classes relative to its root (bit 0 = root). */
uint16_t scaleMask (ScaleType s) noexcept;

struct NoteChoice
{
    int note = -1;          // MIDI note number of the chosen target (or bypass) note
    bool bypass = false;    // nearest note is a Bypass note -> do not correct
    bool valid = false;     // false when no note is available at all
};

class ScaleMapper
{
public:
    /** Rebuilds the 12 resolved states. Cheap and real-time safe. */
    void configure (int key, ScaleType scale, const std::array<NoteMode, 12>& modes,
                    uint16_t midiHeldMask = 0) noexcept;

    NoteState stateOf (int pitchClass) const noexcept { return states[(size_t) (((pitchClass % 12) + 12) % 12)]; }
    uint16_t targetMask() const noexcept;

    /** Nearest candidate (Target or Bypass) to a fractional MIDI pitch, ignoring hysteresis. */
    NoteChoice nearest (float midi) const noexcept;

    /**
        Nearest candidate with hysteresis: a different note only takes over when it is
        closer than the current note by more than `hysteresisSemis`.
    */
    NoteChoice choose (float midi, int currentNote, float hysteresisSemis) const noexcept;

private:
    bool isCandidate (int note) const noexcept { return stateOf (note) != NoteState::Remove; }
    std::array<NoteState, 12> states {};
    bool anyCandidate = true;
};

} // namespace ac
