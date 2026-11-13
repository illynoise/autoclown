#include "Parameters.h"

using namespace juce;

String ParamIDs::note (int pc) { return "note" + String (pc); }

StringArray inputTypeChoices()
{
    StringArray s;
    for (int i = 0; i < (int) ac::InputType::NumTypes; ++i) s.add (ac::inputTypeName ((ac::InputType) i));
    return s;
}

StringArray keyChoices()
{
    StringArray s;
    for (int i = 0; i < 12; ++i) s.add (ac::pitchClassName (i));
    return s;
}

StringArray scaleChoices()
{
    StringArray s;
    for (int i = 0; i < (int) ac::ScaleType::NumScales; ++i) s.add (ac::scaleName ((ac::ScaleType) i));
    return s;
}

StringArray noteModeChoices() { return { "Scale", "Target", "Bypass", "Remove" }; }

namespace
{
constexpr int kVersion = 1;

NormalisableRange<float> skewedRange (float lo, float hi, float centre, float step = 0.0f)
{
    NormalisableRange<float> r (lo, hi, step);
    r.setSkewForCentre (centre);
    return r;
}

auto pct (int decimals = 0)
{
    return AudioParameterFloatAttributes().withStringFromValueFunction (
        [decimals] (float v, int) { return String (v, decimals); }).withLabel ("%");
}
} // namespace

AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    AudioProcessorValueTreeState::ParameterLayout layout;
    using F = AudioParameterFloat;
    using C = AudioParameterChoice;
    using B = AudioParameterBool;
    using I = AudioParameterInt;

    auto group = [] (const String& id, const String& name) { return std::make_unique<AudioProcessorParameterGroup> (id, name, "|"); };

    auto detection = group ("detection", "Detection");
    detection->addChild (std::make_unique<F> (ParameterID { ParamIDs::tracking, kVersion }, "Tracking",
                                              NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f, pct()));
    detection->addChild (std::make_unique<C> (ParameterID { ParamIDs::inputType, kVersion }, "Input Type", inputTypeChoices(), 1));
    detection->addChild (std::make_unique<C> (ParameterID { ParamIDs::key, kVersion }, "Key", keyChoices(), 0));
    detection->addChild (std::make_unique<C> (ParameterID { ParamIDs::scale, kVersion }, "Scale", scaleChoices(), 1));

    auto correction = group ("correction", "Correction");
    correction->addChild (std::make_unique<F> (ParameterID { ParamIDs::retune, kVersion }, "Retune Speed",
                                               skewedRange (0.0f, 400.0f, 50.0f, 0.1f), 20.0f,
                                               AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (
                                                   [] (float v, int) { return v < 0.05f ? String ("0 (hard)") : String (v, v < 10.0f ? 1 : 0); })));
    correction->addChild (std::make_unique<F> (ParameterID { ParamIDs::flex, kVersion }, "Flex Tune",
                                               NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));
    correction->addChild (std::make_unique<F> (ParameterID { ParamIDs::humanize, kVersion }, "Humanize",
                                               NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, pct()));
    correction->addChild (std::make_unique<F> (ParameterID { ParamIDs::vibrato, kVersion }, "Natural Vibrato",
                                               NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                                               AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (
                                                   [] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1); })));
    correction->addChild (std::make_unique<B> (ParameterID { ParamIDs::hold, kVersion }, "Hold", false));
    correction->addChild (std::make_unique<C> (ParameterID { ParamIDs::engine, kVersion }, "Engine", StringArray { "Modern", "Classic" }, 0));

    auto voice = group ("voice", "Voice");
    voice->addChild (std::make_unique<B> (ParameterID { ParamIDs::formantOn, kVersion }, "Formant Correction", true));
    voice->addChild (std::make_unique<F> (ParameterID { ParamIDs::formant, kVersion }, "Formant Amount",
                                          NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct()));
    voice->addChild (std::make_unique<I> (ParameterID { ParamIDs::transpose, kVersion }, "Transpose", -24, 24, 0,
                                          AudioParameterIntAttributes().withLabel ("st").withStringFromValueFunction (
                                              [] (int v, int) { return (v > 0 ? "+" : "") + String (v); })));
    voice->addChild (std::make_unique<F> (ParameterID { ParamIDs::detune, kVersion }, "Detune (A4)",
                                          NormalisableRange<float> (410.0f, 470.0f, 0.1f), 440.0f,
                                          AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (
                                              [] (float v, int) { return String (v, 1); })));
    voice->addChild (std::make_unique<F> (ParameterID { ParamIDs::mix, kVersion }, "Mix",
                                          NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, pct()));

    auto notes = group ("notes", "Notes");
    notes->addChild (std::make_unique<B> (ParameterID { ParamIDs::midi, kVersion }, "MIDI Targets", false));
    notes->addChild (std::make_unique<B> (ParameterID { ParamIDs::latch, kVersion }, "Keyboard Latch", true,
                                          AudioParameterBoolAttributes().withAutomatable (false)));
    for (int pc = 0; pc < 12; ++pc)
        notes->addChild (std::make_unique<C> (ParameterID { ParamIDs::note (pc), kVersion },
                                              String ("Note ") + ac::pitchClassName (pc), noteModeChoices(), 0));

    layout.add (std::move (detection), std::move (correction), std::move (voice), std::move (notes));
    layout.add (std::make_unique<B> (ParameterID { ParamIDs::bypass, kVersion }, "Bypass", false));
    return layout;
}

std::atomic<float>* ParameterReader::get (AudioProcessorValueTreeState& s, const String& id)
{
    auto* p = s.getRawParameterValue (id);
    jassert (p != nullptr);
    return p;
}

ParameterReader::ParameterReader (AudioProcessorValueTreeState& s)
    : trackingParam (get (s, ParamIDs::tracking)), inputTypeParam (get (s, ParamIDs::inputType)),
      keyParam (get (s, ParamIDs::key)), scaleParam (get (s, ParamIDs::scale)), retuneParam (get (s, ParamIDs::retune)),
      flexParam (get (s, ParamIDs::flex)), humanizeParam (get (s, ParamIDs::humanize)), vibratoParam (get (s, ParamIDs::vibrato)),
      formantOnParam (get (s, ParamIDs::formantOn)), formantParam (get (s, ParamIDs::formant)),
      transposeParam (get (s, ParamIDs::transpose)), detuneParam (get (s, ParamIDs::detune)), mixParam (get (s, ParamIDs::mix)),
      engineParam (get (s, ParamIDs::engine)), holdParam (get (s, ParamIDs::hold)), midiParam (get (s, ParamIDs::midi)),
      bypassParam (get (s, ParamIDs::bypass))
{
    for (int pc = 0; pc < 12; ++pc) noteParams[(size_t) pc] = get (s, ParamIDs::note (pc));
}

ac::EngineParams ParameterReader::snapshot() const noexcept
{
    ac::EngineParams p;
    p.tracking = trackingParam->load() * 0.01f;
    p.inputType = (ac::InputType) jlimit (0, (int) ac::InputType::NumTypes - 1, (int) inputTypeParam->load());
    p.key = jlimit (0, 11, (int) keyParam->load());
    p.scale = (ac::ScaleType) jlimit (0, (int) ac::ScaleType::NumScales - 1, (int) scaleParam->load());
    p.retune.retuneMs = retuneParam->load();
    p.retune.flex = flexParam->load() * 0.01f;
    p.retune.humanize = humanizeParam->load() * 0.01f;
    p.retune.vibratoDb = vibratoParam->load();
    p.retune.transpose = std::round (transposeParam->load());
    p.retune.hold = holdParam->load() > 0.5f;
    p.formantOn = formantOnParam->load() > 0.5f;
    p.formantAmount = formantParam->load() * 0.01f;
    p.refHz = detuneParam->load();
    p.mix = mixParam->load() * 0.01f;
    p.engine = engineParam->load() > 0.5f ? ac::EngineMode::Classic : ac::EngineMode::Modern;
    p.midiEnabled = midiParam->load() > 0.5f;
    p.bypass = bypassParam->load() > 0.5f;
    for (int pc = 0; pc < 12; ++pc)
        p.noteModes[(size_t) pc] = (ac::NoteMode) jlimit (0, 3, (int) noteParams[(size_t) pc]->load());
    return p;
}
