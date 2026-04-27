#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "GlitchTypes.h"
#include <vector>
#include <array>

class GlitchEngine
{
public:
    static constexpr int MAX_HISTORY   = 32;
    static constexpr int MAX_CHAIN_LEN = 4;

    struct SavedState {
        std::array<StepLabel, NUM_STEPS>                 grid;
        std::array<std::vector<GlitchState>, NUM_LABELS> effects;
    };

    GlitchEngine()  = default;
    ~GlitchEngine() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void process(juce::AudioBuffer<float>& buffer, juce::AudioPlayHead* playHead);

    void randomize();
    void save();
    void goBack();
    void goForward();

    void setStep(int stepIdx, StepLabel label);
    StepLabel getStep(int stepIdx) const { return grid[stepIdx]; }

    const std::vector<GlitchState>& getEffects(int li) const { return effects[li]; }
    void setEffects(int li, const std::vector<GlitchState>& chain);

    int    getCurrentStep()  const { return currentStep; }
    double getBPM()          const { return lastBPM; }
    int    getHistorySize()  const { return (int)history.size(); }
    int    getHistoryIndex() const { return historyIdx; }

private:
    std::array<StepLabel, NUM_STEPS>                 grid    {};
    std::array<std::vector<GlitchState>, NUM_LABELS> effects {};

    std::vector<SavedState> history;
    int historyIdx = -1;

    int capSize = 0;
    std::vector<std::vector<float>> cap;
    std::vector<std::vector<float>> tempBuf;
    int capWritePos = 0;

    int    currentStep   = -1;
    int    glitchPlayPos = 0;
    int    stepSampleLen = 0;
    int    capStepStart  = 0;

    struct DerivedParams {
        int   stutterLoopLen = 1;
        int   numSlices      = 4;
        std::array<int, 16> sliceOrder {};
        float stretchFactor  = 2.0f;
        float bitDepth       = 4.0f;
        int   dsRate         = 2;
        int   gateChopLen    = 1000;
        int   gateDuty       = 500;
        float tapeSlowdown   = 1.0f;
        float scratchPeriod  = 0.5f;
        int   echoDelay      = 0;
        float echoFeedback   = 0.5f;
        float ringFreq       = 440.0f;
        int   grainLen       = 512;
        int   numGrains      = 4;
        std::array<int, 8> grainOffsets {};
        float warpDepth      = 0.0f;
        float warpRate       = 2.0f;
    };
    std::array<std::vector<DerivedParams>, NUM_LABELS> derived {};

    double lastBPM = 120.0, lastPPQ = -1.0, currentSampleRate = 44100.0;
    juce::Random rng;

    void onNewStep(int step, double samplesPerBeat);
    void processSegment(juce::AudioBuffer<float>& buf, int startSample, int numSamples, StepLabel label);
    void applyEffect(juce::AudioBuffer<float>& buf, int s, int n, int li, int ei, bool isFirst);
    void computeDerived(int li);
    int  wrapCap(int idx) const { return ((idx % capSize) + capSize) % capSize; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchEngine)
};
