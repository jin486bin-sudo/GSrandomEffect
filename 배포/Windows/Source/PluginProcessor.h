#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "GlitchEngine.h"
#include <atomic>

class GSrandomEffectProcessor : public juce::AudioProcessor
{
public:
    GSrandomEffectProcessor();
    ~GSrandomEffectProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "GSrandom effect"; }
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Called from UI thread → executed safely on audio thread
    void requestRandomize() { pendingRandom.store(true); }
    void requestSave()      { pendingSave.store(true); }
    void requestBack()      { pendingBack.store(true); }
    void requestForward()   { pendingFwd.store(true); }

    GlitchEngine& getEngine() { return engine; }

private:
    GlitchEngine engine;

    std::atomic<bool> pendingRandom { false };
    std::atomic<bool> pendingSave   { false };
    std::atomic<bool> pendingBack   { false };
    std::atomic<bool> pendingFwd    { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GSrandomEffectProcessor)
};
