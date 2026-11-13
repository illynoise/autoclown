// Auto Clown — original vector wordmark: "AUTO CLOWN" in a poster face where the
// "O" of CLOWN is a red clown-nose ball, with three juggling balls arcing above.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class AutoClownLogo final : public juce::Component
{
public:
    AutoClownLogo() { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;

    /** Draws the logo into any rectangle (also used by the About box). */
    static void draw (juce::Graphics&, juce::Rectangle<float> area);
};
