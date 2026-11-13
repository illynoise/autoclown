#include "PluginProcessor.h"

#include "ui/PluginEditor.h"

using namespace juce;

AutoClownProcessor::AutoClownProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "AutoClownState", createParameterLayout())
{
    bypassParam = apvts.getParameter (ParamIDs::bypass);
    for (auto& m : momentary) m.store (-1);
    apvts.state.setProperty (StateProps::lowLatency, true, nullptr);
    apvts.state.setProperty (StateProps::presetName, factoryPresets().front().name, nullptr);
    // Report a sensible latency even before prepareToPlay (some hosts query early).
    setLatencySamples (ac::AutoClownEngine::latencyFor (44100.0, ac::LatencyMode::Low));
    startTimerHz (15);
}

AutoClownProcessor::~AutoClownProcessor() { stopTimer(); }

bool AutoClownProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return out == AudioChannelSet::mono() || out == AudioChannelSet::stereo();
}

void AutoClownProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int channels = jmax (1, getMainBusNumOutputChannels());
    engine.prepare (sampleRate, samplesPerBlock, channels,
                    lowLatency.load() ? ac::LatencyMode::Low : ac::LatencyMode::HighQuality);
    engine.setParams (reader.snapshot());
    engine.reset();
    setLatencySamples (engine.getLatencySamples());
    prepared = true;
}

void AutoClownProcessor::setMomentaryNote (int pc, int mode) noexcept
{
    if (isPositiveAndBelow (pc, 12)) momentary[(size_t) pc].store (mode);
}

void AutoClownProcessor::handleMidi (const MidiMessage& m) noexcept
{
    if (m.isNoteOn()) engine.noteOn (m.getNoteNumber());
    else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff()) engine.allNotesOff();
}

void AutoClownProcessor::process (AudioBuffer<float>& buffer, MidiBuffer& midi, bool forceBypass) noexcept
{
    ScopedNoDenormals noDenormals;
    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch) buffer.clear (ch, 0, n);

    auto params = reader.snapshot();
    params.bypass = params.bypass || forceBypass;
    for (int pc = 0; pc < 12; ++pc)
    {
        const int m = momentary[(size_t) pc].load (std::memory_order_relaxed);
        if (m >= 0) params.noteModes[(size_t) pc] = (ac::NoteMode) m;
    }
    engine.setParams (params);

    const int numCh = jmin (buffer.getNumChannels(), ac::AutoClownEngine::kMaxChannels);
    float* chans[ac::AutoClownEngine::kMaxChannels] {};

    // Split the block at MIDI events so note targets change sample-accurately.
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = jlimit (0, n, meta.samplePosition);
        if (at > pos)
        {
            for (int ch = 0; ch < numCh; ++ch) chans[ch] = buffer.getWritePointer (ch, pos);
            engine.process (chans, numCh, at - pos);
            pos = at;
        }
        handleMidi (meta.getMessage());
    }
    if (pos < n)
    {
        for (int ch = 0; ch < numCh; ++ch) chans[ch] = buffer.getWritePointer (ch, pos);
        engine.process (chans, numCh, n - pos);
    }
}

void AutoClownProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    process (buffer, midi, false);
}

void AutoClownProcessor::processBlockBypassed (AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    // Keep running the engine so the latency stays honoured and un-bypassing is seamless.
    process (buffer, midi, true);
}

void AutoClownProcessor::timerCallback()
{
    // Apply results produced on the audio thread (Learn / Auto Key) as host-visible changes.
    if (const int t = engine.learnedInputType.exchange (-1); t >= 0)
        if (auto* p = apvts.getParameter (ParamIDs::inputType))
        {
            undoManager.beginNewTransaction ("Learn");
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) t));
            p->endChangeGesture();
        }

    if (const int k = engine.detectedKey.exchange (-1); k >= 0)
    {
        undoManager.beginNewTransaction ("Auto Key");
        auto set = [this] (const char* id, float v)
        {
            if (auto* p = apvts.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (v));
                p->endChangeGesture();
            }
        };
        set (ParamIDs::key, (float) (k / 2));
        set (ParamIDs::scale, (float) ((k % 2) == 1 ? (int) ac::ScaleType::Minor : (int) ac::ScaleType::Major));
    }
}

void AutoClownProcessor::setCurrentProgram (int index)
{
    if (isPositiveAndBelow (index, getNumPrograms()))
        presets.loadPreset (index);
}

void AutoClownProcessor::applyLatencyMode()
{
    const auto mode = lowLatency.load() ? ac::LatencyMode::Low : ac::LatencyMode::HighQuality;
    if (prepared)
    {
        suspendProcessing (true);
        engine.setLatencyMode (mode);
        suspendProcessing (false);
        setLatencySamples (engine.getLatencySamples());
    }
}

void AutoClownProcessor::setLowLatency (bool shouldBeLow)
{
    apvts.state.setProperty (StateProps::lowLatency, shouldBeLow, nullptr);
    if (lowLatency.exchange (shouldBeLow) != shouldBeLow)
        applyLatencyMode();
}

Rectangle<int> AutoClownProcessor::getSavedEditorSize() const
{
    const int w = apvts.state.getProperty (StateProps::uiWidth, 0);
    const int h = apvts.state.getProperty (StateProps::uiHeight, 0);
    if (w > 0 && h > 0) return { w, h };
    const float s = settings->getDefaultScale();
    return { roundToInt (kDefaultWidth * s), roundToInt (kDefaultHeight * s) };
}

void AutoClownProcessor::setSavedEditorSize (int w, int h)
{
    apvts.state.setProperty (StateProps::uiWidth, w, nullptr);
    apvts.state.setProperty (StateProps::uiHeight, h, nullptr);
}

void AutoClownProcessor::getStateInformation (MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty (StateProps::lowLatency, lowLatency.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void AutoClownProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    apvts.replaceState (ValueTree::fromXml (*xml));

    // APVTS skips parameters whose stored value already matches after quantisation (e.g. a
    // bool holding a raw 0.63); push every value explicitly so restore is exact.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<RangedAudioParameter*> (p))
        {
            const auto child = apvts.state.getChildWithProperty ("id", rp->paramID);
            if (child.isValid() && child.hasProperty ("value"))
                rp->setValueNotifyingHost (rp->convertTo0to1 ((float) child.getProperty ("value")));
        }

    presets.syncFromState();
    const bool low = apvts.state.getProperty (StateProps::lowLatency, true);
    if (lowLatency.exchange (low) != low)
    {
        if (MessageManager::getInstance()->isThisTheMessageThread()) applyLatencyMode();
        else MessageManager::callAsync ([safe = WeakReference<AutoClownProcessor> (this)]
                                        { if (auto* p = safe.get()) p->applyLatencyMode(); });
    }
}

AudioProcessorEditor* AutoClownProcessor::createEditor() { return new AutoClownEditor (*this); }

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AutoClownProcessor(); }
