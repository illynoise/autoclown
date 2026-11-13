// Auto Clown — central pitch-correction meter drawn as a big-top circus ring.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "dsp/AutoClownEngine.h"

class BigTopMeter final : public juce::Component
{
public:
    BigTopMeter() { setInterceptsMouseClicks (false, false); }

    /** Call at frame rate with the newest frame (or nothing new). */
    void update (const ac::MeterFrame* latest, float dtSeconds);
    void paint (juce::Graphics&) override;

private:
    ac::MeterFrame frame;
    float needleCents = 0.0f, devCents = 0.0f, glow = 0.0f;
    int heldNote = -1;
    float holdTimer = 0.0f;
};
