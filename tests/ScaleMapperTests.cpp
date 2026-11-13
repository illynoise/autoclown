#include "TestFramework.h"

#include "dsp/ScaleMapper.h"

using namespace ac;

namespace
{
std::array<NoteMode, 12> follow() { std::array<NoteMode, 12> m {}; m.fill (NoteMode::Scale); return m; }
}

TEST_CASE ("scale: every scale mask has the right size and root")
{
    const int sizes[] = { 12, 7, 7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 6 };
    for (int s = 0; s < (int) ScaleType::NumScales; ++s)
    {
        const auto m = scaleMask ((ScaleType) s);
        int bitsSet = 0;
        for (int i = 0; i < 12; ++i) bitsSet += (m >> i) & 1;
        CHECK (bitsSet == sizes[s]);
        CHECK ((m & 1) == 1);
    }
}

TEST_CASE ("scale: C major snaps to nearest scale note")
{
    ScaleMapper m;
    m.configure (0, ScaleType::Major, follow());
    CHECK (m.nearest (60.2f).note == 60); // C
    CHECK (m.nearest (60.9f).note == 60); // C# region -> C (closer than D)
    CHECK (m.nearest (61.4f).note == 62); // -> D
    CHECK (m.nearest (61.6f).note == 62); // -> D
    CHECK (m.nearest (65.9f).note == 65); // F#(66) is out of C major: 65.9 -> F
    CHECK (m.nearest (66.6f).note == 67); // -> G
    CHECK (m.nearest (69.9f).note == 69); // A# out -> A
    CHECK (m.nearest (70.4f).note == 71); // -> B
    CHECK (m.nearest (35.2f).note == 35); // B1 (any octave)
}

TEST_CASE ("scale: key rotation (D major contains F# and C#)")
{
    ScaleMapper m;
    m.configure (2, ScaleType::Major, follow());
    CHECK (m.stateOf (6) == NoteState::Target);  // F#
    CHECK (m.stateOf (1) == NoteState::Target);  // C#
    CHECK (m.stateOf (5) == NoteState::Remove);  // F
    CHECK (m.stateOf (0) == NoteState::Remove);  // C
    CHECK (m.nearest (65.2f).note == 66);
}

TEST_CASE ("scale: A minor pentatonic and blues")
{
    ScaleMapper m;
    m.configure (9, ScaleType::MinorPentatonic, follow()); // A C D E G
    CHECK (m.targetMask() == ((1 << 9) | (1 << 0) | (1 << 2) | (1 << 4) | (1 << 7)));
    m.configure (9, ScaleType::Blues, follow());           // A C D D# E G
    CHECK (m.stateOf (3) == NoteState::Target);
}

TEST_CASE ("scale: hysteresis prevents flicker at the midpoint")
{
    ScaleMapper m;
    m.configure (0, ScaleType::Chromatic, follow());
    const float hyst = 0.18f;
    int note = m.choose (60.0f, -1, hyst).note;
    CHECK (note == 60);
    // Wobble around the 60/61 boundary: must stay on 60.
    for (float p : { 60.45f, 60.55f, 60.58f, 60.5f, 60.57f, 60.4f })
    {
        note = m.choose (p, note, hyst).note;
        CHECK (note == 60);
    }
    // Clearly past the boundary + hysteresis: switches.
    note = m.choose (60.7f, note, hyst).note;
    CHECK (note == 61);
    // And coming back needs the same margin.
    note = m.choose (60.42f, note, hyst).note;
    CHECK (note == 61);
    note = m.choose (60.3f, note, hyst).note;
    CHECK (note == 60);
}

TEST_CASE ("scale: keyboard Remove and Bypass")
{
    auto modes = follow();
    modes[4] = NoteMode::Remove; // remove E from C major
    modes[7] = NoteMode::Bypass; // bypass G
    modes[6] = NoteMode::Target; // add F#
    ScaleMapper m;
    m.configure (0, ScaleType::Major, modes);
    CHECK (m.nearest (64.0f).note != 64);               // E never a target
    CHECK (m.nearest (63.4f).note == 62);               // D (E removed; D# out)
    CHECK (m.nearest (64.6f).note == 65);               // F
    const auto g = m.nearest (67.1f);
    CHECK (g.note == 67 && g.bypass);                   // near G -> leave alone
    CHECK (m.nearest (66.2f).note == 66);               // F# added
    CHECK (! m.nearest (66.2f).bypass);
}

TEST_CASE ("scale: MIDI held notes define targets, keyboard Remove still vetoes")
{
    auto modes = follow();
    ScaleMapper m;
    const uint16_t held = (1 << 0) | (1 << 4) | (1 << 7); // C E G held
    m.configure (0, ScaleType::Chromatic, modes, held);
    CHECK (m.targetMask() == held);
    CHECK (m.nearest (62.0f).note == 60 || m.nearest (62.0f).note == 64);
    CHECK (m.nearest (65.4f).note == 64);
    modes[4] = NoteMode::Remove;
    m.configure (0, ScaleType::Chromatic, modes, held);
    CHECK (m.nearest (64.2f).note == 67 || m.nearest (64.2f).note == 60);
    CHECK (m.nearest (64.2f).note != 64);
}

TEST_CASE ("scale: everything removed -> no valid target")
{
    auto modes = follow();
    modes.fill (NoteMode::Remove);
    ScaleMapper m;
    m.configure (0, ScaleType::Major, modes);
    CHECK (! m.nearest (60.0f).valid);
}
