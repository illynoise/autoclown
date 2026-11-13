#include "Widgets.h"

#include "CircusLookAndFeel.h"
#include "plugin/Presets.h"

using namespace juce;

// ---------------------------------------------------------------------------- CircusKnob
CircusKnob::CircusKnob (AudioProcessorValueTreeState& s, const String& id, const String& t, bool bipolar)
    : param (*s.getParameter (id)), title (t),
      slider (Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox),
      attachment (s, id, slider)
{
    slider.getProperties().set (Circus::bipolarStyle, bipolar);
    slider.setRotaryParameters (MathConstants<float>::pi * 1.22f, MathConstants<float>::pi * 2.78f, true);
    slider.setDoubleClickReturnValue (true, param.convertFrom0to1 (param.getDefaultValue()));
    slider.setTooltip (param.getName (64) + " (double-click to reset, drag or scroll to change)");
    slider.onValueChange = [this] { repaint(); };
    slider.setTitle (title);
    addAndMakeVisible (slider);
}

void CircusKnob::resized()
{
    auto r = getLocalBounds();
    const int titleH = roundToInt ((float) getHeight() * 0.17f);
    const int valueH = roundToInt ((float) getHeight() * 0.16f);
    r.removeFromTop (titleH);
    r.removeFromBottom (valueH);
    slider.setBounds (r.withSizeKeepingCentre (jmin (r.getWidth(), r.getHeight()), jmin (r.getWidth(), r.getHeight())));
}

void CircusKnob::paint (Graphics& g)
{
    using namespace Circus;
    auto r = getLocalBounds().toFloat();
    const float titleH = r.getHeight() * 0.17f, valueH = r.getHeight() * 0.16f;

    g.setColour (cream);
    g.setFont (display (titleH * 0.78f));
    g.drawFittedText (title, r.removeFromTop (titleH).toNearestInt(), Justification::centred, 1, 0.7f);

    auto v = r.removeFromBottom (valueH).withSizeKeepingCentre (jmin (r.getWidth(), valueH * 4.6f), valueH * 0.92f);
    g.setColour (charcoal.withAlpha (0.85f));
    g.fillRoundedRectangle (v, v.getHeight() * 0.45f);
    g.setColour (gold);
    g.setFont (sans (valueH * 0.78f, true));
    const auto label = param.getLabel();
    auto text = param.getCurrentValueAsText();
    if (label.isNotEmpty() && ! text.containsChar ('(')) text << " " << label;
    g.drawFittedText (text, v.toNearestInt(), Justification::centred, 1, 0.7f);
}

// ---------------------------------------------------------------------------- IconButton
IconButton::IconButton (const String& name, Icon i) : Button (name), icon (i)
{
    setTooltip (name);
    setTitle (name);
}

void IconButton::paintButton (Graphics& g, bool over, bool down)
{
    using namespace Circus;
    auto r = getLocalBounds().toFloat().reduced (1.5f);
    const float d = jmin (r.getWidth(), r.getHeight());
    auto ball = r.withSizeKeepingCentre (d, d);
    const bool on = getToggleState();

    g.setColour (charcoal.withAlpha (0.5f));
    g.fillEllipse (ball.translated (0.0f, d * 0.04f));
    g.setGradientFill (ColourGradient ((on ? gold : red).brighter (over ? 0.35f : 0.15f), ball.getX() + d * 0.3f, ball.getY() + d * 0.25f,
                                       (on ? goldDark : redDark).darker (down ? 0.4f : 0.0f), ball.getRight(), ball.getBottom(), true));
    g.fillEllipse (ball);
    g.setColour (gold);
    g.drawEllipse (ball, jmax (1.0f, d * 0.05f));

    const auto c = ball.getCentre();
    const float s = d * 0.26f;
    Path p;
    PathStrokeType stroke (d * 0.09f, PathStrokeType::curved, PathStrokeType::rounded);
    bool fill = false;
    switch (icon)
    {
        case Icon::Undo:
        case Icon::Redo:
        {
            const float dir = icon == Icon::Undo ? 1.0f : -1.0f;
            p.addCentredArc (c.x, c.y + s * 0.15f, s, s, 0.0f, -dir * 1.9f, dir * 1.2f, true);
            const auto tip = Point<float> (c.x - dir * s * 0.95f, c.y - s * 0.35f);
            Path head;
            head.addTriangle (tip.x - dir * s * 0.1f, tip.y - s * 0.55f, tip.x + dir * s * 0.55f, tip.y + s * 0.15f, tip.x - dir * s * 0.45f, tip.y + s * 0.3f);
            g.setColour (cream);
            g.strokePath (p, stroke);
            g.fillPath (head);
            return;
        }
        case Icon::Gear:
        {
            for (int tooth = 0; tooth < 8; ++tooth)
            {
                p.addRectangle (-s * 0.17f, -s * 1.2f, s * 0.34f, s * 0.5f);
                p.applyTransform (AffineTransform::rotation (MathConstants<float>::twoPi / 8.0f));
            }
            p.addEllipse (-s * 0.85f, -s * 0.85f, s * 1.7f, s * 1.7f);
            p.applyTransform (AffineTransform::translation (c));
            g.setColour (cream);
            g.fillPath (p);
            g.setColour (red.darker (0.1f));
            g.fillEllipse (Rectangle<float> (s * 0.7f, s * 0.7f).withCentre (c));
            return;
        }
        case Icon::Power:
            p.addCentredArc (c.x, c.y + s * 0.1f, s * 0.95f, s * 0.95f, 0.0f, 0.6f, MathConstants<float>::twoPi - 0.6f, true);
            p.startNewSubPath (c.x, c.y - s * 1.1f);
            p.lineTo (c.x, c.y - s * 0.05f);
            break;
        case Icon::Prev:
            p.addTriangle (c.x + s * 0.6f, c.y - s * 0.8f, c.x + s * 0.6f, c.y + s * 0.8f, c.x - s * 0.75f, c.y);
            fill = true;
            break;
        case Icon::Next:
            p.addTriangle (c.x - s * 0.6f, c.y - s * 0.8f, c.x - s * 0.6f, c.y + s * 0.8f, c.x + s * 0.75f, c.y);
            fill = true;
            break;
        case Icon::Dice:
        {
            auto sq = Rectangle<float> (s * 2.0f, s * 2.0f).withCentre (c);
            g.setColour (cream);
            g.fillRoundedRectangle (sq, s * 0.35f);
            g.setColour (charcoal);
            for (auto o : { Point<float> (-0.5f, -0.5f), Point<float> (0.5f, 0.5f), Point<float> (0.0f, 0.0f),
                            Point<float> (0.5f, -0.5f), Point<float> (-0.5f, 0.5f) })
                g.fillEllipse (Rectangle<float> (s * 0.36f, s * 0.36f).withCentre (c + o * s * 1.15f));
            return;
        }
        case Icon::Save:
        {
            // A star stamp = "keep this act"
            p.addStar (c, 5, s * 0.5f, s * 1.15f, 0.0f);
            fill = true;
            break;
        }
    }
    g.setColour (cream);
    if (fill) g.fillPath (p);
    else g.strokePath (p, stroke);
}

// ---------------------------------------------------------------------------- Honk
void HonkIndicator::paint (Graphics& g)
{
    using namespace Circus;
    auto r = getLocalBounds().toFloat();
    const float d = jmin (r.getHeight() * 0.62f, r.getWidth() * 0.5f);
    auto nose = Rectangle<float> (d, d).withCentre ({ r.getX() + d * 0.75f, r.getCentreY() });

    if (active)
    {
        g.setGradientFill (ColourGradient (gold.withAlpha (0.7f), nose.getCentre(), gold.withAlpha (0.0f),
                                           nose.getCentre().translated (d * 0.95f, 0.0f), true));
        g.fillEllipse (nose.expanded (d * 0.45f));
    }
    g.setColour (charcoal);
    g.fillEllipse (nose.expanded (d * 0.06f));
    const auto base = active ? red : red.withSaturation (0.25f).darker (0.5f);
    g.setGradientFill (ColourGradient (base.brighter (0.45f), nose.getX() + d * 0.3f, nose.getY() + d * 0.25f,
                                       base.darker (0.4f), nose.getRight(), nose.getBottom(), true));
    g.fillEllipse (nose);
    g.setColour (cream.withAlpha (active ? 0.85f : 0.3f));
    g.fillEllipse (nose.getX() + d * 0.22f, nose.getY() + d * 0.16f, d * 0.28f, d * 0.2f);

    auto text = r.withTrimmedLeft (nose.getRight() - r.getX() + d * 0.2f);
    g.setFont (display (r.getHeight() * 0.42f));
    g.setColour (active ? gold : cream.withAlpha (0.35f));
    g.drawFittedText (active ? "HONK!" : "honk", text.toNearestInt(), Justification::centredLeft, 1, 0.7f);
}

// ---------------------------------------------------------------------------- Resizer
void CircusResizeHandle::paint (Graphics& g)
{
    using namespace Circus;
    const auto r = getLocalBounds().toFloat();
    Path tri;
    tri.addTriangle (r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getBottom());
    g.saveState();
    g.reduceClipRegion (tri);
    g.setColour (red);
    g.fillPath (tri);
    const float w = r.getWidth() / 6.0f;
    g.setColour (gold);
    for (int i = 1; i < 12; i += 2)
    {
        Path stripe;
        const float o = (float) i * w;
        stripe.addQuadrilateral (r.getX() + o, r.getBottom(), r.getX() + o + w, r.getBottom(),
                                 r.getRight(), r.getBottom() - (r.getWidth() - o - w), r.getRight(), r.getBottom() - (r.getWidth() - o));
        g.fillPath (stripe);
    }
    g.restoreState();
    g.setColour (charcoal);
    g.strokePath (tri, PathStrokeType (1.2f));
}

// ---------------------------------------------------------------------------- PresetBar
PresetBar::PresetBar (PresetManager& p) : presets (p)
{
    for (auto* b : { (Button*) &prev, (Button*) &next, (Button*) &dice, (Button*) &save, (Button*) &nameButton })
        addAndMakeVisible (b);
    nameButton.getProperties().set (Circus::ticketStyle, true);
    nameButton.setTooltip ("Browse presets");
    auto changing = [this] { if (onPresetChanging) onPresetChanging(); };
    prev.onClick = [this, changing] { changing(); presets.previous(); refresh(); };
    next.onClick = [this, changing] { changing(); presets.next(); refresh(); };
    dice.onClick = [this, changing] { changing(); presets.randomise(); refresh(); };
    save.onClick = [this] { promptSave(); };
    nameButton.onClick = [this] { showMenu(); };
    refresh();
}

void PresetBar::refresh()
{
    nameButton.setButtonText (presets.getCurrentName());
    repaint();
}

void PresetBar::paint (Graphics& g)
{
    using namespace Circus;
    auto r = getLocalBounds().toFloat();
    g.setColour (charcoal.withAlpha (0.75f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (gold);
    g.drawRoundedRectangle (r.reduced (1.0f), r.getHeight() * 0.5f, jmax (1.0f, r.getHeight() * 0.04f));
}

void PresetBar::resized()
{
    auto r = getLocalBounds().reduced (roundToInt ((float) getHeight() * 0.1f));
    const int b = r.getHeight();
    const int gap = roundToInt ((float) b * 0.12f);
    prev.setBounds (r.removeFromLeft (b));
    r.removeFromLeft (gap);
    save.setBounds (r.removeFromRight (b));
    r.removeFromRight (gap);
    dice.setBounds (r.removeFromRight (b));
    r.removeFromRight (gap);
    next.setBounds (r.removeFromRight (b));
    r.removeFromRight (gap);
    nameButton.setBounds (r);
}

void PresetBar::showMenu()
{
    PopupMenu m, factory, user;
    for (int i = 0; i < presets.getNumPresets(); ++i)
    {
        auto& target = i < presets.getNumFactoryPresets() ? factory : user;
        target.addItem (i + 1, presets.getPresetName (i), true, i == presets.getCurrentIndex());
    }
    m.addSectionHeader ("Factory Acts");
    for (PopupMenu::MenuItemIterator it (factory); it.next();) m.addItem (it.getItem());
    m.addSeparator();
    m.addSectionHeader ("Your Acts");
    if (user.getNumItems() == 0) m.addItem (-1, "(none saved yet)", false);
    for (PopupMenu::MenuItemIterator it (user); it.next();) m.addItem (it.getItem());
    m.addSeparator();
    m.addItem (-2, "Rescan preset folder");
    m.addItem (-3, "Reveal preset folder");

    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&nameButton),
                     [safe = SafePointer<PresetBar> (this)] (int result)
                     {
                         if (safe == nullptr || result == 0) return;
                         if (result == -2) { safe->presets.rescanUserPresets(); return; }
                         if (result == -3) { PresetManager::getUserPresetDirectory().createDirectory();
                                             PresetManager::getUserPresetDirectory().revealToUser(); return; }
                         if (safe->onPresetChanging) safe->onPresetChanging();
                         safe->presets.loadPreset (result - 1);
                         safe->refresh();
                     });
}

void PresetBar::promptSave()
{
    auto* w = new AlertWindow ("Save Act", "Name your preset:", MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", presets.getCurrentName());
    w->addButton ("Save", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
    w->enterModalState (true, ModalCallbackFunction::create ([safe = SafePointer<PresetBar> (this), w] (int result)
    {
        if (safe != nullptr && result == 1)
        {
            if (! safe->presets.saveUserPreset (w->getTextEditorContents ("name")))
                AlertWindow::showAsync (MessageBoxOptions().withTitle ("Could not save").withMessage ("The preset could not be written.")
                                                           .withButton ("OK", 0), nullptr);
            safe->refresh();
        }
    }), true);
}
