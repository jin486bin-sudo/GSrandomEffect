#include "PluginProcessor.h"
#include "PluginEditor.h"

GSrandomEffectProcessor::GSrandomEffectProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput ("Input",  juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{}

bool GSrandomEffectProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() &&
        layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void GSrandomEffectProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    engine.prepare(sampleRate, samplesPerBlock);
}

void GSrandomEffectProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& /*midi*/)
{
    juce::ScopedNoDenormals noDenormals;

    if (pendingRandom.exchange(false)) engine.randomize();
    if (pendingSave  .exchange(false)) engine.save();
    if (pendingBack  .exchange(false)) engine.goBack();
    if (pendingFwd   .exchange(false)) engine.goForward();

    engine.process(buffer, getPlayHead());
}

juce::AudioProcessorEditor* GSrandomEffectProcessor::createEditor()
{
    return new GSrandomEffectEditor(*this);
}

// ── State persistence ─────────────────────────────────────────────────────────

void GSrandomEffectProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream(destData, true);

    // Grid (16 bytes)
    for (int i = 0; i < NUM_STEPS; ++i)
        stream.writeByte((uint8_t)engine.getStep(i));

    // Effects: for each slot → count (1 byte) + per effect: type(1) + intensity(4) + seed(4)
    for (int li = 0; li < NUM_LABELS; ++li)
    {
        const auto& chain = engine.getEffects(li);
        stream.writeByte((uint8_t)juce::jmin((int)chain.size(), GlitchEngine::MAX_CHAIN_LEN));
        for (int ei = 0; ei < juce::jmin((int)chain.size(), GlitchEngine::MAX_CHAIN_LEN); ++ei)
        {
            stream.writeByte((uint8_t)chain[ei].type);
            stream.writeFloat(chain[ei].intensity);
            stream.writeInt((int)chain[ei].seed);
        }
    }
}

void GSrandomEffectProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    juce::MemoryInputStream stream(data, (size_t)sizeInBytes, false);

    if (stream.getNumBytesRemaining() < NUM_STEPS + NUM_LABELS) return;

    for (int i = 0; i < NUM_STEPS; ++i)
        engine.setStep(i, (StepLabel)stream.readByte());

    for (int li = 0; li < NUM_LABELS; ++li)
    {
        int count = (int)stream.readByte();
        count = juce::jmax(0, juce::jmin(count, GlitchEngine::MAX_CHAIN_LEN));

        std::vector<GlitchState> chain;
        for (int ei = 0; ei < count; ++ei)
        {
            if (stream.getNumBytesRemaining() < 9) break;
            GlitchState st;
            int typeVal = (int)stream.readByte();
            if (typeVal >= 0 && typeVal < (int)GlitchType::Count)
                st.type = (GlitchType)typeVal;
            st.intensity = stream.readFloat();
            st.seed      = (uint32_t)stream.readInt();
            chain.push_back(st);
        }
        engine.setEffects(li, chain);
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GSrandomEffectProcessor();
}
