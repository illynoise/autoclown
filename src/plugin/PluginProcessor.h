// Auto Clown — AudioProcessor.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "GlobalSettings.h"
#include "Parameters.h"
#include "Presets.h"

class AutoClownProcessor final : public juce::AudioProcessor,
                                 private juce::Timer
{
public:
    static constexpr int kDefaultWidth = 1000;
    static constexpr int kDefaultHeight = 620;

    AutoClownProcessor();
    ~AutoClownProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return presets.getNumFactoryPresets(); }
    int getCurrentProgram() override { return juce::jlimit (0, getNumPrograms() - 1, presets.getCurrentIndex()); }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override { return presets.getPresetName (index); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // --- editor-facing API (message thread) --------------------------------
    juce::UndoManager undoManager { 30000, 30 };
    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };
    GlobalSettings& globalSettings() { return *settings; }

    ac::SpscFifo<ac::MeterFrame, 512>& meterFifo() noexcept { return engine.meterFifo; }
    void startLearn() noexcept { engine.requestLearn(); }
    void startAutoKey() noexcept { engine.requestAutoKey(); }
    float getLearnProgress() const noexcept { return engine.learnProgress.load(); }
    float getKeyProgress() const noexcept { return engine.keyProgress.load(); }

    /** Momentary (non-latched) keyboard override: mode -1 clears. */
    void setMomentaryNote (int pitchClass, int mode) noexcept;

    bool isLowLatency() const { return lowLatency.load(); }
    void setLowLatency (bool shouldBeLow);

    /** Saved editor size (from the session, else the global default). */
    juce::Rectangle<int> getSavedEditorSize() const;
    void setSavedEditorSize (int width, int height);

private:
    void process (juce::AudioBuffer<float>&, juce::MidiBuffer&, bool forceBypass) noexcept;
    void handleMidi (const juce::MidiMessage&) noexcept;
    void timerCallback() override;
    void applyLatencyMode();

    juce::SharedResourcePointer<GlobalSettings> settings;
    ParameterReader reader { apvts };
    juce::AudioProcessorParameter* bypassParam = nullptr;
    ac::AutoClownEngine engine;
    bool prepared = false;
    std::atomic<bool> lowLatency { true };
    std::array<std::atomic<int>, 12> momentary;

    JUCE_DECLARE_WEAK_REFERENCEABLE (AutoClownProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AutoClownProcessor)
};
