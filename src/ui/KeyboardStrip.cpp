#include "KeyboardStrip.h"

#include "CircusLookAndFeel.h"
#include "plugin/PluginProcessor.h"

using namespace juce;

KeyboardStrip::KeyboardStrip (AutoClownProcessor& p) : proc (p)
{
    setTitle ("Note keyboard");
    setTooltip ("Click a note to cycle Target -> Bypass -> Remove. Right-click for more. "
                "With Latch off, changes only last while the mouse is held.");
    refreshStates();
}

void KeyboardStrip::refreshStates()
{
    auto params = ParameterReader (proc.apvts).snapshot();
    ac::ScaleMapper m;
    m.configure (params.key, params.scale, params.noteModes);
    for (int pc = 0; pc < 12; ++pc)
    {
        resolved[(size_t) pc] = (int) m.stateOf (pc);
        explicitMode[(size_t) pc] = (int) params.noteModes[(size_t) pc];
    }
    repaint();
}

void KeyboardStrip::setLive (int target, uint16_t held, bool voiced)
{
    if (target != liveTarget || held != heldMask || voiced != liveVoiced)
    {
        liveTarget = target; heldMask = held; liveVoiced = voiced;
        repaint();
    }
}

Rectangle<float> KeyboardStrip::keyBounds (int pc) const
{
    static constexpr int whiteIndex[12] = { 0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6 };
    const auto r = getLocalBounds().toFloat();
    const float ww = r.getWidth() / 7.0f;
    if (! isBlack (pc)) return { r.getX() + ww * (float) whiteIndex[pc], r.getY(), ww, r.getHeight() };
    const float bw = ww * 0.6f;
    const float x = r.getX() + ww * (float) (whiteIndex[pc - 1] + 1) - bw * 0.5f;
    return { x, r.getY(), bw, r.getHeight() * 0.6f };
}

int KeyboardStrip::pitchClassAt (Point<float> p) const
{
    for (int pc : { 1, 3, 6, 8, 10 }) if (keyBounds (pc).contains (p)) return pc;
    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) if (keyBounds (pc).contains (p)) return pc;
    return -1;
}

void KeyboardStrip::paint (Graphics& g)
{
    using namespace Circus;
    const auto r = getLocalBounds().toFloat();
    const float corner = r.getHeight() * 0.08f;
    g.setColour (charcoal);
    g.fillRoundedRectangle (r, corner);

    auto drawKey = [&] (int pc)
    {
        const auto k = keyBounds (pc).reduced (r.getHeight() * 0.025f);
        const bool black = isBlack (pc);
        const int st = resolved[(size_t) pc];
        Colour base = st == 0 ? gold : (st == 1 ? blue : red);
        if (black) base = base.darker (0.35f);
        if (st == 0 && ! black) base = base.interpolatedWith (cream, 0.25f);

        g.setGradientFill (ColourGradient (base.brighter (0.2f), k.getX(), k.getY(), base.darker (0.15f), k.getX(), k.getBottom(), false));
        g.fillRoundedRectangle (k, corner);

        const bool live = liveVoiced && liveTarget >= 0 && liveTarget % 12 == pc;
        if (live)
        {
            g.setColour (cream);
            g.drawRoundedRectangle (k.reduced (1.0f), corner, r.getHeight() * 0.06f);
        }
        if (momentaryPc == pc)
        {
            g.setColour (charcoal.withAlpha (0.25f));
            g.fillRoundedRectangle (k, corner);
        }

        // Explicit override marker (small star) vs. following the scale
        if (explicitMode[(size_t) pc] != 0)
        {
            Path star;
            const float s = k.getWidth() * (black ? 0.16f : 0.09f);
            star.addStar ({ k.getCentreX(), k.getY() + s * 1.6f }, 5, s * 0.45f, s);
            g.setColour (black ? cream : charcoal);
            g.fillPath (star);
        }
        // MIDI held dot
        if ((heldMask >> pc) & 1)
        {
            const float d = k.getWidth() * (black ? 0.3f : 0.16f);
            g.setColour (black ? cream : purple);
            g.fillEllipse (Rectangle<float> (d, d).withCentre ({ k.getCentreX(), k.getY() + k.getHeight() * (black ? 0.45f : 0.42f) }));
        }

        const auto textCol = st == 0 ? charcoal : cream;
        g.setColour (black && st == 0 ? cream : textCol);
        g.setFont (display (k.getHeight() * (black ? 0.22f : 0.2f)));
        auto label = k.withTrimmedTop (k.getHeight() * (black ? 0.55f : 0.55f));
        g.drawFittedText (ac::pitchClassName (pc), label.removeFromTop (label.getHeight() * 0.55f).toNearestInt(), Justification::centred, 1);
        g.setFont (sans (k.getHeight() * 0.13f, true));
        if (! black)
            g.drawFittedText (st == 0 ? "TARGET" : (st == 1 ? "BYPASS" : "REMOVED"), label.toNearestInt(), Justification::centred, 1);
    };

    for (int pc : { 0, 2, 4, 5, 7, 9, 11 }) drawKey (pc);
    for (int pc : { 1, 3, 6, 8, 10 }) drawKey (pc);
}

void KeyboardStrip::setNoteMode (int pc, int mode)
{
    if (auto* p = proc.apvts.getParameter (ParamIDs::note (pc)))
    {
        proc.undoManager.beginNewTransaction ("Keyboard");
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) mode));
        p->endChangeGesture();
    }
}

void KeyboardStrip::mouseDown (const MouseEvent& e)
{
    const int pc = pitchClassAt (e.position);
    if (pc < 0) return;

    // Cycle the resolved state: target(0) -> bypass(1) -> remove(2) -> target
    const int next = (resolved[(size_t) pc] + 1) % 3;
    const int nextMode = next == 0 ? 1 : (next == 1 ? 2 : 3); // NoteMode: Target=1, Bypass=2, Remove=3

    if (e.mods.isPopupMenu())
    {
        PopupMenu m;
        m.addSectionHeader (String ("Note ") + ac::pitchClassName (pc));
        m.addItem (1, "Follow key/scale", true, explicitMode[(size_t) pc] == 0);
        m.addItem (2, "Target", true, explicitMode[(size_t) pc] == 1);
        m.addItem (3, "Bypass (no correction near this note)", true, explicitMode[(size_t) pc] == 2);
        m.addItem (4, "Remove (never a target)", true, explicitMode[(size_t) pc] == 3);
        m.addSeparator();
        m.addItem (5, "Reset all notes to key/scale");
        m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                         [safe = SafePointer<KeyboardStrip> (this), pc] (int r)
                         {
                             if (safe == nullptr || r == 0) return;
                             if (r == 5) { for (int i = 0; i < 12; ++i) safe->setNoteMode (i, 0); }
                             else safe->setNoteMode (pc, r - 1);
                             safe->refreshStates();
                         });
        return;
    }

    const bool latch = proc.apvts.getRawParameterValue (ParamIDs::latch)->load() > 0.5f;
    if (latch)
    {
        setNoteMode (pc, nextMode);
        refreshStates();
    }
    else
    {
        momentaryPc = pc;
        proc.setMomentaryNote (pc, nextMode);
        resolved[(size_t) pc] = next;
        repaint();
    }
}

void KeyboardStrip::mouseUp (const MouseEvent&)
{
    if (momentaryPc >= 0)
    {
        proc.setMomentaryNote (momentaryPc, -1);
        momentaryPc = -1;
        refreshStates();
    }
}
