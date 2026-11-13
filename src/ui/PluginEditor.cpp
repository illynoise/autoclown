#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr float W = (float) AutoClownProcessor::kDefaultWidth;
constexpr float H = (float) AutoClownProcessor::kDefaultHeight;
} // namespace

AutoClownEditor::AutoClownEditor (AutoClownProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);

    for (auto* c : std::initializer_list<Component*> {
             &logo, &presetBar, &undoButton, &redoButton, &settingsButton, &bypassButton,
             &inputTypeBox, &keyBox, &scaleBox, &learnButton, &autoKeyButton, &tracking, &modernButton, &classicButton,
             &holdToggle, &midiToggle, &retune, &humanize, &flex, &vibrato, &honk, &formantToggle, &formant,
             &transpose, &detune, &mix, &meter, &keyboard, &latchToggle })
        addAndMakeVisible (c);

    // Combo items must match the parameter choices before the attachments sync.
    auto fill = [] (ComboBox& b, const StringArray& items) { b.addItemList (items, 1); };
    fill (inputTypeBox, inputTypeChoices());
    fill (keyBox, keyChoices());
    fill (scaleBox, scaleChoices());
    inputTypeAttach = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ParamIDs::inputType, inputTypeBox);
    keyAttach = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ParamIDs::key, keyBox);
    scaleAttach = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, ParamIDs::scale, scaleBox);
    inputTypeBox.setTooltip ("Sets the pitch detector's frequency range");
    keyBox.setTooltip ("Key");
    scaleBox.setTooltip ("Scale");

    for (auto* b : { &learnButton, &autoKeyButton }) b->getProperties().set (Circus::ticketStyle, true);
    learnButton.setTooltip ("Sing for a few seconds: Auto Clown picks the best Input Type");
    autoKeyButton.setTooltip ("Sing or play for ~8 seconds: Auto Clown estimates the key and scale");
    learnButton.onClick = [this] { proc.startLearn(); };
    autoKeyButton.onClick = [this] { proc.startAutoKey(); };

    for (auto* b : { &modernButton, &classicButton })
    {
        b->getProperties().set (Circus::segmentStyle, true);
        b->setClickingTogglesState (false);
    }
    modernButton.setTooltip ("Modern: formant-preserving PSOLA, natural sound");
    classicButton.setTooltip ("Classic: vintage delay-line shifter, more character");
    engineAttach = std::make_unique<ParameterAttachment> (
        *proc.apvts.getParameter (ParamIDs::engine),
        [this] (float v) { modernButton.setToggleState (v < 0.5f, dontSendNotification);
                           classicButton.setToggleState (v >= 0.5f, dontSendNotification); },
        &proc.undoManager);
    engineAttach->sendInitialUpdate();
    modernButton.onClick = [this] { engineAttach->setValueAsCompleteGesture (0.0f); };
    classicButton.onClick = [this] { engineAttach->setValueAsCompleteGesture (1.0f); };

    holdToggle.setTooltip ("Freeze the current corrected pitch");
    midiToggle.setTooltip ("Incoming MIDI notes choose the target notes");
    formantToggle.setTooltip ("Preserve vocal character when shifting");
    latchToggle.setTooltip ("On: keyboard clicks stick. Off: changes last only while held");

    bypassButton.setClickingTogglesState (true);
    undoButton.onClick = [this] { proc.undoManager.undo(); };
    redoButton.onClick = [this] { proc.undoManager.redo(); };
    settingsButton.onClick = [this] { showSettingsMenu(); };
    presetBar.onPresetChanging = [this] { proc.undoManager.beginNewTransaction ("Preset"); };

    addMouseListener (this, true); // start a fresh undo transaction on each gesture

    // Resizing: fixed aspect ratio, 50 %..200 %, custom striped corner + host resizing.
    setResizable (true, false);
    setResizeLimits ((int) (W * 0.5f), (int) (H * 0.5f), (int) (W * 2.0f), (int) (H * 2.0f));
    getConstrainer()->setFixedAspectRatio (kAspect);
    resizer = std::make_unique<CircusResizeHandle> (this, getConstrainer());
    addAndMakeVisible (*resizer);

    const auto saved = proc.getSavedEditorSize();
    const int w = jlimit ((int) (W * 0.5f), (int) (W * 2.0f), saved.getWidth());
    setSize (w, roundToInt ((float) w / kAspect));
    initialised = true;
    resized();
}

AutoClownEditor::~AutoClownEditor()
{
    stopTimer();
    removeMouseListener (this);
    setLookAndFeel (nullptr);
}

void AutoClownEditor::setScale (float s)
{
    s = jlimit (0.5f, 2.0f, s);
    setSize (roundToInt (W * s), roundToInt (H * s));
}

Rectangle<float> AutoClownEditor::designF (float x, float y, float w, float h) const
{
    return { ox + x * unit, oy + y * unit, w * unit, h * unit };
}

Rectangle<int> AutoClownEditor::design (float x, float y, float w, float h) const
{
    return designF (x, y, w, h).toNearestInt();
}

void AutoClownEditor::resized()
{
    // Uniform scale; if a host forces a slightly different aspect, centre the layout.
    unit = jmin ((float) getWidth() / W, (float) getHeight() / H);
    ox = ((float) getWidth() - W * unit) * 0.5f;
    oy = ((float) getHeight() - H * unit) * 0.5f;
    lnf.setMenuScale (unit);

    headerArea = designF (0, 0, W, 98);
    logo.setBounds (design (14, 6, 330, 66));
    presetBar.setBounds (design (362, 18, 396, 44));
    undoButton.setBounds (design (776, 20, 40, 40));
    redoButton.setBounds (design (822, 20, 40, 40));
    settingsButton.setBounds (design (884, 20, 40, 40));
    bypassButton.setBounds (design (940, 20, 40, 40));

    detectorPanel = designF (14, 114, 290, 298);
    inputTypeBox.setBounds (design (30, 152, 168, 28));
    learnButton.setBounds (design (204, 149, 90, 34));
    keyBox.setBounds (design (30, 205, 70, 28));
    scaleBox.setBounds (design (106, 205, 188, 28));
    autoKeyButton.setBounds (design (28, 243, 124, 34));
    modernButton.setBounds (design (30, 305, 62, 26));
    classicButton.setBounds (design (96, 305, 62, 26));
    holdToggle.setBounds (design (30, 342, 130, 24));
    midiToggle.setBounds (design (30, 374, 130, 24));
    tracking.setBounds (design (166, 240, 128, 160));

    meter.setBounds (design (312, 102, 376, 314));
    honk.setBounds (design (612, 104, 84, 30));

    correctionPanel = designF (696, 114, 290, 298);
    retune.setBounds (design (708, 136, 132, 134));
    humanize.setBounds (design (846, 136, 132, 134));
    flex.setBounds (design (708, 272, 132, 134));
    vibrato.setBounds (design (846, 272, 132, 134));

    sideshowPanel = designF (14, 428, 972, 104);
    formantToggle.setBounds (design (34, 468, 170, 26));
    formant.setBounds (design (208, 446, 120, 82));
    transpose.setBounds (design (340, 446, 120, 82));
    detune.setBounds (design (472, 446, 120, 82));
    mix.setBounds (design (604, 446, 120, 82));
    latencyBox = designF (750, 456, 220, 56);

    keyboardPanel = designF (14, 544, 972, 68);
    latchToggle.setBounds (design (28, 578, 108, 24));
    keyboard.setBounds (design (150, 548, 830, 60));

    // resized() can run from inside the constructor (setResizeLimits), before the handle exists.
    if (resizer != nullptr)
    {
        const int rs = roundToInt (18.0f * unit);
        resizer->setBounds (getWidth() - rs, getHeight() - rs, rs, rs);
        resizer->toFront (false);
    }

    // Remember the size with the session now; as the global default once dragging settles.
    if (initialised)
    {
        proc.setSavedEditorSize (getWidth(), getHeight());
        startTimer (700);
    }
}

void AutoClownEditor::timerCallback()
{
    stopTimer();
    proc.globalSettings().setDefaultScale ((float) getWidth() / W);
}

void AutoClownEditor::mouseDown (const MouseEvent&)
{
    proc.undoManager.beginNewTransaction();
}

void AutoClownEditor::caption (Graphics& g, const String& text, Rectangle<float> r) const
{
    g.setColour (Circus::gold);
    g.setFont (Circus::sans (r.getHeight(), true));
    g.drawText (text, r, Justification::bottomLeft, false);
}

void AutoClownEditor::drawCanopy (Graphics& g, Rectangle<float> area) const
{
    using namespace Circus;
    const float stripeW = 50.0f * unit;
    const float bodyH = area.getHeight() * 0.8f;
    const int n = (int) std::ceil (area.getWidth() / stripeW) + 1;

    // Scalloped valance: each stripe ends in a semicircle.
    for (int i = 0; i < n; ++i)
    {
        const float x = area.getX() + (float) i * stripeW;
        Path p;
        p.addRectangle (x, area.getY(), stripeW, bodyH);
        p.addEllipse (x, area.getY() + bodyH - stripeW * 0.36f, stripeW, stripeW * 0.72f);
        g.setColour (charcoal.withAlpha (0.45f));
        g.fillPath (p, AffineTransform::translation (0.0f, 3.0f * unit));
        g.setColour (i % 2 == 0 ? red : cream);
        g.fillPath (p);
    }
    // Shading toward the top for depth
    g.setGradientFill (ColourGradient (charcoal.withAlpha (0.35f), 0.0f, area.getY(), charcoal.withAlpha (0.0f), 0.0f, area.getY() + bodyH * 0.6f, false));
    g.fillRect (area.withHeight (bodyH));
    // Gold trim
    g.setColour (gold);
    g.fillRect (area.getX(), area.getY() + bodyH - 2.0f * unit, area.getWidth(), 3.0f * unit);
}

void AutoClownEditor::paint (Graphics& g)
{
    using namespace Circus;
    // Background: deep purple tent interior with faint stripes
    g.setGradientFill (ColourGradient (purpleDeep, 0.0f, 0.0f, charcoal, 0.0f, (float) getHeight(), false));
    g.fillAll();
    {
        const float sw = 50.0f * unit;
        g.setColour (purple.withAlpha (0.25f));
        for (float x = ox; x < (float) getWidth(); x += sw * 2.0f) g.fillRect (x, 0.0f, sw, (float) getHeight());
    }

    drawCanopy (g, headerArea);
    // Plaques behind logo for contrast
    {
        auto plaque = designF (8, 4, 342, 70);
        g.setColour (charcoal.withAlpha (0.55f));
        g.fillRoundedRectangle (plaque.translated (0.0f, 3.0f * unit), 12.0f * unit);
        g.setColour (blue);
        g.fillRoundedRectangle (plaque, 12.0f * unit);
        g.setColour (gold);
        g.drawRoundedRectangle (plaque.reduced (2.0f * unit), 12.0f * unit, 2.0f * unit);
        auto icons = designF (768, 14, 222, 52);
        g.setColour (charcoal.withAlpha (0.75f));
        g.fillRoundedRectangle (icons, 26.0f * unit);
        g.setColour (gold);
        g.drawRoundedRectangle (icons.reduced (1.0f * unit), 26.0f * unit, 1.5f * unit);
    }

    drawPanel (g, detectorPanel, "The Detector", unit);
    drawPanel (g, correctionPanel, "The Correction", unit);
    drawPanel (g, sideshowPanel, "The Voice", unit);
    drawPanel (g, keyboardPanel, {}, unit);

    caption (g, "INPUT TYPE", designF (31, 134, 160, 14));
    caption (g, "KEY", designF (31, 187, 60, 14));
    caption (g, "SCALE", designF (107, 187, 100, 14));
    caption (g, "ENGINE", designF (31, 287, 100, 14));
    caption (g, "NOTES", designF (29, 556, 100, 16));

    // Latency / status readout
    g.setColour (charcoal.withAlpha (0.8f));
    g.fillRoundedRectangle (latencyBox, 10.0f * unit);
    g.setColour (gold);
    g.drawRoundedRectangle (latencyBox.reduced (unit), 10.0f * unit, 1.5f * unit);
    const double sr = proc.getSampleRate();
    const double ms = sr > 0 ? 1000.0 * proc.getLatencySamples() / sr : 0.0;
    auto lb = latencyBox.reduced (10.0f * unit, 6.0f * unit);
    g.setFont (sans (15.0f * unit, true));
    g.setColour (cream);
    g.drawText (proc.isLowLatency() ? "LOW LATENCY" : "HIGH QUALITY", lb.removeFromTop (lb.getHeight() * 0.5f), Justification::centredLeft);
    g.setColour (gold);
    g.drawText (String (ms, 1) + " ms  ·  " + (sr > 0 ? String (sr / 1000.0, 1) + " kHz" : String ("--")), lb, Justification::centredLeft);
}

void AutoClownEditor::onFrame()
{
    const double now = Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = lastFrameTime > 0.0 ? (float) jlimit (0.001, 0.1, now - lastFrameTime) : 1.0f / 60.0f;
    lastFrameTime = now;

    // Drain the lock-free FIFO; the newest frame wins.
    ac::MeterFrame f, latest;
    bool any = false;
    while (proc.meterFifo().pop (f)) { latest = f; any = true; }
    meter.update (any ? &latest : nullptr, dt);
    if (any) keyboard.setLive (latest.targetNote, latest.heldMidiMask, latest.voiced != 0);

    honk.setActive (proc.apvts.getRawParameterValue (ParamIDs::retune)->load() < 0.05f);

    auto progressText = [] (float prog, const char* idle)
    {
        return prog < 0.0f ? String (idle) : "Listening " + String (roundToInt (prog * 100.0f)) + "%";
    };
    const float lp = proc.getLearnProgress(), kp = proc.getKeyProgress();
    learnButton.setButtonText (lp < 0.0f ? "Learn" : String (roundToInt (lp * 100.0f)) + "%");
    learnButton.setToggleState (lp >= 0.0f, dontSendNotification);
    autoKeyButton.setButtonText (progressText (kp, "Auto Key"));
    autoKeyButton.setToggleState (kp >= 0.0f, dontSendNotification);

    // Keyboard needs refreshing when key/scale/note params change (incl. automation/undo).
    int hash = (int) proc.apvts.getRawParameterValue (ParamIDs::key)->load() * 31
             + (int) proc.apvts.getRawParameterValue (ParamIDs::scale)->load() * 997;
    for (int pc = 0; pc < 12; ++pc)
        hash = hash * 5 + (int) proc.apvts.getRawParameterValue (ParamIDs::note (pc))->load();
    if (hash != lastNoteHash) { lastNoteHash = hash; keyboard.refreshStates(); }

    const auto name = proc.presets.getCurrentName();
    if (name != lastPresetName) { lastPresetName = name; presetBar.refresh(); }

    undoButton.setEnabled (proc.undoManager.canUndo());
    redoButton.setEnabled (proc.undoManager.canRedo());
    undoButton.setAlpha (proc.undoManager.canUndo() ? 1.0f : 0.45f);
    redoButton.setAlpha (proc.undoManager.canRedo() ? 1.0f : 0.45f);
}

void AutoClownEditor::showSettingsMenu()
{
    PopupMenu sizes;
    const float current = (float) getWidth() / W;
    for (int pct : { 75, 100, 125, 150, 200 })
        sizes.addItem (String (pct) + "%", true, std::abs (current - (float) pct / 100.0f) < 0.01f,
                       [safe = SafePointer<AutoClownEditor> (this), pct] { if (safe) safe->setScale ((float) pct / 100.0f); });

    PopupMenu latency;
    latency.addItem ("Low Latency (5 ms)", true, proc.isLowLatency(), [this] { proc.setLowLatency (true); repaint(); });
    latency.addItem ("High Quality (30 ms)", true, ! proc.isLowLatency(), [this] { proc.setLowLatency (false); repaint(); });

    PopupMenu m;
    m.addSectionHeader ("Auto Clown");
    m.addSubMenu ("Window Size", sizes);
    m.addItem ("Use current size for new instances", [this] { proc.globalSettings().setDefaultScale ((float) getWidth() / W); });
    m.addSubMenu ("Latency", latency);
    m.addSeparator();
    m.addItem ("Reset keyboard to key/scale", [this]
    {
        proc.undoManager.beginNewTransaction ("Reset keyboard");
        for (int pc = 0; pc < 12; ++pc)
            if (auto* p = proc.apvts.getParameter (ParamIDs::note (pc))) p->setValueNotifyingHost (0.0f);
    });
    m.addItem ("Open preset folder", [] { PresetManager::getUserPresetDirectory().createDirectory();
                                          PresetManager::getUserPresetDirectory().revealToUser(); });
    m.addSeparator();
    m.addItem ("About Auto Clown", [this] { showAbout(); });
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (&settingsButton));
}

void AutoClownEditor::showAbout()
{
    AlertWindow::showAsync (MessageBoxOptions()
                                .withTitle ("Auto Clown " JucePlugin_VersionString)
                                .withMessage ("Real-time pitch correction under the big top.\n\n"
                                              "Detector: two-stage YIN with octave guards.\n"
                                              "Engines: formant-preserving PSOLA (Modern) and a\n"
                                              "pitch-synchronous delay-line shifter (Classic).\n\n"
                                              "Fonts: Rye (Sorkin Type) and Barlow Semi Condensed\n"
                                              "(The Barlow Project Authors), SIL Open Font License 1.1.")
                                .withButton ("Bravo!", 0)
                                .withAssociatedComponent (this),
                            nullptr);
}
