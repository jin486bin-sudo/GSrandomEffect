#include "GlitchEngine.h"
#include <cmath>
#include <algorithm>

// ── Prepare ───────────────────────────────────────────────────────────────────

void GlitchEngine::prepare(double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate;
    capSize           = (int)(sampleRate * 4.0);
    cap.assign(2, std::vector<float>(capSize, 0.0f));
    tempBuf.assign(2, std::vector<float>(juce::jmax(1, maxBlockSize), 0.0f));
    capWritePos   = 0;
    currentStep   = -1;
    glitchPlayPos = 0;
    lastPPQ       = -1.0;

    // Pre-reserve vectors to prevent reallocation during audio thread writes,
    // which would race with UI thread reads.
    for (int li = 0; li < NUM_LABELS; ++li) {
        effects[li].reserve(MAX_CHAIN_LEN);
        derived[li].reserve(MAX_CHAIN_LEN);
    }
}

// ── Grid / effects ────────────────────────────────────────────────────────────

void GlitchEngine::setStep(int idx, StepLabel label)
{
    if (idx >= 0 && idx < NUM_STEPS) grid[idx] = label;
}

void GlitchEngine::setEffects(int li, const std::vector<GlitchState>& chain)
{
    if (li >= 0 && li < NUM_LABELS) { effects[li] = chain; computeDerived(li); }
}

// ── Randomize ─────────────────────────────────────────────────────────────────

void GlitchEngine::randomize()
{
    for (int li = 0; li < NUM_LABELS; ++li)
    {
        effects[li].clear();
        int count = 1 + rng.nextInt(3); // 1-3 effects per slot
        for (int e = 0; e < count; ++e)
        {
            GlitchState st;
            st.seed      = (uint32_t)rng.nextInt64();
            st.type      = (GlitchType)(rng.nextInt((int)GlitchType::Count));
            st.intensity = 0.25f + rng.nextFloat() * 0.75f;
            effects[li].push_back(st);
        }
        computeDerived(li);
    }
}

// ── History ───────────────────────────────────────────────────────────────────

void GlitchEngine::save()
{
    if (historyIdx < (int)history.size() - 1)
        history.erase(history.begin() + historyIdx + 1, history.end());

    SavedState s;
    s.grid    = grid;
    s.effects = effects;
    history.push_back(s);
    if ((int)history.size() > MAX_HISTORY) history.erase(history.begin());
    historyIdx = (int)history.size() - 1;
}

void GlitchEngine::goBack()
{
    if (history.empty() || historyIdx <= 0) return;
    --historyIdx;
    grid    = history[historyIdx].grid;
    effects = history[historyIdx].effects;
    for (int li = 0; li < NUM_LABELS; ++li) computeDerived(li);
}

void GlitchEngine::goForward()
{
    if (historyIdx >= (int)history.size() - 1) return;
    ++historyIdx;
    grid    = history[historyIdx].grid;
    effects = history[historyIdx].effects;
    for (int li = 0; li < NUM_LABELS; ++li) computeDerived(li);
}

// ── Derived params ────────────────────────────────────────────────────────────

void GlitchEngine::computeDerived(int li)
{
    derived[li].resize(effects[li].size());
    for (int ei = 0; ei < (int)effects[li].size(); ++ei)
    {
        const GlitchState& st = effects[li][ei];
        DerivedParams&     dp = derived[li][ei];
        juce::Random r((juce::int64)st.seed);
        float inten = st.intensity;
        int ssl = juce::jmax(1, stepSampleLen);

        dp.stutterLoopLen = juce::jmax(1, (int)(ssl * (0.5f - 0.45f * inten)));

        dp.numSlices = juce::jmin(16, 2 + (int)(14.0f * inten));
        for (int i = 0; i < dp.numSlices; ++i) dp.sliceOrder[i] = i;
        for (int i = dp.numSlices - 1; i > 0; --i) {
            int j = r.nextInt(i + 1);
            std::swap(dp.sliceOrder[i], dp.sliceOrder[j]);
        }

        dp.stretchFactor = 1.5f + 2.5f * inten;
        dp.bitDepth      = 8.0f - 7.0f * inten;
        dp.dsRate        = 1 + (int)(7.0f * inten);

        dp.gateChopLen   = juce::jmax(1, (int)(ssl / (2.0f + 14.0f * inten)));
        dp.gateDuty      = (int)(dp.gateChopLen * (0.65f - 0.55f * inten));
        dp.gateDuty      = juce::jmax(1, dp.gateDuty);

        dp.tapeSlowdown  = inten;                          // 0..1 → amount of slowdown

        dp.scratchPeriod = ssl * (0.05f + 0.4f * (1.0f - inten));

        dp.echoDelay    = juce::jmax(1, (int)(ssl * (0.125f + 0.375f * (1.0f - inten))));
        dp.echoFeedback = 0.3f + 0.55f * inten;

        dp.ringFreq = 80.0f + inten * 920.0f;

        dp.grainLen  = juce::jmax(64, (int)(currentSampleRate * (0.025f - 0.02f * inten)));
        dp.numGrains = 2 + (int)(6.0f * inten);
        dp.numGrains = juce::jmin(8, dp.numGrains);
        for (int g = 0; g < 8; ++g)
            dp.grainOffsets[g] = r.nextInt(juce::jmax(1, ssl - dp.grainLen));

        dp.warpDepth = (float)ssl * 0.03f * inten;
        dp.warpRate  = 1.0f + 4.0f * inten;
    }
}

// ── Step change ───────────────────────────────────────────────────────────────

void GlitchEngine::onNewStep(int step, double samplesPerBeat)
{
    currentStep   = step;
    glitchPlayPos = 0;
    stepSampleLen = juce::jmax(1, (int)samplesPerBeat);
    capStepStart  = wrapCap(capWritePos - stepSampleLen);
    for (int li = 0; li < NUM_LABELS; ++li)
        if (!effects[li].empty()) computeDerived(li);
}

// ── Main process ──────────────────────────────────────────────────────────────

void GlitchEngine::process(juce::AudioBuffer<float>& buffer, juce::AudioPlayHead* playHead)
{
    const int numSamples = buffer.getNumSamples();
    double bpm = 120.0, ppq = 0.0;
    bool   isPlaying = false;

    if (playHead != nullptr)
        if (auto pos = playHead->getPosition()) {
            bpm       = pos->getBpm().hasValue()         ? *pos->getBpm()         : 120.0;
            ppq       = pos->getPpqPosition().hasValue() ? *pos->getPpqPosition() : 0.0;
            isPlaying = pos->getIsPlaying();
        }

    lastBPM = bpm;
    if (lastPPQ < 0.0) lastPPQ = ppq;

    if (!isPlaying) {
        int numCh = juce::jmin(buffer.getNumChannels(), 2);
        for (int ch = 0; ch < numCh; ++ch)
            for (int i = 0; i < numSamples; ++i)
                cap[ch][(capWritePos + i) % capSize] = buffer.getSample(ch, i);
        capWritePos = (capWritePos + numSamples) % capSize;
        lastPPQ     = ppq + (double)numSamples * bpm / 60.0 / currentSampleRate;
        return;
    }

    double ppqPerSample   = bpm / 60.0 / currentSampleRate;
    double samplesPerBeat = currentSampleRate * 60.0 / bpm;

    int cursor = 0;
    while (cursor < numSamples)
    {
        double curPPQ      = ppq + (double)cursor * ppqPerSample;
        int    step        = ((int)std::floor(curPPQ) % NUM_STEPS + NUM_STEPS) % NUM_STEPS;
        double nextBeatPPQ = std::floor(curPPQ) + 1.0;
        int    segN = juce::jmin(numSamples - cursor,
                                 (int)std::ceil((nextBeatPPQ - curPPQ) / ppqPerSample));
        segN = juce::jmax(1, segN);

        if (step != currentStep) onNewStep(step, samplesPerBeat);
        processSegment(buffer, cursor, segN, grid[step]);
        cursor += segN;
    }

    lastPPQ = ppq + (double)numSamples * ppqPerSample;
}

// ── Segment ───────────────────────────────────────────────────────────────────

void GlitchEngine::processSegment(juce::AudioBuffer<float>& buf,
                                   int startSample, int numSamples, StepLabel label)
{
    int numCh = juce::jmin(buf.getNumChannels(), 2);

    for (int ch = 0; ch < numCh; ++ch)
        for (int i = 0; i < numSamples; ++i)
            cap[ch][(capWritePos + i) % capSize] = buf.getSample(ch, startSample + i);
    capWritePos = (capWritePos + numSamples) % capSize;

    if (label == StepLabel::X) { glitchPlayPos += numSamples; return; }

    int li = (int)label - 1;
    if (effects[li].empty()) { glitchPlayPos += numSamples; return; }

    applyEffect(buf, startSample, numSamples, li, 0, true);
    for (int ei = 1; ei < (int)effects[li].size(); ++ei)
        applyEffect(buf, startSample, numSamples, li, ei, false);

    glitchPlayPos += numSamples;
}

// ── Effect dispatcher ─────────────────────────────────────────────────────────

void GlitchEngine::applyEffect(juce::AudioBuffer<float>& buf,
                                int s, int n, int li, int ei, bool isFirst)
{
    int numCh = juce::jmin(buf.getNumChannels(), 2);
    int ssl   = juce::jmax(1, stepSampleLen);

    // Save current output into tempBuf for chaining
    if (!isFirst)
        for (int ch = 0; ch < numCh; ++ch)
            for (int i = 0; i < n; ++i)
                tempBuf[ch][i] = buf.getSample(ch, s + i);

    const DerivedParams& dp = derived[li][ei];
    const GlitchState&   st = effects[li][ei];

    // Read from cap buffer at beat-local position
    auto readMapped = [&](int ch, int beatPos) -> float {
        if (isFirst)
            return cap[ch % 2][wrapCap(capStepStart + beatPos)];
        int tp = juce::jmax(0, juce::jmin(n - 1, beatPos * n / ssl));
        return tempBuf[ch % 2][tp];
    };

    // Read at segment-local index i (linear, no remapping)
    auto readAt = [&](int ch, int i) -> float {
        if (isFirst)
            return cap[ch % 2][wrapCap(capStepStart + glitchPlayPos + i)];
        return tempBuf[ch % 2][juce::jmin(i, n - 1)];
    };

    switch (st.type)
    {
    // ─── Stutter ─────────────────────────────────────────────────────────────
    case GlitchType::Stutter:
    {
        int loop = dp.stutterLoopLen;
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i)
                d[i] = readMapped(ch, (glitchPlayPos + i) % loop);
        }
        break;
    }
    // ─── Slice ───────────────────────────────────────────────────────────────
    case GlitchType::Slice:
    {
        int sliceLen = juce::jmax(1, ssl / dp.numSlices);
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i) {
                int pos     = glitchPlayPos + i;
                int si      = juce::jmin(pos / sliceLen, dp.numSlices - 1);
                int inSlice = pos % sliceLen;
                int src     = dp.sliceOrder[si];
                d[i] = readMapped(ch, src * sliceLen + inSlice);
            }
        }
        break;
    }
    // ─── Bitcrush ────────────────────────────────────────────────────────────
    case GlitchType::Bitcrush:
    {
        float levels = std::pow(2.0f, dp.bitDepth);
        int   dsRate = dp.dsRate;
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            float inputPeak = 0.0f;
            for (int i = 0; i < n; ++i) { d[i] = readAt(ch, i); inputPeak = std::max(inputPeak, std::abs(d[i])); }
            float held = 0.0f, outPeak = 0.0f;
            for (int i = 0; i < n; ++i) {
                if ((glitchPlayPos + i) % dsRate == 0) held = std::round(d[i] * levels) / levels;
                d[i] = held;
                outPeak = std::max(outPeak, std::abs(d[i]));
            }
            if (outPeak > 0.0001f) {
                float scale = juce::jmin(1.0f, inputPeak / outPeak);
                for (int i = 0; i < n; ++i) d[i] *= scale;
            }
        }
        break;
    }
    // ─── Stretch ─────────────────────────────────────────────────────────────
    case GlitchType::Stretch:
    {
        float factor = dp.stretchFactor;
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i) {
                float rp   = (float)(glitchPlayPos + i) / factor;
                int   idx0 = (int)rp;
                float frac = rp - (float)idx0;
                float v0   = readMapped(ch, idx0);
                float v1   = readMapped(ch, juce::jmin(idx0 + 1, ssl - 1));
                d[i] = v0 + frac * (v1 - v0);
            }
        }
        break;
    }
    // ─── Reverse ─────────────────────────────────────────────────────────────
    case GlitchType::Reverse:
    {
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i) {
                int rev = juce::jmax(0, ssl - 1 - (glitchPlayPos + i));
                d[i] = readMapped(ch, rev);
            }
        }
        break;
    }
    // ─── Gate ────────────────────────────────────────────────────────────────
    case GlitchType::Gate:
    {
        int chopLen = dp.gateChopLen;
        int duty    = dp.gateDuty;
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i) {
                bool open = ((glitchPlayPos + i) % chopLen) < duty;
                d[i] = open ? readAt(ch, i) : 0.0f;
            }
        }
        break;
    }
    // ─── TapeStop ────────────────────────────────────────────────────────────
    case GlitchType::TapeStop:
    {
        float accumulated = 0.0f;
        // Pre-advance accumulator to glitchPlayPos
        for (int k = 0; k < glitchPlayPos; ++k) {
            float progress = (float)k / (float)ssl;
            float rate     = juce::jmax(0.01f, 1.0f - progress * dp.tapeSlowdown);
            accumulated   += rate;
        }
        for (int ch = 0; ch < numCh; ++ch) {
            auto*  d   = buf.getWritePointer(ch, s);
            float  acc = accumulated;
            for (int i = 0; i < n; ++i) {
                float progress = (float)(glitchPlayPos + i) / (float)ssl;
                float rate     = juce::jmax(0.01f, 1.0f - progress * dp.tapeSlowdown);
                acc           += rate;
                int   pos0 = (int)acc;
                float frac = acc - (float)pos0;
                float v0   = readMapped(ch, juce::jmin(pos0,   ssl - 1));
                float v1   = readMapped(ch, juce::jmin(pos0+1, ssl - 1));
                d[i] = v0 + frac * (v1 - v0);
            }
        }
        break;
    }
    // ─── Scratch ─────────────────────────────────────────────────────────────
    case GlitchType::Scratch:
    {
        float period = juce::jmax(1.0f, dp.scratchPeriod);
        float center = (float)ssl * 0.5f;
        float amp    = center * st.intensity;
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i) {
                float pos = (float)(glitchPlayPos + i);
                float osc = std::sin(pos / period * juce::MathConstants<float>::twoPi);
                int   bp  = juce::jmax(0, juce::jmin(ssl - 1, (int)(center + osc * amp)));
                d[i] = readMapped(ch, bp);
            }
        }
        break;
    }
    // ─── Echo ────────────────────────────────────────────────────────────────
    case GlitchType::Echo:
    {
        int   delay    = dp.echoDelay;
        float feedback = dp.echoFeedback;
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i) {
                float dry     = readAt(ch, i);
                int   delayBp = glitchPlayPos + i - delay;
                float wet     = (delayBp >= 0)
                                ? cap[ch % 2][wrapCap(capStepStart + delayBp)] * feedback
                                : 0.0f;
                d[i] = dry * 0.65f + wet * 0.55f;
            }
        }
        break;
    }
    // ─── RingMod ─────────────────────────────────────────────────────────────
    case GlitchType::RingMod:
    {
        float freq = dp.ringFreq;
        float sr   = (float)currentSampleRate;
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i) {
                float t       = (float)(glitchPlayPos + i) / sr;
                float carrier = std::sin(juce::MathConstants<float>::twoPi * freq * t);
                d[i] = readAt(ch, i) * carrier;
            }
        }
        break;
    }
    // ─── Granular ────────────────────────────────────────────────────────────
    case GlitchType::Granular:
    {
        int grainLen  = dp.grainLen;
        int numGrains = dp.numGrains;
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i) {
                int pos      = glitchPlayPos + i;
                int gi       = (pos / grainLen) % numGrains;
                int inGrain  = pos % grainLen;
                float window = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi
                                                        * inGrain / (float)juce::jmax(1, grainLen)));
                int bp = juce::jmin(dp.grainOffsets[gi % 8] + inGrain, ssl - 1);
                d[i] = readMapped(ch, bp) * window;
            }
        }
        break;
    }
    // ─── TapeWarp ────────────────────────────────────────────────────────────
    case GlitchType::TapeWarp:
    {
        float depth = dp.warpDepth;
        float rate  = dp.warpRate;
        for (int ch = 0; ch < numCh; ++ch) {
            auto* d = buf.getWritePointer(ch, s);
            for (int i = 0; i < n; ++i) {
                float t    = (float)(glitchPlayPos + i) / (float)ssl;
                float warp = depth * std::sin(t * rate * juce::MathConstants<float>::twoPi);
                float rp   = (float)(glitchPlayPos + i) + warp;
                int   idx0 = juce::jmax(0, juce::jmin(ssl - 2, (int)rp));
                float frac = rp - (float)idx0;
                float v0   = readMapped(ch, idx0);
                float v1   = readMapped(ch, idx0 + 1);
                d[i] = v0 + frac * (v1 - v0);
            }
        }
        break;
    }
    default: break;
    }
}
