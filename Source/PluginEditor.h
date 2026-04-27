#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class GSrandomEffectEditor : public juce::AudioProcessorEditor,
                              public juce::Timer
{
public:
    explicit GSrandomEffectEditor(GSrandomEffectProcessor&);
    ~GSrandomEffectEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;

private:
    GSrandomEffectProcessor& proc;

    // Palette selection state
    StepLabel selectedLabel { StepLabel::A };
    int       hoverStep     { -1 };

    // Layout rects (computed in resized)
    juce::Rectangle<int> cellBounds[NUM_STEPS];
    juce::Rectangle<int> paletteBounds[5];   // X A B C D
    juce::Rectangle<int> slotBounds[NUM_LABELS];
    juce::Rectangle<int> actionBounds[4];    // RAND SAVE PREV NEXT

    // Colours
    static juce::Colour bg()      { return juce::Colour(0xff111111); }
    static juce::Colour panel()   { return juce::Colour(0xff1a1a1a); }
    static juce::Colour border()  { return juce::Colour(0xff303030); }
    static juce::Colour dimText() { return juce::Colour(0xff555555); }
    static juce::Colour bright()  { return juce::Colour(0xffe8e8e8); }

    static juce::Colour labelColor(int li) {
        switch (li) {
            case 0: return juce::Colour(0xffff6600); // A = orange
            case 1: return juce::Colour(0xff00e5cc); // B = cyan
            case 2: return juce::Colour(0xffffcc00); // C = yellow
            case 3: return juce::Colour(0xffcc44ff); // D = violet
            default: return juce::Colour(0xff888888);
        }
    }
    static juce::Colour stepLabelColor(StepLabel l) {
        if (l == StepLabel::X) return juce::Colour(0xff333333);
        return labelColor((int)l - 1);
    }

    // Sub-painters
    void drawGrid(juce::Graphics&);
    void drawPalette(juce::Graphics&);
    void drawSlots(juce::Graphics&);
    void drawStatusBar(juce::Graphics&);
    void drawActionButtons(juce::Graphics&);

    // Icon painters
    void drawDiceIcon  (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c);
    void drawSaveIcon  (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c);
    void drawPrevIcon  (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c);
    void drawNextIcon  (juce::Graphics&, juce::Rectangle<float> r, juce::Colour c);

    static constexpr int W = 600;
    static constexpr int H = 490;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GSrandomEffectEditor)
};
