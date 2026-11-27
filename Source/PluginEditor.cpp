#include "PluginEditor.h"

namespace ui
{
juce::Font font (float height, bool bold)
{
#if JUCE_WINDOWS
    return juce::Font (juce::FontOptions ("Segoe UI", height, bold ? juce::Font::bold : juce::Font::plain));
#else
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
#endif
}

static juce::Font tracked (float height, float kerning) { return font (height, true).withExtraKerningFactor (kerning); }

// ---------------------------------------------------------------------------
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, col::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::textColourId, col::text);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.18f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::text);
    setColour (juce::Label::textColourId, col::text);
    setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.3f));
    setColour (juce::CaretComponent::caretColourId, col::accent);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& s)
{
    using namespace juce;
    const auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
    const float r = std::min (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float arcR = r - 3.0f, lineW = 3.5f;
    const float angle = start + pos * (end - start);
    const bool hot = s.isMouseOverOrDragging() && s.isEnabled();

    Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, start, end, true);
    g.setColour (col::border);
    g.strokePath (track, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));

    const float from = (bool) s.getProperties()["bipolar"] ? (start + end) * 0.5f : start;
    if (std::abs (angle - from) > 0.001f)
    {
        Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, std::min (from, angle), std::max (from, angle), true);
        g.setColour (col::accent.withAlpha (hot ? 0.28f : 0.16f));
        g.strokePath (value, PathStrokeType (lineW * 3.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setGradientFill (ColourGradient (col::accent2, c.x - r, c.y + r, col::accent, c.x + r, c.y - r, false));
        g.strokePath (value, PathStrokeType (lineW, PathStrokeType::curved, PathStrokeType::rounded));
    }

    const float kr = arcR - 8.0f;
    g.setColour (Colours::black.withAlpha (0.4f));
    g.fillEllipse (c.x - kr, c.y - kr + 3.0f, kr * 2.0f, kr * 2.0f);
    g.setGradientFill (ColourGradient (hot ? Colour (0xff313b51) : Colour (0xff293245), c.x, c.y - kr,
                                       Colour (0xff131822), c.x, c.y + kr, false));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (Colour (0xff364056));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    Path pointer;
    pointer.addRoundedRectangle (-1.5f, -kr + 5.0f, 3.0f, kr * 0.42f, 1.5f);
    g.setColour (col::text);
    g.fillPath (pointer, AffineTransform::rotation (angle).translated (c.x, c.y));
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (font (13.0f, true));
    l->setColour (juce::Label::textColourId, col::text);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundWhenEditingColourId, col::bg2);
    l->setColour (juce::Label::textWhenEditingColourId, col::text);
    l->setColour (juce::Label::outlineWhenEditingColourId, col::accent);
    return l;
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();
    const float rad = r.getHeight() * 0.5f;

    g.setColour (on ? col::accent.withAlpha (0.13f) : col::bg2);
    g.fillRoundedRectangle (r, rad);
    g.setColour (on ? col::accent.withAlpha (0.75f) : over ? col::textFaint : col::border);
    g.drawRoundedRectangle (r, rad, 1.0f);

    const float d = 7.0f;
    const juce::Rectangle<float> led (r.getX() + 14.0f, r.getCentreY() - d * 0.5f, d, d);
    if (on)
    {
        g.setColour (col::accent.withAlpha (0.3f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? col::accent : col::textFaint);
    g.fillEllipse (led);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    g.setFont (tracked (11.5f, 0.12f));
    g.setColour (b.getToggleState() ? col::accent : over ? col::text : col::textDim);
    g.drawText (b.getButtonText(), b.getLocalBounds().withTrimmedLeft (18), juce::Justification::centred);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (col::bg2);
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.isMouseOver (true) ? col::textFaint : col::border);
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    juce::Path chevron;
    const float cx = (float) w - 18.0f, cy = (float) h * 0.5f;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (col::textDim);
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (14, 1, box.getWidth() - 40, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centred);
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (col::panel);
    g.setColour (col::border);
    g.drawRect (0, 0, w, h);
}

// ---------------------------------------------------------------------------
bool ScopeView::push (const GSideChainProcessor::Point& p)
{
    hist[(size_t) head] = p;
    head = (head + 1) % numPoints;
    // silent points all draw at the floor; once the whole window is silent the picture stops changing
    const bool silent = p.scDb <= -60.0f && p.inDb <= -60.0f && p.grDb < 0.01f;
    quietRun = silent ? std::min (quietRun + 1, numPoints + 1) : 0;
    return quietRun <= numPoints;
}

void ScopeView::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (Colour (0xff0f141e), 0, b.getY(), Colour (0xff0a0d14), 0, b.getBottom(), false));
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);

    const auto plot = b.reduced (14.0f, 14.0f).withTrimmedLeft (24.0f);
    auto xFor = [&] (int i) { return plot.getX() + plot.getWidth() * (float) i / (float) (numPoints - 1); };
    auto yFor = [&] (float db) { return plot.getY() + plot.getHeight() * (1.0f - (jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f); };
    constexpr float grRange = 30.0f;
    auto grY = [&] (float gr) { return plot.getY() + plot.getHeight() * jmin (gr, grRange) / grRange; };
    auto at = [&] (int i) -> const GSideChainProcessor::Point& { return hist[(size_t) ((head + i) % numPoints)]; };

    // grid
    g.setFont (font (10.0f, true));
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f })
    {
        const float y = yFor (db);
        g.setColour (col::border.withAlpha (db == 0.0f ? 0.0f : 0.45f));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (col::textFaint);
        g.drawText (String ((int) db), Rectangle<float> (b.getX() + 8.0f, y - 6.0f, 26.0f, 12.0f), Justification::centredLeft);
    }
    for (int i = 1; i < 8; ++i)
    {
        g.setColour (col::border.withAlpha (0.25f));
        g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * (float) i / 8.0f), plot.getY(), plot.getBottom());
    }

    // input (bass) level
    Path in;
    in.startNewSubPath (plot.getX(), plot.getBottom());
    for (int i = 0; i < numPoints; ++i) in.lineTo (xFor (i), yFor (at (i).inDb));
    in.lineTo (plot.getRight(), plot.getBottom());
    in.closeSubPath();
    g.setColour (col::input.withAlpha (0.16f));
    g.fillPath (in);

    // sidechain (kick, as the detector sees it)
    Path scFill, scLine;
    scFill.startNewSubPath (plot.getX(), plot.getBottom());
    for (int i = 0; i < numPoints; ++i)
    {
        const float x = xFor (i), y = yFor (at (i).scDb);
        scFill.lineTo (x, y);
        if (i == 0) scLine.startNewSubPath (x, y); else scLine.lineTo (x, y);
    }
    scFill.lineTo (plot.getRight(), plot.getBottom());
    scFill.closeSubPath();
    g.setGradientFill (ColourGradient (col::kick.withAlpha (0.42f), 0, plot.getY(), col::kick.withAlpha (0.03f), 0, plot.getBottom(), false));
    g.fillPath (scFill);
    g.setColour (col::kick.withAlpha (0.85f));
    g.strokePath (scLine, PathStrokeType (1.3f));

    // threshold
    const float ty = yFor (thresholdDb);
    const float dashes[] = { 6.0f, 4.0f };
    g.setColour (col::kick.brighter (0.4f).withAlpha (0.8f));
    g.drawDashedLine (Line<float> (plot.getX(), ty, plot.getRight(), ty), dashes, 2, 1.2f);
    g.setFont (font (10.5f, true));
    g.drawText ("THRESHOLD " + String (thresholdDb, 1) + " dB", Rectangle<float> (plot.getRight() - 180.0f, ty - 15.0f, 176.0f, 13.0f),
                Justification::centredRight);

    // gain reduction, hanging from the top
    Path grFill, grLine;
    grFill.startNewSubPath (plot.getX(), plot.getY());
    for (int i = 0; i < numPoints; ++i)
    {
        const float x = xFor (i), y = grY (at (i).grDb);
        grFill.lineTo (x, y);
        if (i == 0) grLine.startNewSubPath (x, y); else grLine.lineTo (x, y);
    }
    grFill.lineTo (plot.getRight(), plot.getY());
    grFill.closeSubPath();
    g.setGradientFill (ColourGradient (col::accent.withAlpha (0.30f), 0, plot.getY(), col::accent2.withAlpha (0.04f), 0, plot.getBottom(), false));
    g.fillPath (grFill);
    g.setColour (col::accent.withAlpha (0.14f));
    g.strokePath (grLine, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
    g.setGradientFill (ColourGradient (col::accent2, plot.getX(), 0, col::accent, plot.getRight(), 0, false));
    g.strokePath (grLine, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));

    // legend
    struct Item { const char* label; Colour c; };
    const Item items[] = { { "SIDECHAIN", col::kick }, { "INPUT", col::input }, { "GAIN REDUCTION", col::accent } };
    g.setFont (tracked (10.0f, 0.1f));
    float lw = 16.0f;
    for (auto& it : items) lw += GlyphArrangement::getStringWidth (g.getCurrentFont(), it.label) + 30.0f;
    auto legend = Rectangle<float> (plot.getRight() - lw, plot.getBottom() - 26.0f, lw, 22.0f);
    g.setColour (Colour (0xcc0b0e14));
    g.fillRoundedRectangle (legend, 11.0f);
    float lx = legend.getX() + 14.0f;
    for (auto& it : items)
    {
        g.setColour (it.c);
        g.fillEllipse (lx, legend.getCentreY() - 3.5f, 7.0f, 7.0f);
        const float tw = GlyphArrangement::getStringWidth (g.getCurrentFont(), it.label);
        g.setColour (col::textDim);
        g.drawText (it.label, Rectangle<float> (lx + 12.0f, legend.getY(), tw + 4.0f, legend.getHeight()), Justification::centredLeft);
        lx += tw + 30.0f;
    }

    if (! scActive)
    {
        const auto pill = Rectangle<float> (plot.getX() + 6.0f, plot.getBottom() - 26.0f, 186.0f, 22.0f);
        g.setColour (col::warn.withAlpha (0.14f));
        g.fillRoundedRectangle (pill, 11.0f);
        g.setColour (col::warn.withAlpha (0.6f));
        g.drawRoundedRectangle (pill, 11.0f, 1.0f);
        g.setColour (col::warn);
        g.setFont (tracked (10.0f, 0.1f));
        g.drawText ("NO SIDECHAIN SIGNAL", pill, Justification::centred);
    }
}

// ---------------------------------------------------------------------------
void GainMeter::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat();
    g.setColour (Colour (0xff0d1119));
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (col::border);
    g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, 1.0f);

    auto bar = b.reduced (0.0f, 14.0f).withTrimmedBottom (34.0f).withSizeKeepingCentre (14.0f, b.getHeight() - 62.0f);
    bar.setY (b.getY() + 14.0f);
    g.setColour (col::bg2);
    g.fillRoundedRectangle (bar, 4.0f);

    constexpr float range = 30.0f;
    g.setColour (col::border);
    for (float db : { 6.0f, 12.0f, 18.0f, 24.0f })
        g.drawHorizontalLine ((int) (bar.getY() + bar.getHeight() * db / range), bar.getRight() + 3.0f, bar.getRight() + 7.0f);

    if (value > 0.05f)
    {
        g.setGradientFill (ColourGradient (col::accent, 0, bar.getY(), col::accent2, 0, bar.getBottom(), false));
        g.fillRoundedRectangle (bar.withHeight (bar.getHeight() * jmin (value, range) / range), 4.0f);
    }
    g.setColour (col::text.withAlpha (0.85f));
    g.fillRect (bar.getX(), bar.getY() + bar.getHeight() * jmin (peak, range) / range - 1.0f, bar.getWidth(), 2.0f);

    g.setColour (col::textFaint);
    g.setFont (tracked (9.5f, 0.12f));
    g.drawText ("GR", b.withTrimmedTop (b.getHeight() - 40.0f).withHeight (14.0f), Justification::centred);
    g.setColour (col::text);
    g.setFont (font (12.5f, true));
    g.drawText (value > 0.05f ? String (-value, 1) : String ("0.0"), b.withTrimmedTop (b.getHeight() - 26.0f).withHeight (16.0f),
                Justification::centred);
}

// ---------------------------------------------------------------------------
void AutoButton::setLearning (bool isLearning, float p)
{
    learning = isLearning;
    progress = p;
}

void AutoButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    using namespace juce;
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    constexpr float rad = 10.0f;

    if (! learning)
    {
        g.setColour (col::accent.withAlpha (over ? 0.35f : 0.2f));
        g.fillRoundedRectangle (b.expanded (1.0f), rad + 1.0f);
        g.setGradientFill (ColourGradient (col::accent, b.getX(), b.getY(), col::accent2, b.getRight(), b.getBottom(), false));
        g.fillRoundedRectangle (b, rad);
        if (over) { g.setColour (Colours::white.withAlpha (0.08f)); g.fillRoundedRectangle (b, rad); }
        if (down) { g.setColour (Colours::black.withAlpha (0.12f)); g.fillRoundedRectangle (b, rad); }

        const String label ("AUTO DETECT");
        const auto f = tracked (13.0f, 0.1f);
        const float tw = GlyphArrangement::getStringWidth (f, label);
        const float total = tw + 22.0f, x0 = b.getCentreX() - total * 0.5f, cy = b.getCentreY();

        Path star;
        const float cx = x0 + 6.0f, s = 6.5f;
        star.startNewSubPath (cx, cy - s);
        star.quadraticTo (cx, cy, cx + s, cy);
        star.quadraticTo (cx, cy, cx, cy + s);
        star.quadraticTo (cx, cy, cx - s, cy);
        star.quadraticTo (cx, cy, cx, cy - s);
        star.closeSubPath();
        const Colour ink (0xff07121c);
        g.setColour (ink);
        g.fillPath (star);
        g.setFont (f);
        g.drawText (label, Rectangle<float> (x0 + 20.0f, b.getY(), tw + 4.0f, b.getHeight()), Justification::centredLeft);
        return;
    }

    g.setColour (col::bg2);
    g.fillRoundedRectangle (b, rad);
    {
        Graphics::ScopedSaveState save (g);
        Path clip;
        clip.addRoundedRectangle (b, rad);
        g.reduceClipRegion (clip);
        g.setGradientFill (ColourGradient (col::accent.withAlpha (0.4f), b.getX(), 0, col::accent2.withAlpha (0.4f), b.getRight(), 0, false));
        g.fillRect (b.withWidth (b.getWidth() * jlimit (0.0f, 1.0f, progress)));
    }
    g.setColour (col::accent.withAlpha (over ? 1.0f : 0.75f));
    g.drawRoundedRectangle (b, rad, 1.2f);

    const float pulse = 0.55f + 0.45f * std::sin ((float) Time::getMillisecondCounter() * 0.006f);
    g.setColour (col::warn.withAlpha (pulse));
    g.fillEllipse (b.getX() + 14.0f, b.getCentreY() - 4.0f, 8.0f, 8.0f);

    g.setColour (col::text);
    g.setFont (tracked (12.0f, 0.1f));
    const String caption = over ? String ("CANCEL")
                      : progress <= 0.0f ? String ("WAITING FOR AUDIO")
                                         : "LISTENING  " + String (roundToInt (progress * 100.0f)) + "%";
    g.drawText (caption, b.withTrimmedLeft (20.0f), Justification::centred);
}

void ChevronButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (down ? col::panelHi : col::bg2);
    g.fillRoundedRectangle (b, 8.0f);
    g.setColour (over ? col::textFaint : col::border);
    g.drawRoundedRectangle (b, 8.0f, 1.0f);

    juce::Path p;
    const float cx = b.getCentreX() + (right ? 1.0f : -1.0f), cy = b.getCentreY(), d = right ? 1.0f : -1.0f;
    p.startNewSubPath (cx - 2.5f * d, cy - 5.0f);
    p.lineTo (cx + 2.5f * d, cy);
    p.lineTo (cx - 2.5f * d, cy + 5.0f);
    g.setColour (over ? col::text : col::textDim);
    g.strokePath (p, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace ui

// ===========================================================================
using namespace ui;

namespace
{
const juce::String hintText ("Route your kick to the sidechain input, play kick + bass together and hit AUTO DETECT.");
}

GSideChainEditor::GSideChainEditor (GSideChainProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (scope);
    addAndMakeVisible (meter);

    addAndMakeVisible (autoButton);
    autoButton.onClick = [this]
    {
        if (proc.isLearning()) proc.cancelLearn(); else proc.startLearn();
        refresh();
    };

    presetBox.addItemList (GSideChainProcessor::getPresetNames(), 1);
    presetBox.onChange = [this]
    {
        if (const int id = presetBox.getSelectedId(); id > 0 && id - 1 != proc.getCurrentProgram())
        {
            proc.setCurrentProgram (id - 1);
            proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
        }
    };
    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);
    prevButton.onClick = [this] { stepPreset (-1); };
    nextButton.onClick = [this] { stepPreset (1); };

    infoLabel.setFont (ui::font (13.5f));
    infoLabel.setColour (juce::Label::textColourId, col::textDim);
    infoLabel.setText (hintText, juce::dontSendNotification);
    addAndMakeVisible (infoLabel);

    setupKnob (threshold, "threshold", "Threshold");
    setupKnob (ratio, "ratio", "Ratio");
    setupKnob (focus, "focus", "SC Focus");
    setupKnob (attack, "attack", "Attack");
    setupKnob (hold, "hold", "Hold");
    setupKnob (release, "release", "Release");
    setupKnob (depth, "depth", "Depth");
    setupKnob (crossover, "crossover", "Crossover");
    setupKnob (mix, "mix", "Mix");
    setupKnob (output, "output", "Output", true);

    setupToggle (listenButton, listenAttachment, "listen", "SC LISTEN");
    setupToggle (lowOnlyButton, lowOnlyAttachment, "lowOnly", "LOWS ONLY");

    cards[0].title = "DETECTOR";
    cards[1].title = "ENVELOPE";
    cards[2].title = "DUCKING";
    cards[3].title = "OUTPUT";

    setSize (960, 580);
    GSideChainProcessor::Point stale;
    while (proc.popPoint (stale)) {} // drop points queued while the window was closed
    refresh();
    startTimerHz (60);
}

GSideChainEditor::~GSideChainEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void GSideChainEditor::setupKnob (Knob& k, const juce::String& id, const juce::String& name, bool bipolar)
{
    k.name = name.toUpperCase();
    addAndMakeVisible (k.slider); // parent first so the value box is built by our LookAndFeel
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 20);
    k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k.slider.getProperties().set ("bipolar", bipolar);
    k.slider.setMouseDragSensitivity (220);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    if (auto* param = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void GSideChainEditor::setupToggle (juce::TextButton& b, std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>& att,
                                    const juce::String& id, const juce::String& text)
{
    b.setButtonText (text);
    b.setClickingTogglesState (true);
    addAndMakeVisible (b);
    att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, b);
}

void GSideChainEditor::stepPreset (int delta)
{
    const int n = proc.getNumPrograms() - 1; // factory presets only, skip the "Auto Detected" slot
    const int cur = proc.getCurrentProgram();
    proc.setCurrentProgram (cur >= n ? (delta > 0 ? 0 : n - 1) : (cur + delta + n) % n);
    proc.updateHostDisplay (juce::AudioProcessor::ChangeDetails().withProgramChanged (true));
    refresh();
}

void GSideChainEditor::refresh()
{
    // redraw only what changed, so an idle window costs nothing
    bool scopeDirty = false;
    GSideChainProcessor::Point pt;
    while (proc.popPoint (pt)) scopeDirty |= scope.push (pt);
    scopeDirty |= scope.setThreshold (proc.apvts.getRawParameterValue ("threshold")->load());
    scopeDirty |= scope.setSidechainActive (proc.isSidechainActive());
    if (scopeDirty) scope.repaint();
    if (meter.setGr (proc.getCurrentGr())) meter.repaint();

    const bool learning = proc.isLearning();
    autoButton.setLearning (learning, proc.getLearnProgress());
    if (learning || learning != wasLearning) autoButton.repaint();

    if (learning)
    {
        infoLabel.setColour (juce::Label::textColourId, col::accent);
        infoLabel.setText (proc.getLearnProgress() <= 0.0f
                               ? "Waiting for the kick - press play. Capture starts when the kick reaches the sidechain."
                               : "Listening to kick + bass... keep the loop playing.",
                           juce::dontSendNotification);
    }
    else if (proc.getAnalysisSerial() != seenSerial || wasLearning)
    {
        seenSerial = proc.getAnalysisSerial();
        const auto& a = proc.getLastAnalysis();
        const bool fresh = seenSerial > 0;
        infoLabel.setColour (juce::Label::textColourId, ! fresh ? col::textDim : a.valid ? col::text : col::warn);
        infoLabel.setText (! fresh ? hintText : a.valid ? a.message + "   " + juce::String::charToString (0x2192) + "   applied" : a.message, juce::dontSendNotification);
    }
    wasLearning = learning;

    if (const int prog = proc.getCurrentProgram(); prog != seenProgram)
    {
        seenProgram = prog;
        presetBox.setSelectedId (prog + 1, juce::dontSendNotification);
    }

    if (const float xa = lowOnlyButton.getToggleState() ? 1.0f : 0.4f; crossover.slider.getAlpha() != xa)
    {
        crossover.slider.setAlpha (xa); // crossover only matters in lows-only mode
        repaint (cards[2].bounds);
    }

    const auto env = std::make_tuple (attack.slider.getValue(), hold.slider.getValue(), release.slider.getValue());
    if (env != lastEnvelope)
    {
        lastEnvelope = env;
        repaint (cards[1].bounds); // envelope preview follows attack / hold / release
    }
}

void GSideChainEditor::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (ColourGradient (col::bg, 0, 0, col::bg2, 0, bounds.getBottom(), false));
    g.fillAll();
    g.setGradientFill (ColourGradient (col::accent2.withAlpha (0.10f), bounds.getRight() - 120.0f, 0, Colours::transparentBlack,
                                       bounds.getRight() - 520.0f, 320.0f, true));
    g.fillRect (bounds);

    // header
    const Rectangle<float> logo (24.0f, 14.0f, 36.0f, 36.0f);
    g.setGradientFill (ColourGradient (col::accent, logo.getX(), logo.getY(), col::accent2, logo.getRight(), logo.getBottom(), false));
    g.fillRoundedRectangle (logo, 9.0f);
    Path duck;
    duck.startNewSubPath (logo.getX() + 7.0f, logo.getY() + 12.0f);
    duck.lineTo (logo.getX() + 12.0f, logo.getY() + 12.0f);
    duck.lineTo (logo.getX() + 14.0f, logo.getY() + 26.0f);
    duck.quadraticTo (logo.getX() + 20.0f, logo.getY() + 13.0f, logo.getX() + 29.0f, logo.getY() + 12.0f);
    g.setColour (Colour (0xff07121c));
    g.strokePath (duck, PathStrokeType (2.4f, PathStrokeType::curved, PathStrokeType::rounded));

    const auto titleFont = font (21.0f, true).withExtraKerningFactor (0.03f);
    g.setFont (titleFont);
    g.setColour (col::accent);
    g.drawText ("G-", Rectangle<float> (72.0f, 12.0f, 40.0f, 24.0f), Justification::centredLeft);
    const float gw = GlyphArrangement::getStringWidth (titleFont, "G-");
    g.setColour (col::text);
    g.drawText ("SIDECHAIN", Rectangle<float> (72.0f + gw, 12.0f, 200.0f, 24.0f), Justification::centredLeft);
    g.setColour (col::textDim);
    g.setFont (font (9.5f, true).withExtraKerningFactor (0.22f));
    g.drawText ("AUTO  KICK / BASS  DUCKER", Rectangle<float> (73.0f, 36.0f, 260.0f, 14.0f), Justification::centredLeft);

    g.setColour (col::border.withAlpha (0.7f));
    g.drawHorizontalLine (64, 0.0f, bounds.getRight());

    // cards
    for (const auto& c : cards)
    {
        const auto r = c.bounds.toFloat();
        g.setColour (col::panel);
        g.fillRoundedRectangle (r, 12.0f);
        g.setColour (col::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 12.0f, 1.0f);
        g.setColour (col::accent);
        g.fillEllipse (r.getX() + 16.0f, r.getY() + 16.0f, 5.0f, 5.0f);
        g.setColour (col::textDim);
        g.setFont (font (10.5f, true).withExtraKerningFactor (0.16f));
        g.drawText (c.title, Rectangle<float> (r.getX() + 27.0f, r.getY() + 11.0f, r.getWidth() - 40.0f, 15.0f), Justification::centredLeft);
    }

    // knob names
    g.setFont (font (10.5f, true).withExtraKerningFactor (0.1f));
    for (auto* k : { &threshold, &ratio, &focus, &attack, &hold, &release, &depth, &crossover, &mix, &output })
    {
        g.setColour (col::textDim.withAlpha (k->slider.getAlpha()));
        g.drawText (k->name, k->slider.getBounds().withY (k->slider.getY() - 16).withHeight (14), Justification::centred);
    }

    // envelope shape preview (ENVELOPE card footer)
    {
        const auto area = cards[1].bounds.toFloat().withTrimmedTop ((float) cards[1].bounds.getHeight() - 52.0f).reduced (20.0f, 10.0f);
        g.setColour (col::bg2);
        g.fillRoundedRectangle (area.expanded (6.0f, 4.0f), 8.0f);

        const float a = (float) attack.slider.getValue(), h = (float) hold.slider.getValue(), r = (float) release.slider.getValue();
        const float total = (a + h + r * 4.0f) / 0.88f, t0 = total * 0.06f;
        Path line, fill;
        fill.startNewSubPath (area.getX(), area.getY());
        constexpr int steps = 120;
        for (int i = 0; i <= steps; ++i)
        {
            const float te = total * (float) i / (float) steps - t0;
            float v = 0.0f;
            if (te >= 0.0f)
            {
                const float reached = 1.0f - std::exp (-4.0f * std::min (te, a) / std::max (a, 0.01f));
                v = te < a + h ? reached : reached * std::exp (-(te - a - h) / std::max (r, 1.0f));
            }
            const float x = area.getX() + area.getWidth() * (float) i / (float) steps, y = area.getY() + area.getHeight() * v;
            fill.lineTo (x, y);
            if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
        }
        fill.lineTo (area.getRight(), area.getY());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (col::accent.withAlpha (0.25f), 0, area.getY(), col::accent2.withAlpha (0.05f), 0, area.getBottom(), false));
        g.fillPath (fill);
        g.setGradientFill (ColourGradient (col::accent2, area.getX(), 0, col::accent, area.getRight(), 0, false));
        g.strokePath (line, PathStrokeType (1.6f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // output footer: ducking summary
    {
        const auto area = cards[3].bounds.toFloat().withTrimmedTop ((float) cards[3].bounds.getHeight() - 52.0f).reduced (14.0f, 10.0f);
        g.setColour (col::textFaint);
        g.setFont (font (10.0f, true).withExtraKerningFactor (0.12f));
        g.drawText ("G AUDIO  |  v1.0", area, Justification::centred);
    }
}

void GSideChainEditor::resized()
{
    autoButton.setBounds (760, 14, 176, 36);
    prevButton.setBounds (344, 16, 32, 32);
    presetBox.setBounds (382, 16, 236, 32);
    nextButton.setBounds (624, 16, 32, 32);

    scope.setBounds (24, 76, 852, 216);
    meter.setBounds (888, 76, 48, 216);
    infoLabel.setBounds (20, 298, 920, 26);

    const int top = 332, height = 228, gap = 12, total = 912;
    const int units[] = { 3, 3, 2, 2 };
    const float unit = (float) (total - gap * 3) / 10.0f;
    int x = 24;
    for (size_t i = 0; i < cards.size(); ++i)
    {
        const int w = juce::roundToInt (unit * (float) units[i]);
        cards[i].bounds = { x, top, w, height };
        x += w + gap;
    }

    auto place = [] (const Card& c, std::initializer_list<Knob*> knobs)
    {
        const auto row = c.bounds.withTrimmedTop (44).withHeight (112).reduced (8, 0);
        const int cw = row.getWidth() / (int) knobs.size();
        int i = 0;
        for (auto* k : knobs)
            k->slider.setBounds (row.withX (row.getX() + cw * i++).withWidth (cw).withTrimmedTop (14));
    };
    place (cards[0], { &threshold, &ratio, &focus });
    place (cards[1], { &attack, &hold, &release });
    place (cards[2], { &depth, &crossover });
    place (cards[3], { &mix, &output });

    auto footerButton = [] (juce::Button& b, const Card& c)
    {
        b.setBounds (c.bounds.getCentreX() - 72, c.bounds.getBottom() - 46, 144, 30);
    };
    footerButton (listenButton, cards[0]);
    footerButton (lowOnlyButton, cards[2]);
}
