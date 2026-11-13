// Renders the editor to PNG at several sizes after feeding it a sung vowel.
// Usage: AutoClownSnapshot <outputDir>
#include "TestSignals.h"
#include "ui/PluginEditor.h"

using namespace juce;

namespace
{
void feed (AutoClownProcessor& p, double fs, double hz, double seconds)
{
    auto x = actest::vowel (fs, hz, seconds, 5.5, 25.0, -50.0);
    AudioBuffer<float> buf (2, 512);
    MidiBuffer midi;
    for (size_t i = 0; i + 512 <= x.size(); i += 512)
    {
        for (int ch = 0; ch < 2; ++ch) buf.copyFrom (ch, 0, x.data() + i, 512);
        p.processBlock (buf, midi);
    }
}

void shoot (AutoClownEditor& ed, const File& file, float imageScale)
{
    ed.refreshFromProcessor();
    const auto img = ed.createComponentSnapshot (ed.getLocalBounds(), true, imageScale);
    file.deleteFile();
    FileOutputStream out (file);
    PNGImageFormat().writeImageToStream (img, out);
    std::printf ("wrote %s (%dx%d)\n", file.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
}

void setParam (AutoClownProcessor& p, const String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}
} // namespace

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI gui;
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    AutoClownProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);

    // Natural lead: C major, slightly sharp A3, with a removed and a bypassed note.
    setParam (proc, ParamIDs::retune, 45.0f);
    setParam (proc, ParamIDs::humanize, 30.0f);
    setParam (proc, ParamIDs::flex, 15.0f);
    setParam (proc, ParamIDs::note (5), 3.0f); // F removed explicitly
    setParam (proc, ParamIDs::note (7), 2.0f); // G bypass
    feed (proc, 48000.0, 220.0 * std::exp2 (34.0 / 1200.0), 1.0);
    std::unique_ptr<AudioProcessorEditor> base (proc.createEditor());
    auto& ed = dynamic_cast<AutoClownEditor&> (*base);
    ed.setScale (1.0f);
    feed (proc, 48000.0, 220.0 * std::exp2 (34.0 / 1200.0), 0.3);
    shoot (ed, outDir.getChildFile ("autoclown-100.png"), 2.0f);

    // Hard tune: honk lights up.
    setParam (proc, ParamIDs::retune, 0.0f);
    feed (proc, 48000.0, 261.63 * std::exp2 (-28.0 / 1200.0), 0.5);
    shoot (ed, outDir.getChildFile ("autoclown-hardtune.png"), 2.0f);

    for (float s : { 0.5f, 2.0f })
    {
        ed.setScale (s);
        feed (proc, 48000.0, 261.63 * std::exp2 (-28.0 / 1200.0), 0.2);
        shoot (ed, outDir.getChildFile ("autoclown-" + String (roundToInt (s * 100)) + ".png"), 1.0f);
    }
    std::printf ("saved size in state: %s x %s\n", proc.apvts.state.getProperty (StateProps::uiWidth).toString().toRawUTF8(),
                 proc.apvts.state.getProperty (StateProps::uiHeight).toString().toRawUTF8());
    base.reset();
    return 0;
}
