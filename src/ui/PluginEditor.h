// Auto Clown — editor: circus-themed, freely resizable at a fixed aspect ratio.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "BigTopMeter.h"
#include "CircusLookAndFeel.h"
#include "KeyboardStrip.h"
#include "Logo.h"
#include "Widgets.h"
#include "plugin/PluginProcessor.h"

class AutoClownEditor final : public juce::AudioProcessorEditor,
                              private juce::Timer
{
public:
    explicit AutoClownEditor (AutoClownProcessor&);
    ~AutoClownEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    static constexpr float kAspect = (float) AutoClownProcessor::kDefaultWidth / (float) AutoClownProcessor::kDefaultHeight;
    void setScale (float scale);
    /** Pulls meter data immediately (normally driven by the display's vblank). */
    void refreshFromProcessor() { onFrame(); }

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    juce::Rectangle<int> design (float x, float y, float w, float h) const;
    juce::Rectangle<float> designF (float x, float y, float w, float h) const;
    void onFrame();
    void timerCallback() override; // debounced "remember size as global default"
    void showSettingsMenu();
    void showAbout();
    void drawCanopy (juce::Graphics&, juce::Rectangle<float> area) const;
    void caption (juce::Graphics&, const juce::String&, juce::Rectangle<float>) const;

    AutoClownProcessor& proc;
    CircusLookAndFeel lnf;
    float unit = 1.0f, ox = 0.0f, oy = 0.0f;

    // Header
    AutoClownLogo logo;
    PresetBar presetBar { proc.presets };
    IconButton undoButton { "Undo", IconButton::Icon::Undo }, redoButton { "Redo", IconButton::Icon::Redo };
    IconButton settingsButton { "Settings", IconButton::Icon::Gear }, bypassButton { "Bypass", IconButton::Icon::Power };

    // Detector
    juce::ComboBox inputTypeBox, keyBox, scaleBox;
    juce::TextButton learnButton { "Learn" }, autoKeyButton { "Auto Key" };
    CircusKnob tracking { proc.apvts, ParamIDs::tracking, "Tracking" };
    juce::TextButton modernButton { "Modern" }, classicButton { "Classic" };
    juce::ToggleButton holdToggle { "Hold" }, midiToggle { "MIDI Targets" };

    // Correction
    CircusKnob retune { proc.apvts, ParamIDs::retune, "Retune Speed" };
    CircusKnob humanize { proc.apvts, ParamIDs::humanize, "Humanize" };
    CircusKnob flex { proc.apvts, ParamIDs::flex, "Flex Tune" };
    CircusKnob vibrato { proc.apvts, ParamIDs::vibrato, "Natural Vibrato", true };
    HonkIndicator honk;

    // Sideshow
    juce::ToggleButton formantToggle { "Formant Correction" };
    CircusKnob formant { proc.apvts, ParamIDs::formant, "Formant" };
    CircusKnob transpose { proc.apvts, ParamIDs::transpose, "Transpose", true };
    CircusKnob detune { proc.apvts, ParamIDs::detune, "Detune A4", true };
    CircusKnob mix { proc.apvts, ParamIDs::mix, "Mix" };

    // Centre + keyboard
    BigTopMeter meter;
    KeyboardStrip keyboard { proc };
    juce::ToggleButton latchToggle { "Latch" };

    std::unique_ptr<CircusResizeHandle> resizer;
    juce::TooltipWindow tooltips { this, 600 };

    std::unique_ptr<APVTS::ComboBoxAttachment> inputTypeAttach, keyAttach, scaleAttach; // created after items
    APVTS::ButtonAttachment holdAttach { proc.apvts, ParamIDs::hold, holdToggle };
    APVTS::ButtonAttachment midiAttach { proc.apvts, ParamIDs::midi, midiToggle };
    APVTS::ButtonAttachment formantAttach { proc.apvts, ParamIDs::formantOn, formantToggle };
    APVTS::ButtonAttachment latchAttach { proc.apvts, ParamIDs::latch, latchToggle };
    APVTS::ButtonAttachment bypassAttach { proc.apvts, ParamIDs::bypass, bypassButton };
    std::unique_ptr<juce::ParameterAttachment> engineAttach;

    // panels (pixel space, recomputed in resized)
    juce::Rectangle<float> headerArea, detectorPanel, correctionPanel, sideshowPanel, keyboardPanel, latencyBox;

    juce::VBlankAttachment vblank { this, [this] { onFrame(); } };
    bool initialised = false;
    double lastFrameTime = 0.0;
    int lastNoteHash = -1;
    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoClownEditor)
};
