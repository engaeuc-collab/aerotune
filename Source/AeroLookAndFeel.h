#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "AeroGfx.h"

class AeroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AeroLookAndFeel()
    {
        using namespace juce;
        setColour (ComboBox::textColourId,  navy);
        setColour (ComboBox::arrowColourId, navy);
        setColour (PopupMenu::backgroundColourId,            Colour (0xf2e6f6ff));
        setColour (PopupMenu::textColourId,                  navy);
        setColour (PopupMenu::highlightedBackgroundColourId, Colour (0xff2aa8e8));
        setColour (PopupMenu::highlightedTextColourId,       Colours::white);
        setColour (Slider::textBoxTextColourId,              navy);
        setColour (Slider::textBoxOutlineColourId,           Colours::transparentBlack);
        setColour (Slider::textBoxBackgroundColourId,        Colours::transparentBlack);
        setColour (TextEditor::highlightColourId,            Colour (0xff2aa8e8));
    }

    static inline const juce::Colour navy { 0xff0a3f73 };

    juce::Font getComboBoxFont (juce::ComboBox&) override { return AeroGfx::font (14.0f, true); }
    juce::Font getPopupMenuFont() override               { return AeroGfx::font (14.0f, false); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (12, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool isDown, int, int, int, int, juce::ComboBox&) override
    {
        using namespace juce;
        auto r = Rectangle<float> (0, 0, (float) w, (float) h).reduced (1.0f);
        const float cr = r.getHeight() * 0.5f;

        g.setGradientFill (ColourGradient (Colour (0xfff6fdff), 0, r.getY(),
                                           isDown ? Colour (0xff6cc4ee) : Colour (0xff8fd6f5), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, cr);

        Path gloss; gloss.addRoundedRectangle (r.getX() + 2, r.getY() + 1.5f, r.getWidth() - 4, r.getHeight() * 0.5f, cr - 2.0f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.75f), 0, r.getY(), Colours::white.withAlpha (0.10f), 0, r.getCentreY(), false));
        g.fillPath (gloss);

        g.setColour (Colour (0xff3aa0d8));
        g.drawRoundedRectangle (r, cr, 1.2f);

        Path arrow;
        const float ax = (float) w - cr - 6.0f, ay = r.getCentreY();
        arrow.addTriangle (ax - 5, ay - 2.5f, ax + 5, ay - 2.5f, ax, ay + 3.5f);
        g.setColour (navy);
        g.fillPath (arrow);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider&) override
    {
        using namespace juce;
        auto bounds = Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float d = jmin (bounds.getWidth(), bounds.getHeight());
        const auto c = bounds.getCentre();
        const float r = d * 0.5f;
        const float angle = startAngle + pos * (endAngle - startAngle);
        const auto ring = Rectangle<float> (d, d).withCentre (c);

        // track
        Path track; track.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, endAngle, true);
        g.setColour (Colours::white.withAlpha (0.45f));
        g.strokePath (track, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (Colour (0xff3aa0d8).withAlpha (0.35f));
        g.strokePath (track, PathStrokeType (1.0f));

        // value arc
        if (pos > 0.002f)
        {
            Path val; val.addCentredArc (c.x, c.y, r - 3.0f, r - 3.0f, 0.0f, startAngle, angle, true);
            g.setGradientFill (ColourGradient (Colour (0xff22d3ee), ring.getX(), ring.getBottom(),
                                               Colour (0xff9cf53b), ring.getRight(), ring.getBottom(), false));
            g.strokePath (val, PathStrokeType (5.0f, PathStrokeType::curved, PathStrokeType::rounded));
        }

        // orb
        const auto body = ring.reduced (12.0f);
        g.setColour (Colour (0x40043a78));
        g.fillEllipse (body.translated (0.0f, 3.0f).expanded (1.0f));

        g.setGradientFill (ColourGradient (Colour (0xff8ceaff), 0, body.getY(), Colour (0xff0a78d2), 0, body.getBottom(), false));
        g.fillEllipse (body);

        ColourGradient glowLow (Colour (0xaa9cf53b), body.getCentreX(), body.getBottom() - 2.0f, Colour (0x009cf53b), body.getCentreX(), body.getCentreY(), true);
        g.setGradientFill (glowLow);
        g.fillEllipse (body);

        g.setColour (Colours::white.withAlpha (0.75f));
        g.drawEllipse (body, 1.5f);

        const Rectangle<float> gloss (body.getX() + body.getWidth() * 0.12f, body.getY() + body.getHeight() * 0.05f,
                                      body.getWidth() * 0.76f, body.getHeight() * 0.50f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.92f), 0, gloss.getY(), Colours::white.withAlpha (0.08f), 0, gloss.getBottom(), false));
        g.fillEllipse (gloss);

        // pointer
        Path p; p.addRoundedRectangle (-2.0f, -body.getHeight() * 0.5f + 5.0f, 4.0f, body.getHeight() * 0.22f, 2.0f);
        p.applyTransform (AffineTransform::rotation (angle).translated (c.x, c.y));
        g.setColour (Colour (0xff083a6a).withAlpha (0.35f));
        g.fillPath (p, AffineTransform::translation (0.0f, 1.0f));
        g.setColour (Colours::white);
        g.fillPath (p);
    }
};
