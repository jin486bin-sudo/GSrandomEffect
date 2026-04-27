#include "PluginEditor.h"

static const char* LABEL_NAMES[] = { "X", "A", "B", "C", "D" };

GSrandomEffectEditor::GSrandomEffectEditor(GSrandomEffectProcessor& p)
    : AudioProcessorEditor(&p), proc(p)
{
    setSize(W, H);
    setMouseClickGrabsKeyboardFocus(false);
    startTimerHz(24);
}

GSrandomEffectEditor::~GSrandomEffectEditor() { stopTimer(); }

void GSrandomEffectEditor::timerCallback() { repaint(); }

// ── Layout ────────────────────────────────────────────────────────────────────

void GSrandomEffectEditor::resized()
{
    // Grid: y=32, h=72
    int cellW = (W - 32) / NUM_STEPS;
    for (int i = 0; i < NUM_STEPS; ++i)
        cellBounds[i] = { 16 + i * cellW, 32, cellW - 2, 72 };

    // Palette: 5 buttons (X A B C D), y=118, h=40
    int palW  = 64, palH = 40, palGap = 10;
    int palX0 = (W - (5 * palW + 4 * palGap)) / 2;
    for (int i = 0; i < 5; ++i)
        paletteBounds[i] = { palX0 + i * (palW + palGap), 118, palW, palH };

    // Effect slots: y=174, h=208
    int slotW = (W - 32) / NUM_LABELS;
    for (int li = 0; li < NUM_LABELS; ++li)
        slotBounds[li] = { 16 + li * slotW, 174, slotW - 4, 208 };

    // Action buttons: y=400, h=44
    int btnW = 80, btnH = 44, btnGap = 14;
    int btnX0 = (W - (4 * btnW + 3 * btnGap)) / 2;
    for (int i = 0; i < 4; ++i)
        actionBounds[i] = { btnX0 + i * (btnW + btnGap), 400, btnW, btnH };
}

// ── Mouse ─────────────────────────────────────────────────────────────────────

void GSrandomEffectEditor::mouseDown(const juce::MouseEvent& e)
{
    // Palette: select label
    for (int i = 0; i < 5; ++i) {
        if (paletteBounds[i].contains(e.getPosition())) {
            selectedLabel = (StepLabel)i;
            return;
        }
    }
    // Grid: apply selected label
    for (int i = 0; i < NUM_STEPS; ++i) {
        if (cellBounds[i].contains(e.getPosition())) {
            proc.getEngine().setStep(i, selectedLabel);
            return;
        }
    }
    // Action buttons
    if (actionBounds[0].contains(e.getPosition())) { proc.requestRandomize(); return; }
    if (actionBounds[1].contains(e.getPosition())) { proc.requestSave();      return; }
    if (actionBounds[2].contains(e.getPosition())) { proc.requestBack();      return; }
    if (actionBounds[3].contains(e.getPosition())) { proc.requestForward();   return; }
}

void GSrandomEffectEditor::mouseMove(const juce::MouseEvent& e)
{
    hoverStep = -1;
    for (int i = 0; i < NUM_STEPS; ++i)
        if (cellBounds[i].contains(e.getPosition())) { hoverStep = i; return; }
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void GSrandomEffectEditor::paint(juce::Graphics& g)
{
    g.fillAll(bg());

    // Title bar
    g.setColour(juce::Colour(0xff181818));
    g.fillRect(0, 0, W, 28);
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 12.0f, juce::Font::bold));
    g.setColour(bright().withAlpha(0.5f));
    g.drawText("GSrandom effect  v2.0", 0, 0, W, 28, juce::Justification::centred);

    g.setColour(border());
    g.drawHorizontalLine(28, 0.0f, (float)W);

    drawGrid(g);
    drawPalette(g);

    // Divider before slots
    g.setColour(border());
    g.drawHorizontalLine(168, 16.0f, (float)(W - 16));

    // "EFFECTS" label
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::plain));
    g.setColour(dimText());
    g.drawText("EFFECT SLOTS", 16, 158, 120, 10, juce::Justification::centredLeft);

    drawSlots(g);
    drawStatusBar(g);
    drawActionButtons(g);
}

// ── Grid ──────────────────────────────────────────────────────────────────────

void GSrandomEffectEditor::drawGrid(juce::Graphics& g)
{
    auto& engine     = proc.getEngine();
    int   activeStep = engine.getCurrentStep();

    // "STEP GRID" label
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::plain));
    g.setColour(dimText());
    g.drawText("STEP GRID  (BAR 1 ─── BAR 2 ─── BAR 3 ─── BAR 4)", 16, 22, W - 32, 10, juce::Justification::centredLeft);

    juce::Font stepFont(juce::Font::getDefaultSansSerifFontName(), 20.0f, juce::Font::bold);
    g.setFont(stepFont);

    for (int i = 0; i < NUM_STEPS; ++i)
    {
        auto r    = cellBounds[i].toFloat();
        auto lbl  = engine.getStep(i);
        bool act  = (i == activeStep);
        bool hov  = (i == hoverStep);

        juce::Colour col = stepLabelColor(lbl);

        // Background
        if (lbl == StepLabel::X)
            g.setColour(juce::Colour(0xff141414));
        else
            g.setColour(col.withAlpha(0.15f));
        g.fillRect(r);

        if (act) { g.setColour(bright().withAlpha(0.08f)); g.fillRect(r); }
        if (hov && !act) { g.setColour(bright().withAlpha(0.05f)); g.fillRect(r); }

        // Border
        float bw = act ? 2.0f : 0.7f;
        g.setColour(act ? bright().withAlpha(0.7f) : (lbl == StepLabel::X ? border() : col.withAlpha(0.5f)));
        g.drawRect(r, bw);

        // Letter
        if (lbl == StepLabel::X)
            g.setColour(dimText());
        else
            g.setColour(col);
        g.drawText(LABEL_NAMES[(int)lbl], cellBounds[i], juce::Justification::centred);
    }
}

// ── Palette ───────────────────────────────────────────────────────────────────

void GSrandomEffectEditor::drawPalette(juce::Graphics& g)
{
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::plain));
    g.setColour(dimText());
    g.drawText("SELECT LABEL  →  CLICK STEP TO APPLY",
               paletteBounds[0].getX(), 108, W - 32, 10, juce::Justification::centredLeft);

    juce::Font palFont(juce::Font::getDefaultSansSerifFontName(), 18.0f, juce::Font::bold);
    g.setFont(palFont);

    for (int i = 0; i < 5; ++i)
    {
        auto r     = paletteBounds[i].toFloat();
        bool sel   = ((int)selectedLabel == i);
        juce::Colour col = (i == 0) ? bright().withAlpha(0.3f) : labelColor(i - 1);

        // Background
        g.setColour(sel ? col.withAlpha(0.3f) : col.withAlpha(0.07f));
        g.fillRoundedRectangle(r, 5.0f);

        // Border
        g.setColour(sel ? col : col.withAlpha(0.3f));
        g.drawRoundedRectangle(r, 5.0f, sel ? 2.0f : 1.0f);

        // Label
        g.setColour(sel ? (i == 0 ? bright() : col.brighter(0.4f)) : col.withAlpha(0.7f));
        g.drawText(LABEL_NAMES[i], paletteBounds[i], juce::Justification::centred);
    }
}

// ── Effect slots ──────────────────────────────────────────────────────────────

void GSrandomEffectEditor::drawSlots(juce::Graphics& g)
{
    auto& engine = proc.getEngine();

    juce::Font bigFont  (juce::Font::getDefaultSansSerifFontName(), 26.0f, juce::Font::bold);
    juce::Font typeFont (juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::bold);
    juce::Font seedFont (juce::Font::getDefaultMonospacedFontName(), 9.0f, juce::Font::plain);

    for (int li = 0; li < NUM_LABELS; ++li)
    {
        auto r   = slotBounds[li].toFloat();
        juce::Colour col = labelColor(li);
        const auto& chain = engine.getEffects(li);

        // Slot background
        g.setColour(col.withAlpha(0.04f));
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(col.withAlpha(0.2f));
        g.drawRoundedRectangle(r, 6.0f, 1.0f);

        int x = slotBounds[li].getX();
        int y = slotBounds[li].getY();
        int w = slotBounds[li].getWidth();

        // Slot letter
        g.setFont(bigFont);
        g.setColour(col);
        g.drawText(LABEL_NAMES[li + 1], x + 8, y + 6, 30, 30, juce::Justification::centredLeft);

        if (chain.empty())
        {
            g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 11.0f, juce::Font::plain));
            g.setColour(dimText());
            g.drawText("no effects", x + 8, y + 80, w - 16, 20, juce::Justification::centred);
        }
        else
        {
            for (int ei = 0; ei < (int)chain.size(); ++ei)
            {
                const GlitchState& st = chain[ei];
                int ey = y + 44 + ei * 52;
                int barW = w - 20;

                // Effect type badge
                juce::String typeName(glitchTypeShort[(int)st.type]);
                g.setFont(typeFont);
                float tW = typeFont.getStringWidthFloat(typeName) + 10.0f;

                juce::Colour badgeCol = col.withAlpha(0.8f);
                g.setColour(badgeCol);
                g.fillRoundedRectangle((float)(x + 8), (float)ey, tW, 16.0f, 3.0f);
                g.setColour(juce::Colour(0xff000000));
                g.drawText(typeName, x + 8, ey, (int)tW, 16, juce::Justification::centred);

                // Intensity bar
                g.setColour(col.withAlpha(0.15f));
                g.fillRect(x + 8, ey + 20, barW, 6);
                g.setColour(col.withAlpha(0.8f));
                g.fillRect(x + 8, ey + 20, (int)(barW * st.intensity), 6);

                // Seed hint
                g.setFont(seedFont);
                g.setColour(dimText());
                g.drawText(juce::String::toHexString((int)(st.seed & 0xffff)).toUpperCase(),
                           x + 8, ey + 30, barW, 10, juce::Justification::centredLeft);
            }
        }
    }
}

// ── Status bar ────────────────────────────────────────────────────────────────

void GSrandomEffectEditor::drawStatusBar(juce::Graphics& g)
{
    auto& engine = proc.getEngine();
    int y = 388;

    g.setColour(juce::Colour(0xff181818));
    g.fillRect(0, y, W, 12);
    g.setColour(border());
    g.drawHorizontalLine(y, 0.0f, (float)W);

    // Mini step strip
    int cellW = (W - 32) / NUM_STEPS;
    int step  = engine.getCurrentStep();
    for (int i = 0; i < NUM_STEPS; ++i) {
        StepLabel lbl = engine.getStep(i);
        juce::Colour c = (lbl == StepLabel::X)
                         ? dimText().withAlpha(0.15f)
                         : stepLabelColor(lbl).withAlpha(i == step ? 1.0f : 0.3f);
        g.setColour(c);
        g.fillRect(16 + i * cellW, y + 1, cellW - 2, 6);
    }

    // Info text
    g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 10.0f, juce::Font::plain));
    g.setColour(bright().withAlpha(0.45f));
    juce::String info;
    info << "BPM " << juce::String(engine.getBPM(), 1)
         << "   STEP " << juce::String::formatted("%02d/16", step + 1)
         << "   HIST " << (engine.getHistoryIndex() + 1) << "/" << engine.getHistorySize();
    g.drawText(info, 16, y + 1, W - 32, 11, juce::Justification::centredRight);
}

// ── Action buttons ────────────────────────────────────────────────────────────

void GSrandomEffectEditor::drawActionButtons(juce::Graphics& g)
{
    const char* labels[] = { "RAND", "SAVE", "PREV", "NEXT" };
    juce::Colour btnBase = juce::Colour(0xff222222);
    juce::Colour iconCol = bright().withAlpha(0.85f);

    for (int i = 0; i < 4; ++i)
    {
        auto r    = actionBounds[i].toFloat();
        auto icon = r.reduced(8.0f, 8.0f);

        // Button background
        g.setColour(btnBase);
        g.fillRoundedRectangle(r, 6.0f);
        g.setColour(border());
        g.drawRoundedRectangle(r, 6.0f, 1.0f);

        // Icon
        switch (i) {
            case 0: drawDiceIcon(g, icon, iconCol); break;
            case 1: drawSaveIcon(g, icon, iconCol); break;
            case 2: drawPrevIcon(g, icon, iconCol); break;
            case 3: drawNextIcon(g, icon, iconCol); break;
        }

        // Label below
        g.setFont(juce::Font(juce::Font::getDefaultSansSerifFontName(), 9.0f, juce::Font::plain));
        g.setColour(dimText());
        g.drawText(labels[i], actionBounds[i].getX(), actionBounds[i].getBottom() + 2,
                   actionBounds[i].getWidth(), 10, juce::Justification::centred);
    }
}

// ── Icon painters ─────────────────────────────────────────────────────────────

void GSrandomEffectEditor::drawDiceIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    // Draw a die face (⚅ = 6 dots in 2 columns)
    g.setColour(c.withAlpha(0.3f));
    float s = juce::jmin(r.getWidth(), r.getHeight());
    auto  d = r.withSizeKeepingCentre(s, s);
    g.drawRoundedRectangle(d, 3.0f, 1.5f);

    float dotR = s * 0.09f;
    float cx   = d.getCentreX(), cy = d.getCentreY();
    float dx   = s * 0.22f, dy = s * 0.22f;
    g.setColour(c);
    // 6-dot pattern
    for (float row : { -dy, 0.0f, dy })
        for (float col : { -dx, dx })
            g.fillEllipse(cx + col - dotR, cy + row - dotR, dotR * 2, dotR * 2);
}

void GSrandomEffectEditor::drawSaveIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    // Down-arrow with base line (save = push down to stack)
    float cx = r.getCentreX(), cy = r.getCentreY();
    float h  = r.getHeight() * 0.8f, w = r.getWidth() * 0.55f;
    float stemH = h * 0.45f, arrowH = h * 0.45f;
    float stemW = w * 0.3f;

    juce::Path p;
    p.addRectangle(cx - stemW / 2, cy - h / 2, stemW, stemH);
    p.addTriangle(cx - w / 2, cy - h / 2 + stemH,
                  cx + w / 2, cy - h / 2 + stemH,
                  cx,         cy - h / 2 + stemH + arrowH);
    // Base bar
    float baseY = cy + h / 2 - 2.0f;
    p.addRectangle(cx - w / 2, baseY, w, 2.5f);

    g.setColour(c);
    g.fillPath(p);
}

void GSrandomEffectEditor::drawPrevIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    float cx = r.getCentreX(), cy = r.getCentreY();
    float h  = r.getHeight() * 0.6f, w = r.getWidth() * 0.65f;
    juce::Path p;
    // Two left-pointing triangles (skip-back feel)
    p.addTriangle(cx,          cy - h / 2,
                  cx,          cy + h / 2,
                  cx - w / 2,  cy);
    p.addTriangle(cx + w / 4,  cy - h / 2,
                  cx + w / 4,  cy + h / 2,
                  cx - w / 4,  cy);
    g.setColour(c);
    g.fillPath(p);
}

void GSrandomEffectEditor::drawNextIcon(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
{
    float cx = r.getCentreX(), cy = r.getCentreY();
    float h  = r.getHeight() * 0.6f, w = r.getWidth() * 0.65f;
    juce::Path p;
    // Two right-pointing triangles
    p.addTriangle(cx,          cy - h / 2,
                  cx,          cy + h / 2,
                  cx + w / 2,  cy);
    p.addTriangle(cx - w / 4,  cy - h / 2,
                  cx - w / 4,  cy + h / 2,
                  cx + w / 4,  cy);
    g.setColour(c);
    g.fillPath(p);
}
