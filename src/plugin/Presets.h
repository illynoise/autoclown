// Auto Clown — factory presets, user presets (XML files) and randomise.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

struct FactoryPreset
{
    juce::String name;
    juce::String description;
    std::vector<std::pair<juce::String, float>> values; // plain (un-normalised) values
};

const std::vector<FactoryPreset>& factoryPresets();

class PresetManager
{
public:
    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    int getNumPresets() const { return (int) factoryPresets().size() + userPresets.size(); }
    int getNumFactoryPresets() const { return (int) factoryPresets().size(); }
    juce::String getPresetName (int index) const;
    int getCurrentIndex() const { return currentIndex; }
    juce::String getCurrentName() const;

    void loadPreset (int index);
    void next() { loadPreset ((currentIndex + 1) % getNumPresets()); }
    void previous() { loadPreset ((currentIndex - 1 + getNumPresets()) % getNumPresets()); }
    void randomise();
    bool saveUserPreset (const juce::String& name);
    void rescanUserPresets();

    /** Called after state restore so the browser shows the saved name. */
    void syncFromState();

    static juce::File getUserPresetDirectory();
    static constexpr const char* kExtension = ".acpreset";

private:
    void setParam (const juce::String& id, float plainValue);
    void resetCharacterParams();

    juce::AudioProcessorValueTreeState& apvts;
    juce::Array<juce::File> userPresets;
    int currentIndex = 0;
    juce::Random rng;
};
