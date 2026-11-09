#pragma once
#include <array>
#include "PluginProcessor.h"
#include "AeroGfx.h"
#include "AeroLookAndFeel.h"

//==============================================================================
// Scrolling pitch-roll: light line = what you sang, green line = what comes out.
// Lanes are lit for notes that are allowed targets.
class PitchDisplay : public juce::Component, private juce::Timer
{
public:
    explicit PitchDisplay (AeroTuneAudioProcessor& p) : proc (p)
    {
        inHist.fill (-1.0f);
        outHist.fill (-1.0f);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        auto b = getLocalBounds().toFloat();

        Path shape; shape.addRoundedRectangle (b, 16.0f);
        g.setGradientFill (ColourGradient (Colour (0xd8043a78), 0, b.getY(), Colour (0xd80a72b8), 0, b.getBottom(), false));
        g.fillPath (shape);

        Path gl; gl.addRoundedRectangle (b.getX() + 3, b.getY() + 3, b.getWidth() - 6, b.getHeight() * 0.38f, 14.0f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.22f), 0, b.getY(), Colours::white.withAlpha (0.0f), 0, b.getY() + b.getHeight() * 0.4f, false));
        g.fillPath (gl);
        g.setColour (Colours::white.withAlpha (0.55f));
        g.strokePath (shape, PathStrokeType (1.5f));

        auto left = b.removeFromLeft (196.0f);
        g.setColour (Colours::white.withAlpha (0.20f));
        g.drawVerticalLine ((int) left.getRight(), b.getY() + 14, b.getBottom() - 14);

        paintReadout (g, left.reduced (18.0f, 14.0f));
        paintGraph (g, b.reduced (8.0f, 12.0f).withTrimmedRight (6.0f));
    }

private:
    static constexpr int kHist = 270;   // 9 s at 30 Hz

    void timerCallback() override
    {
        lastIn  = proc.getEngine().uiInMidi.load();
        lastOut = proc.getEngine().uiOutMidi.load();
        inHist[(size_t) head]  = lastIn;
        outHist[(size_t) head] = lastOut;
        head = (head + 1) % kHist;
        if (lastOut >= 0.0f)
            viewCentre += 0.05f * (juce::jlimit (30.0f, 100.0f, lastOut) - viewCentre);
        repaint();
    }

    void paintReadout (juce::Graphics& g, juce::Rectangle<float> info)
    {
        using namespace juce;
        const bool voiced = lastIn >= 0.0f;
        const Colour dim = Colours::white.withAlpha (0.65f);

        g.setFont (AeroGfx::font (11.0f, true)); g.setColour (dim);
        g.drawText ("DETECTED", info.removeFromTop (14.0f), Justification::centredLeft);
        g.setFont (AeroGfx::font (48.0f, true)); g.setColour (Colours::white);
        g.drawText (voiced ? AeroParams::noteLabel (roundToInt (lastIn)) : String ("--"), info.removeFromTop (56.0f), Justification::centredLeft);

        g.setFont (AeroGfx::font (11.0f, true)); g.setColour (dim);
        g.drawText ("OUTPUT", info.removeFromTop (14.0f), Justification::centredLeft);
        g.setFont (AeroGfx::font (24.0f, true)); g.setColour (Colour (0xffa6ff55));
        g.drawText (voiced ? AeroParams::noteLabel (roundToInt (lastOut)) : String ("--"), info.removeFromTop (30.0f), Justification::centredLeft);

        info.removeFromTop (8.0f);
        auto mr = info.removeFromTop (12.0f);
        g.setColour (Colours::white.withAlpha (0.20f));
        g.fillRoundedRectangle (mr, 6.0f);
        g.setColour (Colours::white.withAlpha (0.70f));
        g.fillRect (mr.getCentreX() - 0.75f, mr.getY() - 2.0f, 1.5f, mr.getHeight() + 4.0f);

        if (voiced)
        {
            const float dev = jlimit (-50.0f, 50.0f, (lastIn - std::round (lastIn)) * 100.0f);
            const float mx = mr.getCentreX() + dev / 50.0f * (mr.getWidth() * 0.5f - 6.0f);
            const Colour col = std::abs (dev) < 10.0f ? Colour (0xffa6ff55) : Colour (0xffffc24a);
            g.setColour (col);
            g.fillEllipse (mx - 6.0f, mr.getY(), 12.0f, 12.0f);
            g.setColour (Colours::white.withAlpha (0.8f));
            g.fillEllipse (mx - 3.0f, mr.getY() + 1.5f, 5.0f, 3.0f);

            g.setFont (AeroGfx::font (11.0f, false)); g.setColour (dim);
            g.drawText ((dev >= 0 ? "+" : "") + String (roundToInt (dev)) + " cents", info.removeFromTop (16.0f), Justification::centred);
        }
    }

    void paintGraph (juce::Graphics& g, juce::Rectangle<float> graph)
    {
        using namespace juce;
        const float span = 16.0f;
        const float lo = viewCentre - span * 0.5f;
        auto yFor = [&] (float m) { return graph.getBottom() - (m - lo) / span * graph.getHeight(); };

        g.saveState();
        g.reduceClipRegion (graph.toNearestInt());

        const int mask = proc.uiNoteMask.load();
        g.setFont (AeroGfx::font (9.5f, true));
        for (int n = (int) std::floor (lo) - 1; n <= (int) std::ceil (lo + span) + 1; ++n)
        {
            const int pc = ((n % 12) + 12) % 12;
            const bool allowed = ((mask >> pc) & 1) != 0;
            const bool black = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
            const float y0 = yFor ((float) n + 0.5f), y1 = yFor ((float) n - 0.5f);
            const Rectangle<float> lane (graph.getX(), y0, graph.getWidth(), y1 - y0);

            if (allowed)
            {
                g.setColour (Colour (0xff7dff4d).withAlpha (0.14f));
                g.fillRect (lane);
                g.setColour (Colours::white.withAlpha (0.12f));
                g.drawHorizontalLine ((int) yFor ((float) n), graph.getX(), graph.getRight());
                g.setColour (Colours::white.withAlpha (0.75f));
                g.drawText (AeroParams::noteLabel (n), Rectangle<float> (graph.getX() + 4.0f, yFor ((float) n) - 7.0f, 40.0f, 14.0f), Justification::centredLeft);
            }
            else if (black)
            {
                g.setColour (Colours::black.withAlpha (0.12f));
                g.fillRect (lane);
            }
        }

        auto build = [&] (const std::array<float, kHist>& h)
        {
            Path p; bool pen = false;
            for (int i = 0; i < kHist; ++i)
            {
                const float v = h[(size_t) ((head + i) % kHist)];
                if (v < 0.0f) { pen = false; continue; }
                const float x = graph.getX() + graph.getWidth() * (float) i / (float) (kHist - 1);
                if (! pen) { p.startNewSubPath (x, yFor (v)); pen = true; } else p.lineTo (x, yFor (v));
            }
            return p;
        };

        const auto stroke = [&] (const Path& p, Colour c)
        {
            g.setColour (c.withAlpha (0.25f));
            g.strokePath (p, PathStrokeType (6.0f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (c);
            g.strokePath (p, PathStrokeType (2.0f, PathStrokeType::curved, PathStrokeType::rounded));
        };
        stroke (build (inHist),  Colour (0xffbfefff));
        stroke (build (outHist), Colour (0xffa6ff55));
        g.restoreState();
    }

    AeroTuneAudioProcessor& proc;
    std::array<float, kHist> inHist {}, outHist {};
    int head = 0;
    float lastIn = -1.0f, lastOut = -1.0f, viewCentre = 60.0f;
};

//==============================================================================
// 12 glossy pill buttons: click a note to allow / forbid it as a correction target.
class NoteKeys : public juce::Component
{
public:
    explicit NoteKeys (juce::AudioProcessorValueTreeState& apvts)
    {
        for (int i = 0; i < 12; ++i)
        {
            attachments[(size_t) i] = std::make_unique<juce::ParameterAttachment> (
                *apvts.getParameter (AeroParams::noteId (i)),
                [this, i] (float v) { on[(size_t) i] = v > 0.5f; repaint(); });
            attachments[(size_t) i]->sendInitialUpdate();
        }
    }

    void applyMask (int mask)
    {
        for (int i = 0; i < 12; ++i)
            attachments[(size_t) i]->setValueAsCompleteGesture (((mask >> i) & 1) ? 1.0f : 0.0f);
    }

    void paint (juce::Graphics& g) override
    {
        using namespace juce;
        const auto b = getLocalBounds().toFloat();
        const float gap = 6.0f, kw = (b.getWidth() - gap * 11.0f) / 12.0f;

        for (int i = 0; i < 12; ++i)
        {
            const Rectangle<float> r (b.getX() + (float) i * (kw + gap), b.getY(), kw, b.getHeight());
            const bool sharp = i == 1 || i == 3 || i == 6 || i == 8 || i == 10;
            const bool isOn = on[(size_t) i];
            const float cr = r.getHeight() * 0.5f;

            if (isOn)
            {
                g.setColour (Colour (0xff9cf53b).withAlpha (0.28f));
                g.drawRoundedRectangle (r.expanded (1.5f), cr + 1.5f, 3.0f);
                g.setGradientFill (ColourGradient (sharp ? Colour (0xff86e450) : Colour (0xffc9ff82), 0, r.getY(),
                                                   sharp ? Colour (0xff2f9e2a) : Colour (0xff46c42e), 0, r.getBottom(), false));
            }
            else
            {
                g.setGradientFill (ColourGradient (Colours::white.withAlpha (sharp ? 0.22f : 0.40f), 0, r.getY(),
                                                   Colour (0xff6cb8e8).withAlpha (sharp ? 0.30f : 0.18f), 0, r.getBottom(), false));
            }
            g.fillRoundedRectangle (r, cr);

            Path gloss; gloss.addRoundedRectangle (r.getX() + 2, r.getY() + 1.5f, r.getWidth() - 4, r.getHeight() * 0.48f, cr - 2.0f);
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.65f), 0, r.getY(), Colours::white.withAlpha (0.05f), 0, r.getCentreY(), false));
            g.fillPath (gloss);

            g.setColour (Colours::white.withAlpha (isOn ? 0.90f : 0.55f));
            g.drawRoundedRectangle (r, cr, 1.3f);

            g.setFont (AeroGfx::font (13.0f, true));
            g.setColour (isOn ? Colour (0xff0b4a1e) : Colour (0xff2b6a9a).withAlpha (0.8f));
            g.drawText (AeroParams::noteNames[i], r, Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const float gap = 6.0f, kw = ((float) getWidth() - gap * 11.0f) / 12.0f;
        const int i = juce::jlimit (0, 11, (int) (e.position.x / (kw + gap)));
        attachments[(size_t) i]->setValueAsCompleteGesture (on[(size_t) i] ? 0.0f : 1.0f);
    }

private:
    std::array<std::unique_ptr<juce::ParameterAttachment>, 12> attachments;
    std::array<bool, 12> on {};
};
