// Auto Clown — one-octave note keyboard: gold = target, red = removed, blue = bypassed.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class AutoClownProcessor;

class KeyboardStrip final : public juce::Component,
                            public juce::SettableTooltipClient
{
public:
    explicit KeyboardStrip (AutoClownProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    void setLive (int targetNote, uint16_t heldMidiMask, bool voiced);
    /** Resolved per-note states for the current key/scale/note params. */
    void refreshStates();

private:
    int pitchClassAt (juce::Point<float>) const;
    juce::Rectangle<float> keyBounds (int pc) const;
    static bool isBlack (int pc) { return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    void setNoteMode (int pc, int mode);

    AutoClownProcessor& proc;
    std::array<int, 12> resolved {}; // 0 target, 1 bypass, 2 remove
    std::array<int, 12> explicitMode {};
    int liveTarget = -1, momentaryPc = -1;
    uint16_t heldMask = 0;
    bool liveVoiced = false;
};
