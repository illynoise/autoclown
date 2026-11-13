// Auto Clown — parameter IDs, layout and a real-time snapshot reader.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/AutoClownEngine.h"

namespace ParamIDs
{
inline constexpr auto tracking   = "tracking";
inline constexpr auto inputType  = "inputType";
inline constexpr auto key        = "key";
inline constexpr auto scale      = "scale";
inline constexpr auto retune     = "retuneSpeed";
inline constexpr auto flex       = "flexTune";
inline constexpr auto humanize   = "humanize";
inline constexpr auto vibrato    = "naturalVibrato";
inline constexpr auto formantOn  = "formantOn";
inline constexpr auto formant    = "formantAmount";
inline constexpr auto transpose  = "transpose";
inline constexpr auto detune     = "detune";
inline constexpr auto mix        = "mix";
inline constexpr auto engine     = "engine";
inline constexpr auto hold       = "hold";
inline constexpr auto midi       = "midiTarget";
inline constexpr auto latch      = "latch";
inline constexpr auto bypass     = "bypass";

juce::String note (int pitchClass); // "note0" .. "note11"
} // namespace ParamIDs

namespace StateProps
{
inline const juce::Identifier uiWidth { "uiWidth" };
inline const juce::Identifier uiHeight { "uiHeight" };
inline const juce::Identifier lowLatency { "lowLatency" };
inline const juce::Identifier presetName { "presetName" };
} // namespace StateProps

juce::StringArray inputTypeChoices();
juce::StringArray keyChoices();
juce::StringArray scaleChoices();
juce::StringArray noteModeChoices();

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Cached raw parameter pointers; snapshot() is lock-free and allocation-free. */
class ParameterReader
{
public:
    explicit ParameterReader (juce::AudioProcessorValueTreeState& state);
    ac::EngineParams snapshot() const noexcept;
    bool bypassed() const noexcept { return bypassParam->load() > 0.5f; }

private:
    std::atomic<float>* get (juce::AudioProcessorValueTreeState& s, const juce::String& id);
    std::atomic<float> *trackingParam, *inputTypeParam, *keyParam, *scaleParam, *retuneParam, *flexParam,
        *humanizeParam, *vibratoParam, *formantOnParam, *formantParam, *transposeParam, *detuneParam,
        *mixParam, *engineParam, *holdParam, *midiParam, *bypassParam;
    std::array<std::atomic<float>*, 12> noteParams {};
};
