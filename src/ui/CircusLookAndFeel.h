// Auto Clown — circus / big-top LookAndFeel (original design).
// Everything is drawn with vector paths sized from component bounds, so it scales cleanly
// at any window size and on Retina displays.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace Circus
{
// Palette
inline const juce::Colour red      { 0xFFD7263D };
inline const juce::Colour cream    { 0xFFFFF4E0 };
inline const juce::Colour gold     { 0xFFF4B400 };
inline const juce::Colour blue     { 0xFF1B4F9C };
inline const juce::Colour purple   { 0xFF4B2A6B };
inline const juce::Colour charcoal { 0xFF1E1A1D };

// Derived tones (kept close to the palette for cohesion)
inline const juce::Colour purpleDeep  = purple.darker (0.45f);
inline const juce::Colour creamShade  = cream.darker (0.12f);
inline const juce::Colour redDark     = red.darker (0.35f);
inline const juce::Colour goldDark    = gold.darker (0.35f);

/** Embedded open-licensed fonts (SIL OFL): Rye (display) and Barlow Semi Condensed (UI). */
juce::Font display (float height);
juce::Font sans (float height, bool bold = false);

/** Component properties used to pick a drawing style. */
inline const juce::Identifier ticketStyle { "circusTicket" };
inline const juce::Identifier bipolarStyle { "circusBipolar" };
inline const juce::Identifier segmentStyle { "circusSegment" };

/** Candy-stripe arc between two angles (radians, JUCE convention: 0 = 12 o'clock). */
void drawStripedArc (juce::Graphics&, juce::Point<float> centre, float radius, float thickness,
                     float fromAngle, float toAngle, float stripeAngle);

/** A ticket-stub shape: rounded rectangle with semicircular notches on both ends. */
juce::Path ticketPath (juce::Rectangle<float> r);

/** Section panel with a gold frame and a ribbon header. Returns the content area. */
juce::Rectangle<float> drawPanel (juce::Graphics&, juce::Rectangle<float> bounds, const juce::String& title, float unit);
} // namespace Circus

class CircusLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    CircusLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool over, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool over, bool down) override;

    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
                            bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                            const juce::Colour* textColour) override;
    juce::Font getPopupMenuFont() override;
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator, int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getLabelFont (juce::Label&) override;

    /** Popup menus are not inside the scaled editor, so they get their own scale. */
    void setMenuScale (float s) { menuScale = s; }

private:
    float menuScale = 1.0f;
};
