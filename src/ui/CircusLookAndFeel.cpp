#include "CircusLookAndFeel.h"

#include "AutoClownAssets.h"

using namespace juce;

namespace Circus
{
namespace
{
    struct Typefaces
    {
        Typeface::Ptr rye = Typeface::createSystemTypefaceFor (AutoClownAssets::RyeRegular_ttf,
                                                               (size_t) AutoClownAssets::RyeRegular_ttfSize);
        Typeface::Ptr semi = Typeface::createSystemTypefaceFor (AutoClownAssets::BarlowSemiCondensedSemiBold_ttf,
                                                                (size_t) AutoClownAssets::BarlowSemiCondensedSemiBold_ttfSize);
        Typeface::Ptr bold = Typeface::createSystemTypefaceFor (AutoClownAssets::BarlowSemiCondensedBold_ttf,
                                                                (size_t) AutoClownAssets::BarlowSemiCondensedBold_ttfSize);
    };

    const Typefaces& faces()
    {
        static const Typefaces t;
        return t;
    }
} // namespace

Font display (float h) { return Font (FontOptions (faces().rye).withHeight (jmax (1.0f, h))); }
Font sans (float h, bool b) { return Font (FontOptions (b ? faces().bold : faces().semi).withHeight (jmax (1.0f, h))); }

void drawStripedArc (Graphics& g, Point<float> c, float radius, float thickness, float from, float to, float stripe)
{
    if (std::abs (to - from) < 1.0e-4f) return;
    if (to < from) std::swap (from, to);
    int i = 0;
    for (float a = from; a < to; a += stripe, ++i)
    {
        const float b = jmin (to, a + stripe);
        Path p;
        p.addCentredArc (c.x, c.y, radius, radius, 0.0f, a, b, true);
        g.setColour ((i % 2 == 0) ? red : cream);
        g.strokePath (p, PathStrokeType (thickness, PathStrokeType::curved, PathStrokeType::butt));
    }
}

Path ticketPath (Rectangle<float> r)
{
    const float notch = r.getHeight() * 0.22f, corner = r.getHeight() * 0.12f;
    Path outer;
    outer.addRoundedRectangle (r, corner);
    Path holes;
    holes.addEllipse (r.getX() - notch, r.getCentreY() - notch, notch * 2.0f, notch * 2.0f);
    holes.addEllipse (r.getRight() - notch, r.getCentreY() - notch, notch * 2.0f, notch * 2.0f);
    // Subtract notches using the even-odd fill rule on a combined path.
    Path p = outer;
    p.addPath (holes);
    p.setUsingNonZeroWinding (false);
    return p;
}

Rectangle<float> drawPanel (Graphics& g, Rectangle<float> b, const String& title, float u)
{
    const float corner = 10.0f * u;
    // Drop shadow
    g.setColour (charcoal.withAlpha (0.45f));
    g.fillRoundedRectangle (b.translated (0.0f, 3.0f * u), corner);
    // Body
    g.setGradientFill (ColourGradient (purple, b.getX(), b.getY(), purpleDeep, b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, corner);
    g.setColour (gold);
    g.drawRoundedRectangle (b.reduced (1.5f * u), corner, 2.0f * u);

    if (title.isEmpty()) return b.reduced (10.0f * u);

    // Ribbon header
    const float rh = 26.0f * u;
    auto ribbon = b.withHeight (rh).reduced (18.0f * u, 0.0f).translated (0.0f, -rh * 0.35f);
    Path tail;
    const float tw = 12.0f * u;
    tail.addTriangle (ribbon.getX() - tw, ribbon.getY() + rh * 0.25f, ribbon.getX(), ribbon.getY() + rh * 0.25f, ribbon.getX(), ribbon.getBottom());
    tail.addTriangle (ribbon.getRight() + tw, ribbon.getY() + rh * 0.25f, ribbon.getRight(), ribbon.getY() + rh * 0.25f, ribbon.getRight(), ribbon.getBottom());
    g.setColour (redDark);
    g.fillPath (tail);
    g.setColour (red);
    g.fillRoundedRectangle (ribbon, 4.0f * u);
    g.setColour (gold);
    g.drawRoundedRectangle (ribbon.reduced (1.0f * u), 4.0f * u, 1.5f * u);
    g.setColour (cream);
    g.setFont (display (rh * 0.68f));
    g.drawText (title, ribbon, Justification::centred, false);

    return b.withTrimmedTop (rh * 0.8f).reduced (10.0f * u, 6.0f * u);
}
} // namespace Circus

// =================================================================================

CircusLookAndFeel::CircusLookAndFeel()
{
    using namespace Circus;
    setColour (ComboBox::textColourId, charcoal);
    setColour (ComboBox::backgroundColourId, cream);
    setColour (PopupMenu::backgroundColourId, purpleDeep);
    setColour (PopupMenu::textColourId, cream);
    setColour (PopupMenu::highlightedBackgroundColourId, gold);
    setColour (PopupMenu::highlightedTextColourId, charcoal);
    setColour (Label::textColourId, cream);
    setColour (TextButton::textColourOffId, red);
    setColour (TextButton::textColourOnId, charcoal);
    setColour (AlertWindow::backgroundColourId, purpleDeep);
    setColour (AlertWindow::textColourId, cream);
    setColour (AlertWindow::outlineColourId, gold);
    setColour (TextEditor::backgroundColourId, cream);
    setColour (TextEditor::textColourId, charcoal);
    setColour (TextEditor::highlightColourId, gold.withAlpha (0.5f));
    setColour (TextEditor::outlineColourId, gold);
    setColour (TextEditor::focusedOutlineColourId, red);
    setColour (CaretComponent::caretColourId, charcoal);
    setColour (TooltipWindow::backgroundColourId, charcoal);
    setColour (TooltipWindow::textColourId, cream);
    setColour (TooltipWindow::outlineColourId, gold);
}

void CircusLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                          float start, float end, Slider& s)
{
    using namespace Circus;
    const auto bounds = Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = jmin (bounds.getWidth(), bounds.getHeight());
    const auto c = bounds.getCentre();
    const float rOuter = size * 0.5f - 1.0f;
    const float arcW = size * 0.085f;
    const float arcR = rOuter - arcW * 0.5f;
    const float ballR = rOuter - arcW - size * 0.05f;
    const bool enabled = s.isEnabled();

    // Track
    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (charcoal.withAlpha (0.75f));
    g.strokePath (track, PathStrokeType (arcW, PathStrokeType::curved, PathStrokeType::rounded));

    // Candy-stripe value arc (from centre for bipolar controls)
    const float angle = start + pos * (end - start);
    const bool bipolar = (bool) s.getProperties().getWithDefault (bipolarStyle, false);
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (enabled) drawStripedArc (g, c, arcR, arcW * 0.8f, from, angle, 0.16f);

    // Ball: blue-to-purple sphere with a gold band and highlight
    auto ball = Rectangle<float> (ballR * 2.0f, ballR * 2.0f).withCentre (c);
    g.setColour (charcoal.withAlpha (0.5f));
    g.fillEllipse (ball.translated (0.0f, size * 0.025f));
    ColourGradient grad (blue.brighter (0.25f), c.x - ballR * 0.4f, c.y - ballR * 0.5f,
                         purpleDeep, c.x + ballR * 0.6f, c.y + ballR, true);
    g.setGradientFill (grad);
    g.fillEllipse (ball);
    // Equator band (follows the pointer so the ball "rolls")
    {
        Path band;
        band.addCentredArc (c.x, c.y, ballR * 0.72f, ballR * 0.72f, 0.0f, angle + 0.9f, angle + MathConstants<float>::twoPi - 0.9f, true);
        g.setColour (red.withAlpha (enabled ? 0.85f : 0.3f));
        g.strokePath (band, PathStrokeType (ballR * 0.16f));
    }
    g.setColour (cream.withAlpha (0.28f));
    g.fillEllipse (ball.getX() + ballR * 0.35f, ball.getY() + ballR * 0.22f, ballR * 0.75f, ballR * 0.45f);
    g.setColour (gold);
    g.drawEllipse (ball, jmax (1.0f, size * 0.018f));

    // Gold pointer marker
    const auto tip = c.getPointOnCircumference (ballR * 0.92f, angle);
    const auto base = c.getPointOnCircumference (ballR * 0.2f, angle);
    g.setColour (charcoal);
    g.drawLine ({ base, tip }, size * 0.075f);
    g.setColour (enabled ? gold : gold.withSaturation (0.2f));
    g.drawLine ({ base, tip }, size * 0.045f);
    const float dot = size * 0.075f;
    g.fillEllipse (Rectangle<float> (dot, dot).withCentre (tip));
    g.setColour (charcoal);
    g.drawEllipse (Rectangle<float> (dot, dot).withCentre (tip), jmax (0.8f, size * 0.012f));
}

void CircusLookAndFeel::drawButtonBackground (Graphics& g, Button& b, const Colour&, bool over, bool down)
{
    using namespace Circus;
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();

    if ((bool) b.getProperties().getWithDefault (ticketStyle, false))
    {
        auto body = r.reduced (r.getHeight() * 0.06f);
        const auto p = ticketPath (body);
        g.setColour (charcoal.withAlpha (0.45f));
        g.fillPath (p, AffineTransform::translation (0.0f, r.getHeight() * 0.06f));
        g.setColour (on ? gold : (down ? creamShade : (over ? cream.brighter (0.3f) : cream)));
        g.fillPath (p);
        g.setColour (red);
        g.strokePath (p, PathStrokeType (jmax (1.0f, r.getHeight() * 0.05f)));
        // Perforation line
        const float px = body.getX() + body.getHeight() * 0.62f;
        g.setColour (red.withAlpha (0.7f));
        for (float yy = body.getY() + body.getHeight() * 0.16f; yy < body.getBottom() - body.getHeight() * 0.16f; yy += body.getHeight() * 0.13f)
            g.fillEllipse (px - 1.0f, yy, 2.2f, 2.2f);
        // Star on the stub
        Path star;
        star.addStar ({ body.getX() + body.getHeight() * 0.36f, body.getCentreY() }, 5,
                      body.getHeight() * 0.09f, body.getHeight() * 0.2f);
        g.setColour (on ? red : gold.darker (0.1f));
        g.fillPath (star);
        return;
    }

    if ((bool) b.getProperties().getWithDefault (segmentStyle, false))
    {
        g.setColour (on ? gold : (over ? purple.brighter (0.25f) : purpleDeep.brighter (0.15f)));
        g.fillRoundedRectangle (r, r.getHeight() * 0.25f);
        g.setColour (gold);
        g.drawRoundedRectangle (r, r.getHeight() * 0.25f, jmax (1.0f, r.getHeight() * 0.05f));
        return;
    }

    g.setColour (on ? gold : (down ? red.darker (0.2f) : (over ? red.brighter (0.15f) : red)));
    g.fillRoundedRectangle (r, r.getHeight() * 0.3f);
    g.setColour (gold);
    g.drawRoundedRectangle (r, r.getHeight() * 0.3f, jmax (1.0f, r.getHeight() * 0.06f));
}

Font CircusLookAndFeel::getTextButtonFont (TextButton& b, int h)
{
    if ((bool) b.getProperties().getWithDefault (Circus::ticketStyle, false))
        return Circus::display ((float) h * 0.42f);
    return Circus::sans ((float) h * 0.52f, true);
}

void CircusLookAndFeel::drawButtonText (Graphics& g, TextButton& b, bool, bool)
{
    using namespace Circus;
    const bool ticket = (bool) b.getProperties().getWithDefault (ticketStyle, false);
    const bool segment = (bool) b.getProperties().getWithDefault (segmentStyle, false);
    auto r = b.getLocalBounds().toFloat();
    if (ticket) r = r.withTrimmedLeft (r.getHeight() * 0.62f);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    const bool on = b.getToggleState();
    g.setColour (ticket ? (on ? charcoal : red) : (segment ? (on ? charcoal : cream) : (on ? charcoal : cream)));
    g.drawFittedText (b.getButtonText(), r.reduced (4.0f, 0.0f).toNearestInt(), Justification::centred, 1, 0.8f);
}

void CircusLookAndFeel::drawToggleButton (Graphics& g, ToggleButton& b, bool over, bool)
{
    // Marquee bulb + label
    using namespace Circus;
    auto r = b.getLocalBounds().toFloat();
    const float d = jmin (r.getHeight() * 0.7f, r.getWidth() * 0.35f);
    auto bulb = Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.5f + 2.0f, r.getCentreY() });
    const bool on = b.getToggleState();
    if (on)
    {
        g.setGradientFill (ColourGradient (gold.withAlpha (0.55f), bulb.getCentre(), gold.withAlpha (0.0f),
                                           bulb.getCentre().translated (d, 0.0f), true));
        g.fillEllipse (bulb.expanded (d * 0.45f));
    }
    g.setColour (charcoal);
    g.fillEllipse (bulb.expanded (d * 0.08f));
    g.setGradientFill (ColourGradient (on ? cream : gold.darker (0.65f), bulb.getX() + d * 0.35f, bulb.getY() + d * 0.3f,
                                       on ? gold : goldDark.darker (0.6f), bulb.getRight(), bulb.getBottom(), true));
    g.fillEllipse (bulb);
    if (over) { g.setColour (cream.withAlpha (0.25f)); g.drawEllipse (bulb, 1.0f); }

    g.setColour (b.isEnabled() ? cream : cream.withAlpha (0.4f));
    g.setFont (sans (r.getHeight() * 0.55f, true));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (d + d * 0.6f).toNearestInt(), Justification::centredLeft, 1, 0.8f);
}

void CircusLookAndFeel::drawComboBox (Graphics& g, int w, int h, bool, int, int, int, int, ComboBox& box)
{
    using namespace Circus;
    auto r = Rectangle<float> ((float) w, (float) h).reduced (1.0f);
    const float corner = r.getHeight() * 0.22f;
    g.setColour (charcoal.withAlpha (0.4f));
    g.fillRoundedRectangle (r.translated (0.0f, r.getHeight() * 0.05f), corner);
    g.setColour (box.isEnabled() ? cream : creamShade);
    g.fillRoundedRectangle (r, corner);
    g.setColour (gold);
    g.drawRoundedRectangle (r, corner, jmax (1.0f, r.getHeight() * 0.06f));

    // Red pennant arrow
    const float a = r.getHeight() * 0.22f;
    const auto ac = Point<float> (r.getRight() - r.getHeight() * 0.45f, r.getCentreY());
    Path tri;
    tri.addTriangle (ac.x - a, ac.y - a * 0.5f, ac.x + a, ac.y - a * 0.5f, ac.x, ac.y + a * 0.7f);
    g.setColour (red);
    g.fillPath (tri);
}

Font CircusLookAndFeel::getComboBoxFont (ComboBox& b) { return Circus::sans ((float) b.getHeight() * 0.56f, true); }

void CircusLookAndFeel::positionComboBoxText (ComboBox& box, Label& label)
{
    const int h = box.getHeight();
    label.setBounds (roundToInt ((float) h * 0.25f), 1, box.getWidth() - roundToInt ((float) h * 1.0f), h - 2);
    label.setFont (getComboBoxFont (box));
    label.setColour (Label::textColourId, Circus::charcoal);
}

void CircusLookAndFeel::drawPopupMenuBackground (Graphics& g, int w, int h)
{
    using namespace Circus;
    g.fillAll (purpleDeep);
    g.setColour (gold);
    g.drawRect (0, 0, w, h, 2);
}

Font CircusLookAndFeel::getPopupMenuFont() { return Circus::sans (17.0f * menuScale, true); }

void CircusLookAndFeel::getIdealPopupMenuItemSize (const String& text, bool sep, int stdH, int& w, int& h)
{
    LookAndFeel_V4::getIdealPopupMenuItemSize (text, sep, stdH, w, h);
    if (! sep) h = roundToInt (26.0f * menuScale);
    w = roundToInt ((float) w * 1.05f);
}

void CircusLookAndFeel::drawPopupMenuItem (Graphics& g, const Rectangle<int>& area, bool isSeparator, bool isActive,
                                           bool isHighlighted, bool isTicked, bool hasSubMenu, const String& text,
                                           const String& shortcut, const Drawable* icon, const Colour* textColour)
{
    using namespace Circus;
    if (isSeparator)
    {
        auto r = area.toFloat().reduced (8.0f, 0.0f);
        g.setColour (gold.withAlpha (0.5f));
        g.drawLine (r.getX(), r.getCentreY(), r.getRight(), r.getCentreY(), 1.0f);
        return;
    }
    auto r = area.toFloat().reduced (3.0f, 1.0f);
    if (isHighlighted && isActive)
    {
        g.setColour (gold);
        g.fillRoundedRectangle (r, 4.0f);
    }
    const auto col = textColour != nullptr ? *textColour : (isHighlighted ? charcoal : cream);
    g.setColour (isActive ? col : col.withAlpha (0.4f));
    g.setFont (getPopupMenuFont());
    auto textArea = r.withTrimmedLeft (r.getHeight() * 1.1f).withTrimmedRight (r.getHeight() * 0.8f);
    g.drawFittedText (text, textArea.toNearestInt(), Justification::centredLeft, 1);
    if (isTicked)
    {
        Path star;
        star.addStar ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() }, 5, r.getHeight() * 0.13f, r.getHeight() * 0.3f);
        g.setColour (isHighlighted ? red : gold);
        g.fillPath (star);
    }
    if (hasSubMenu)
    {
        Path tri;
        const float a = r.getHeight() * 0.18f, cx = r.getRight() - r.getHeight() * 0.4f;
        tri.addTriangle (cx - a * 0.6f, r.getCentreY() - a, cx - a * 0.6f, r.getCentreY() + a, cx + a * 0.7f, r.getCentreY());
        g.fillPath (tri);
    }
    if (shortcut.isNotEmpty())
        g.drawText (shortcut, textArea.toNearestInt(), Justification::centredRight);
    ignoreUnused (icon);
}

Font CircusLookAndFeel::getLabelFont (Label& l) { return Circus::sans ((float) l.getHeight() * 0.7f, true); }
