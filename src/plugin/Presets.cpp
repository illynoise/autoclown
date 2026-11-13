#include "Presets.h"

#include "Parameters.h"

using namespace juce;

const std::vector<FactoryPreset>& factoryPresets()
{
    namespace P = ParamIDs;
    static const std::vector<FactoryPreset> presets = {
        { "Big Top - Default", "Balanced starting point: fast, natural correction.",
          {} },
        { "Tightrope - Gentle Fix", "Transparent touch-up; leaves expression and scoops alone.",
          { { P::retune, 80.0f }, { P::humanize, 40.0f }, { P::flex, 40.0f }, { P::tracking, 50.0f } } },
        { "Ringmaster - Natural Lead", "Confident lead vocal tuning that still sounds human.",
          { { P::retune, 45.0f }, { P::humanize, 30.0f }, { P::flex, 15.0f } } },
        { "Full Clown - Hard Tune", "Instant snap between notes: the robotic effect.",
          { { P::retune, 0.0f }, { P::humanize, 0.0f }, { P::flex, 0.0f } } },
        { "Trapeze Harmonies - Backing Vocals", "Tight, steady stacks: fast retune, tamed vibrato.",
          { { P::retune, 15.0f }, { P::humanize, 10.0f }, { P::vibrato, -6.0f } } },
        { "Cannonball - Fast Pop Snap", "Modern pop polish: quick but not robotic.",
          { { P::retune, 8.0f }, { P::humanize, 25.0f } } },
        { "Lion Tamer - Vibrato Tamer", "Calms a wide, wobbly vibrato while keeping the tone.",
          { { P::retune, 30.0f }, { P::humanize, 50.0f }, { P::vibrato, -9.0f } } },
        { "Sideshow Barker - Wide Vibrato", "Exaggerates the singer's own vibrato for drama.",
          { { P::retune, 60.0f }, { P::humanize, 40.0f }, { P::vibrato, 6.0f } } },
        { "Stilt Walker - Octave Down Giant", "One octave down with formants moving too: a towering giant.",
          { { P::retune, 20.0f }, { P::transpose, -12.0f }, { P::formantOn, 0.0f } } },
        { "Helium Balloon - Chipmunk Up", "One octave up, formants following: squeaky clown voice.",
          { { P::retune, 20.0f }, { P::transpose, 12.0f }, { P::formantOn, 0.0f } } },
        { "Calliope - Robotic Organ", "Hard-tuned Classic engine with vibrato removed: steam-organ robot.",
          { { P::retune, 0.0f }, { P::engine, 1.0f }, { P::vibrato, -12.0f } } },
        { "Juggler - MIDI Targeted Lead", "Notes follow incoming MIDI. Route a MIDI track to Auto Clown.",
          { { P::retune, 10.0f }, { P::midi, 1.0f }, { P::humanize, 15.0f } } },
        { "Bass Clown - Bass Instrument Fix", "Low-range tracking for fretless and synth bass lines.",
          { { P::inputType, 4.0f }, { P::retune, 30.0f }, { P::tracking, 40.0f } } },
    };
    return presets;
}

PresetManager::PresetManager (AudioProcessorValueTreeState& s) : apvts (s)
{
    rescanUserPresets();
}

File PresetManager::getUserPresetDirectory()
{
   #if JUCE_MAC
    return File::getSpecialLocation (File::userHomeDirectory).getChildFile ("Library/Audio/Presets/Auto Clown");
   #else
    return File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Auto Clown/Presets");
   #endif
}

void PresetManager::rescanUserPresets()
{
    userPresets.clear();
    const auto dir = getUserPresetDirectory();
    if (dir.isDirectory())
        for (const auto& f : dir.findChildFiles (File::findFiles, false, String ("*") + kExtension))
            userPresets.add (f);
    std::sort (userPresets.begin(), userPresets.end(),
               [] (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()) < 0; });
}

String PresetManager::getPresetName (int i) const
{
    const auto& f = factoryPresets();
    if (isPositiveAndBelow (i, (int) f.size())) return f[(size_t) i].name;
    i -= (int) f.size();
    return isPositiveAndBelow (i, userPresets.size()) ? userPresets[i].getFileNameWithoutExtension() : String();
}

String PresetManager::getCurrentName() const
{
    const auto stored = apvts.state.getProperty (StateProps::presetName).toString();
    return stored.isNotEmpty() ? stored : getPresetName (currentIndex);
}

void PresetManager::setParam (const String& id, float plain)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
        p->endChangeGesture();
    }
}

void PresetManager::resetCharacterParams()
{
    namespace P = ParamIDs;
    for (auto* id : { P::retune, P::flex, P::humanize, P::vibrato, P::formantOn, P::formant,
                      P::transpose, P::mix, P::engine, P::hold, P::tracking, P::midi })
        if (auto* p = apvts.getParameter (id))
            setParam (id, p->convertFrom0to1 (p->getDefaultValue()));
}

void PresetManager::loadPreset (int index)
{
    const auto& f = factoryPresets();
    if (! isPositiveAndBelow (index, getNumPresets())) return;
    currentIndex = index;

    if (index < (int) f.size())
    {
        resetCharacterParams();
        for (const auto& [id, v] : f[(size_t) index].values) setParam (id, v);
        apvts.state.setProperty (StateProps::presetName, f[(size_t) index].name, nullptr);
        return;
    }

    const auto file = userPresets[index - (int) f.size()];
    if (auto xml = parseXML (file))
    {
        for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
        {
            const auto id = e->getStringAttribute ("id");
            if (id == ParamIDs::bypass || id == ParamIDs::latch) continue;
            if (e->hasAttribute ("value")) setParam (id, (float) e->getDoubleAttribute ("value"));
        }
        apvts.state.setProperty (StateProps::presetName, file.getFileNameWithoutExtension(), nullptr);
    }
}

bool PresetManager::saveUserPreset (const String& rawName)
{
    const auto name = File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;
    const auto dir = getUserPresetDirectory();
    if (! dir.createDirectory()) return false;

    XmlElement xml ("AUTOCLOWN_PRESET");
    xml.setAttribute ("version", 1);
    for (auto* p : apvts.processor.getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            if (rp->paramID == ParamIDs::bypass) continue;
            auto* e = xml.createNewChildElement ("PARAM");
            e->setAttribute ("id", rp->paramID);
            e->setAttribute ("value", rp->convertFrom0to1 (rp->getValue()));
        }

    const auto file = dir.getChildFile (name + kExtension);
    if (! xml.writeTo (file)) return false;
    rescanUserPresets();
    currentIndex = (int) factoryPresets().size() + userPresets.indexOf (file);
    apvts.state.setProperty (StateProps::presetName, name, nullptr);
    return true;
}

void PresetManager::randomise()
{
    namespace P = ParamIDs;
    auto r = [this] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    // Musically sane ranges: biased towards usable settings rather than uniform chaos.
    setParam (P::retune, rng.nextFloat() < 0.25f ? 0.0f : std::pow (rng.nextFloat(), 2.0f) * 200.0f);
    setParam (P::humanize, r (0.0f, 70.0f));
    setParam (P::flex, rng.nextFloat() < 0.5f ? 0.0f : r (0.0f, 60.0f));
    setParam (P::vibrato, std::round (r (-8.0f, 6.0f)));
    setParam (P::formantOn, rng.nextFloat() < 0.85f ? 1.0f : 0.0f);
    setParam (P::formant, r (70.0f, 100.0f));
    setParam (P::engine, rng.nextFloat() < 0.2f ? 1.0f : 0.0f);
    const float t = rng.nextFloat();
    setParam (P::transpose, t < 0.8f ? 0.0f : (t < 0.9f ? 12.0f : -12.0f));
    setParam (P::mix, r (80.0f, 100.0f));
    apvts.state.setProperty (StateProps::presetName, "Random Act #" + String (rng.nextInt ({ 100, 1000 })), nullptr);
}

void PresetManager::syncFromState()
{
    const auto name = apvts.state.getProperty (StateProps::presetName).toString();
    for (int i = 0; i < getNumPresets(); ++i)
        if (getPresetName (i) == name) { currentIndex = i; return; }
}
