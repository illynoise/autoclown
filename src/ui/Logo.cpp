#include "Logo.h"

#include "CircusLookAndFeel.h"

using namespace juce;

namespace
{
void drawNose (Graphics& g, Rectangle<float> r)
{
    using namespace Circus;
    g.setColour (charcoal);
    g.fillEllipse (r.expanded (r.getWidth() * 0.06f));
    g.setGradientFill (ColourGradient (red.brighter (0.45f), r.getX() + r.getWidth() * 0.3f, r.getY() + r.getHeight() * 0.25f,
                                       red.darker (0.4f), r.getRight(), r.getBottom(), true));
    g.fillEllipse (r);
    g.setColour (cream.withAlpha (0.85f));
    g.fillEllipse (r.getX() + r.getWidth() * 0.22f, r.getY() + r.getHeight() * 0.16f, r.getWidth() * 0.28f, r.getHeight() * 0.2f);
}
} // namespace

void AutoClownLogo::paint (Graphics& g) { draw (g, getLocalBounds().toFloat()); }

void AutoClownLogo::draw (Graphics& g, Rectangle<float> area)
{
    using namespace Circus;
    // Design box: 330 x 64 units.
    const float u = jmin (area.getWidth() / 330.0f, area.getHeight() / 64.0f);
    const auto box = Rectangle<float> (330.0f * u, 64.0f * u).withCentre (area.getCentre());

    const float textH = 37.0f * u;
    const auto font = display (textH);
    const float baseline = box.getY() + 55.0f * u;

    GlyphArrangement left, right;
    left.addLineOfText (font, "AUTO CL", box.getX() + 6.0f * u, baseline);
    const float leftW = left.getBoundingBox (0, -1, true).getWidth();
    const float noseD = textH * 0.66f;
    const float noseX = box.getX() + 6.0f * u + leftW + 2.0f * u;
    right.addLineOfText (font, "WN", noseX + noseD + 2.0f * u, baseline);

    Path text;
    left.createPath (text);
    right.createPath (text);

    // Shadow, outline, gold fill
    g.setColour (charcoal.withAlpha (0.6f));
    g.fillPath (text, AffineTransform::translation (2.5f * u, 3.0f * u));
    g.setColour (charcoal);
    g.strokePath (text, PathStrokeType (4.0f * u, PathStrokeType::curved, PathStrokeType::rounded));
    g.setGradientFill (ColourGradient (cream, 0.0f, baseline - textH * 0.75f, gold, 0.0f, baseline, false));
    g.fillPath (text);

    // Clown nose in place of the "O"
    const auto nose = Rectangle<float> (noseD, noseD).withPosition (noseX, baseline - noseD - textH * 0.02f);
    drawNose (g, nose);

    // Juggling balls arcing over the wordmark
    const Colour cols[] = { red, gold, blue };
    const float bd = 9.0f * u;
    const float arcX0 = box.getX() + 40.0f * u, arcX1 = box.getRight() - 40.0f * u;
    for (int i = 0; i < 3; ++i)
    {
        const float t = 0.2f + 0.3f * (float) i;
        const float x = arcX0 + t * (arcX1 - arcX0);
        const float y = box.getY() + 15.0f * u - 6.0f * u * std::sin (MathConstants<float>::pi * t);
        auto b = Rectangle<float> (bd, bd).withCentre ({ x, y });
        g.setColour (charcoal);
        g.fillEllipse (b.expanded (1.2f * u));
        g.setColour (cols[i]);
        g.fillEllipse (b);
        g.setColour (cream.withAlpha (0.7f));
        g.fillEllipse (b.getX() + bd * 0.2f, b.getY() + bd * 0.15f, bd * 0.35f, bd * 0.3f);
    }
}
