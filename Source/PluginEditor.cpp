#include "PluginEditor.h"

namespace
{
    constexpr int kW = 900, kH = 620;
    const juce::Rectangle<int> panelA { 24, 78, 852, 208 };
    const juce::Rectangle<int> panelB { 24, 300, 852, 222 };
}

AeroTuneAudioProcessorEditor::AeroTuneAudioProcessorEditor (AeroTuneAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p), keys (p.apvts)
{
    setLookAndFeel (&laf);
    setSize (kW, kH);
    setResizable (false, false);

    const auto navy = AeroLookAndFeel::navy;

    auto setupLabel = [this] (juce::Label& l, const juce::String& text, juce::Justification j, float size, juce::Colour c)
    {
        l.setText (text, juce::dontSendNotification);
        l.setJustificationType (j);
        l.setFont (AeroGfx::font (size, true));
        l.setColour (juce::Label::textColourId, c);
        l.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (l);
    };

    addAndMakeVisible (display);
    addAndMakeVisible (keys);

    // key / scale / range
    keyBox.addItemList   (AeroParams::noteNames,  1);
    scaleBox.addItemList (AeroParams::scaleNames, 1);
    rangeBox.addItemList (AeroParams::rangeNames, 1);
    addAndMakeVisible (keyBox); addAndMakeVisible (scaleBox); addAndMakeVisible (rangeBox);
    keyAtt   = std::make_unique<ComboAtt> (p.apvts, "key",   keyBox);
    scaleAtt = std::make_unique<ComboAtt> (p.apvts, "scale", scaleBox);
    rangeAtt = std::make_unique<ComboAtt> (p.apvts, "range", rangeBox);

    // Choosing a key/scale lights the matching notes; the notes can then be edited by hand.
    auto applyScale = [this]
    {
        const int key = keyBox.getSelectedItemIndex(), sc = scaleBox.getSelectedItemIndex();
        if (key < 0 || sc < 0) return;
        const int base = AeroParams::scaleMasks[sc];
        int mask = 0;
        for (int i = 0; i < 12; ++i)
            if ((base >> ((i - key + 12) % 12)) & 1) mask |= (1 << i);
        keys.applyMask (mask);
    };
    keyBox.onChange   = applyScale;
    scaleBox.onChange = applyScale;

    setupLabel (keyLabel,   "KEY",   juce::Justification::centredRight, 12.0f, navy);
    setupLabel (scaleLabel, "SCALE", juce::Justification::centredRight, 12.0f, navy);
    setupLabel (rangeLabel, "RANGE", juce::Justification::centredRight, 12.0f, navy);
    setupLabel (hintLabel,  "click a note to allow / mute it", juce::Justification::centredRight, 11.0f, navy.withAlpha (0.65f));

    // knobs
    struct Spec { const char* id; const char* name; };
    static const Spec specs[6] = { { "speed", "RETUNE SPEED" }, { "amount", "AMOUNT" }, { "detune", "DETUNE" },
                                   { "ref", "A4 REFERENCE" },   { "mix", "MIX" },       { "output", "OUTPUT" } };
    for (size_t i = 0; i < 6; ++i)
    {
        auto& k = knobs[i];
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 18);
        addAndMakeVisible (k.slider);
        k.att = std::make_unique<SliderAtt> (p.apvts, specs[i].id, k.slider);
        setupLabel (k.label, specs[i].name, juce::Justification::centred, 12.0f, navy);
    }

    // ambient bubbles
    juce::Random rng (2009);
    for (int i = 0; i < 26; ++i)
        bubbles.push_back ({ rng.nextFloat() * kW, rng.nextFloat() * kH,
                             3.0f + rng.nextFloat() * rng.nextFloat() * 22.0f,
                             0.15f + rng.nextFloat() * 0.7f, rng.nextFloat() * 6.28f });

    renderBackground();
    startTimerHz (30);
}

AeroTuneAudioProcessorEditor::~AeroTuneAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

void AeroTuneAudioProcessorEditor::renderBackground()
{
    using namespace juce;
    const float scale = 2.0f;   // render at 2x so it stays crisp on HiDPI displays
    background = Image (Image::RGB, (int) (kW * scale), (int) (kH * scale), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (scale));

    AeroGfx::paintScene (g, kW, kH);
    AeroGfx::drawGlassPanel (g, panelA.toFloat(), 22.0f);
    AeroGfx::drawGlassPanel (g, panelB.toFloat(), 22.0f);

    // logo orb
    const Rectangle<float> orb (30.0f, 14.0f, 46.0f, 46.0f);
    g.setColour (Colour (0x55043a78));
    g.fillEllipse (orb.translated (0.0f, 3.0f));
    g.setGradientFill (ColourGradient (Colour (0xffe6ffb0), orb.getCentreX() - 8, orb.getCentreY() - 10,
                                       Colour (0xff159a35), orb.getCentreX() + 22, orb.getCentreY() + 22, true));
    g.fillEllipse (orb);
    g.setColour (Colours::white.withAlpha (0.9f));
    g.drawEllipse (orb, 1.5f);
    g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.9f), 0, orb.getY() + 3, Colours::white.withAlpha (0.0f), 0, orb.getCentreY(), false));
    g.fillEllipse (orb.getX() + 6, orb.getY() + 3, orb.getWidth() - 12, orb.getHeight() * 0.5f);
    AeroGfx::drawBubble (g, { 66.0f, 54.0f }, 9.0f, 0.9f);

    // title
    g.setFont (AeroGfx::font (38.0f, true));
    g.setColour (Colour (0x66043a78));
    g.drawText ("AeroTune", Rectangle<int> (91, 12, 320, 44), Justification::centredLeft);
    g.setColour (Colours::white);
    g.drawText ("AeroTune", Rectangle<int> (90, 10, 320, 44), Justification::centredLeft);
    g.setFont (AeroGfx::font (11.0f, true));
    g.setColour (Colours::white.withAlpha (0.92f));
    g.drawText ("REAL-TIME STEREO VOCAL PITCH CORRECTION", Rectangle<int> (92, 50, 420, 16), Justification::centredLeft);

    // footer
    g.setFont (AeroGfx::font (12.0f, true));
    g.setColour (Colour (0x66043a78));
    g.drawText ("AeroTune  |  VST3  |  open source", Rectangle<int> (0, 586, kW, 18), Justification::centred);
    g.setColour (Colours::white.withAlpha (0.95f));
    g.drawText ("AeroTune  |  VST3  |  open source", Rectangle<int> (0, 585, kW, 18), Justification::centred);
}

void AeroTuneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImage (background, getLocalBounds().toFloat());

    for (const auto& b : bubbles)
        AeroGfx::drawBubble (g, { b.x, b.y }, b.r, 0.8f);
}

void AeroTuneAudioProcessorEditor::timerCallback()
{
    for (auto& b : bubbles)
    {
        b.y -= b.speed;
        b.phase += 0.03f;
        b.x += std::sin (b.phase) * 0.35f;
        if (b.y < -b.r * 2.0f) { b.y = (float) kH + b.r; b.x = (float) (std::fmod (b.x * 7.3f + 131.0f, (float) kW)); }
    }
    repaint();
}

void AeroTuneAudioProcessorEditor::resized()
{
    display.setBounds (panelA.reduced (14));

    const int y1 = 316;
    keyLabel.setBounds   (40,  y1, 38, 28);  keyBox.setBounds   (82,  y1, 72,  28);
    scaleLabel.setBounds (160, y1, 56, 28);  scaleBox.setBounds (220, y1, 160, 28);
    rangeLabel.setBounds (390, y1, 56, 28);  rangeBox.setBounds (450, y1, 200, 28);
    hintLabel.setBounds  (656, y1, 200, 28);

    keys.setBounds (44, 354, 812, 34);

    const int kw = 812 / 6;
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        const int x = 44 + (int) i * kw;
        knobs[i].label.setBounds  (x, 396, kw, 16);
        knobs[i].slider.setBounds (x + 8, 410, kw - 16, 106);
    }
}
