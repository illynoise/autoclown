// Auto Clown — reusable themed widgets.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

class PresetManager;

/** A rotary "circus ball" knob with a title and a gold value readout, bound to a parameter. */
class CircusKnob final : public juce::Component
{
public:
    CircusKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& title,
                bool bipolar = false);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider& getSlider() { return slider; }

private:
    juce::RangedAudioParameter& param;
    juce::String title;
    juce::Slider slider;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

/** Round icon button; the icon is a vector path drawn by a lambda. */
class IconButton final : public juce::Button
{
public:
    enum class Icon { Undo, Redo, Gear, Power, Prev, Next, Dice, Save };
    IconButton (const juce::String& name, Icon icon);
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    Icon icon;
};

/** Clown nose that glows "HONK!" when Retune Speed is 0 (purely visual). */
class HonkIndicator final : public juce::Component
{
public:
    HonkIndicator() { setInterceptsMouseClicks (false, false); }
    void setActive (bool a) { if (a != active) { active = a; repaint(); } }
    void paint (juce::Graphics&) override;

private:
    bool active = false;
};

/** Corner resizer drawn as a gold-and-red striped triangle. */
class CircusResizeHandle final : public juce::ResizableCornerComponent
{
public:
    using ResizableCornerComponent::ResizableCornerComponent;
    void paint (juce::Graphics&) override;
};

/** Preset browser: prev / name menu / next / randomise / save. */
class PresetBar final : public juce::Component
{
public:
    explicit PresetBar (PresetManager&);
    void resized() override;
    void paint (juce::Graphics&) override;
    void refresh();
    std::function<void()> onPresetChanging; // e.g. begin an undo transaction

private:
    void showMenu();
    void promptSave();

    PresetManager& presets;
    IconButton prev { "Previous preset", IconButton::Icon::Prev }, next { "Next preset", IconButton::Icon::Next };
    IconButton dice { "Randomise", IconButton::Icon::Dice }, save { "Save preset", IconButton::Icon::Save };
    juce::TextButton nameButton;
};
