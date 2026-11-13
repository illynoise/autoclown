#include "BigTopMeter.h"

#include "CircusLookAndFeel.h"

using namespace juce;

void BigTopMeter::update (const ac::MeterFrame* latest, float dt)
{
    if (latest != nullptr) frame = *latest;
    const bool voiced = frame.voiced != 0;
    // Needle ballistics: fast attack, gentle release, ~frame-rate independent.
    const float k = 1.0f - std::exp (-dt / 0.045f);
    needleCents += k * ((voiced ? jlimit (-100.0f, 100.0f, frame.correctionCents) : 0.0f) - needleCents);
    devCents += k * ((voiced ? jlimit (-100.0f, 100.0f, frame.deviationCents) : 0.0f) - devCents);
    glow += (1.0f - std::exp (-dt / 0.08f)) * ((voiced ? 1.0f : 0.0f) - glow);

    // Keep showing the last note briefly after the singer stops, dimmed.
    if (voiced && frame.targetNote >= 0) { heldNote = frame.targetNote; holdTimer = 0.6f; }
    else holdTimer = jmax (0.0f, holdTimer - dt);
    if (holdTimer <= 0.0f && ! voiced) heldNote = -1;
    repaint();
}

void BigTopMeter::paint (Graphics& g)
{
    using namespace Circus;
    const auto bounds = getLocalBounds().toFloat();
    const float R = jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 2.0f;
    const auto c = bounds.getCentre();
    const float twoPi = MathConstants<float>::twoPi;

    // Shadow
    g.setColour (charcoal.withAlpha (0.55f));
    g.fillEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c.translated (0.0f, R * 0.03f)));

    // Striped tent rim (wedges)
    const int wedges = 32;
    for (int i = 0; i < wedges; ++i)
    {
        Path w;
        const float a0 = (float) i * twoPi / (float) wedges, a1 = a0 + twoPi / (float) wedges;
        w.addPieSegment (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), a0, a1, 0.8f);
        g.setColour (i % 2 == 0 ? red : cream);
        g.fillPath (w);
    }
    g.setColour (charcoal);
    g.drawEllipse (Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), R * 0.015f);

    // Marquee bulbs on the rim — lit while the singer is voiced
    for (int i = 0; i < 24; ++i)
    {
        const auto p = c.getPointOnCircumference (R * 0.9f, (float) i * twoPi / 24.0f);
        const float bd = R * 0.055f;
        auto b = Rectangle<float> (bd, bd).withCentre (p);
        if (glow > 0.02f)
        {
            g.setColour (gold.withAlpha (0.35f * glow));
            g.fillEllipse (b.expanded (bd * 0.6f));
        }
        g.setColour (charcoal);
        g.fillEllipse (b.expanded (bd * 0.12f));
        g.setColour (goldDark.darker (0.5f).interpolatedWith (gold.brighter (0.3f), glow));
        g.fillEllipse (b);
    }

    // Arena floor
    const float r = R * 0.8f;
    auto arena = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
    g.setGradientFill (ColourGradient (purple.brighter (0.1f), c.x, c.y - r * 0.3f, charcoal, c.x, c.y + r, true));
    g.fillEllipse (arena);
    g.setColour (gold);
    g.drawEllipse (arena, R * 0.025f);

    // Cents scale on the upper arc (-100 .. +100)
    const float span = 1.15f; // radians either side of 12 o'clock
    const float scaleR = r * 0.82f;
    g.setFont (sans (r * 0.085f, true));
    for (int cents = -100; cents <= 100; cents += 10)
    {
        const float a = (float) cents / 100.0f * span;
        const bool major = cents % 50 == 0;
        const auto p0 = c.getPointOnCircumference (scaleR, a);
        const auto p1 = c.getPointOnCircumference (scaleR - (major ? r * 0.1f : r * 0.05f), a);
        g.setColour (cents == 0 ? gold : cream.withAlpha (major ? 0.9f : 0.5f));
        g.drawLine ({ p0, p1 }, major ? r * 0.02f : r * 0.01f);
        if (major)
        {
            const auto lp = c.getPointOnCircumference (scaleR - r * 0.19f, a);
            g.drawText (cents == 0 ? String ("0") : (cents > 0 ? "+" : "") + String (cents),
                        Rectangle<float> (r * 0.3f, r * 0.12f).withCentre (lp), Justification::centred);
        }
    }
    // Candy-stripe arc showing the applied correction
    if (std::abs (needleCents) > 0.5f)
        drawStripedArc (g, c, scaleR + r * 0.06f, r * 0.05f, 0.0f, needleCents / 100.0f * span, 0.07f);

    // Singer deviation marker (blue)
    {
        const float a = devCents / 100.0f * span;
        const auto p = c.getPointOnCircumference (scaleR + r * 0.06f, a);
        Path tri;
        const float s = r * 0.05f;
        tri.addTriangle (-s, -s * 1.3f, s, -s * 1.3f, 0.0f, s * 0.4f);
        tri.applyTransform (AffineTransform::rotation (a).translated (p));
        g.setColour (blue.brighter (0.4f).withAlpha (0.25f + 0.75f * glow));
        g.fillPath (tri);
    }

    // Gold needle (a juggling pin) for the applied correction
    {
        const float a = needleCents / 100.0f * span;
        Path pin;
        const float L = scaleR * 0.95f;
        pin.startNewSubPath (0.0f, -L);
        pin.cubicTo (r * 0.045f, -L * 0.85f, r * 0.03f, -L * 0.6f, r * 0.02f, -L * 0.45f);
        pin.cubicTo (r * 0.07f, -L * 0.3f, r * 0.05f, -L * 0.1f, 0.0f, -L * 0.02f);
        pin.cubicTo (-r * 0.05f, -L * 0.1f, -r * 0.07f, -L * 0.3f, -r * 0.02f, -L * 0.45f);
        pin.cubicTo (-r * 0.03f, -L * 0.6f, -r * 0.045f, -L * 0.85f, 0.0f, -L);
        pin.closeSubPath();
        pin.applyTransform (AffineTransform::rotation (a).translated (c));
        g.setColour (charcoal.withAlpha (0.6f));
        g.fillPath (pin, AffineTransform::translation (r * 0.015f, r * 0.02f));
        g.setColour (gold);
        g.fillPath (pin);
        g.setColour (charcoal);
        g.strokePath (pin, PathStrokeType (r * 0.012f));
        g.setColour (red);
        g.fillEllipse (Rectangle<float> (r * 0.09f, r * 0.09f).withCentre (c));
    }

    // Big poster lettering: target note
    const bool voiced = frame.voiced != 0;
    String noteName = "—", octave;
    if (heldNote >= 0)
    {
        noteName = ac::pitchClassName (heldNote);
        octave = String (heldNote / 12 - 1);
    }
    const bool bypass = voiced && frame.bypassNote != 0;
    const auto noteCol = bypass ? blue.brighter (0.5f) : (voiced ? gold : cream.withAlpha (heldNote >= 0 ? 0.45f : 0.3f));
    auto noteBox = Rectangle<float> (r * 1.2f, r * 0.46f).withCentre (c.translated (0.0f, r * 0.27f));
    const auto big = display (r * 0.46f);
    GlyphArrangement ga;
    ga.addFittedText (big, noteName, noteBox.getX(), noteBox.getY(), noteBox.getWidth() * 0.8f, noteBox.getHeight(), Justification::centred, 1);
    Path text;
    ga.createPath (text);
    g.setColour (charcoal);
    g.strokePath (text, PathStrokeType (r * 0.03f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setColour (noteCol);
    g.fillPath (text);
    if (octave.isNotEmpty())
    {
        const auto tb = text.getBounds();
        g.setFont (display (r * 0.17f));
        g.drawText (octave, Rectangle<float> (r * 0.3f, r * 0.2f).withPosition (tb.getRight() + r * 0.02f, tb.getBottom() - r * 0.18f),
                    Justification::centredLeft);
    }

    // Readouts
    auto info = Rectangle<float> (r * 1.3f, r * 0.14f).withCentre (c.translated (0.0f, r * 0.58f));
    g.setColour (charcoal.withAlpha (0.8f));
    g.fillRoundedRectangle (info, info.getHeight() * 0.45f);
    g.setFont (sans (info.getHeight() * 0.72f, true));
    String txt;
    if (bypass) txt = "BYPASS NOTE  ·  " + String (frame.inputHz, 1) + " Hz";
    else if (voiced) txt = String (frame.inputHz, 1) + " Hz  ·  " + (frame.correctionCents >= 0 ? "+" : "") + String (roundToInt (frame.correctionCents)) + " cents";
    else txt = "waiting for a voice...";
    g.setColour (voiced ? gold : cream.withAlpha (0.5f));
    g.drawText (txt, info, Justification::centred);

    g.setColour (cream.withAlpha (0.6f));
    g.setFont (sans (r * 0.07f, true));
    g.drawText ("CORRECTION (CENTS)", Rectangle<float> (r * 1.2f, r * 0.1f).withCentre (c.translated (0.0f, r * 0.74f)), Justification::centred);
}
