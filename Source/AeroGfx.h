#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// Procedural Frutiger Aero artwork: sky, sun, clouds, rainbow, glass towers, grass, bubbles, glossy glass panels.
// Everything is drawn in code, so the repo ships with no image assets.
namespace AeroGfx
{
    inline juce::Font font (float height, bool bold = false)
    {
       #if JUCE_WINDOWS
        const juce::String face = "Segoe UI";
       #elif JUCE_MAC
        const juce::String face = "Helvetica Neue";
       #else
        const juce::String face = juce::Font::getDefaultSansSerifFontName();
       #endif
        return juce::Font (juce::FontOptions (face, height, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline void drawBubble (juce::Graphics& g, juce::Point<float> c, float r, float a = 1.0f)
    {
        using namespace juce;
        const Rectangle<float> b = Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);

        Path disc; disc.addEllipse (b);
        g.saveState();
        g.reduceClipRegion (disc);

        ColourGradient body (Colour (0x00ffffff), c.x - r * 0.25f, c.y - r * 0.3f,
                             Colour::fromFloatRGBA (0.35f, 0.85f, 1.0f, 0.42f * a), c.x, c.y + r, true);
        g.setGradientFill (body);
        g.fillRect (b);

        ColourGradient caustic (Colours::white.withAlpha (0.55f * a), c.x, c.y + r * 0.95f,
                                Colours::white.withAlpha (0.0f),      c.x, c.y + r * 0.15f, false);
        g.setGradientFill (caustic);
        g.fillRect (b);
        g.restoreState();

        g.setColour (Colours::white.withAlpha (0.75f * a));
        g.drawEllipse (b.reduced (0.6f), juce::jmax (1.0f, r * 0.045f));

        g.setColour (Colours::white.withAlpha (0.85f * a));
        g.fillEllipse (Rectangle<float> (r * 0.55f, r * 0.30f).withCentre ({ c.x - r * 0.38f, c.y - r * 0.55f }));
        g.setColour (Colours::white.withAlpha (0.35f * a));
        g.fillEllipse (Rectangle<float> (r * 0.18f, r * 0.18f).withCentre ({ c.x + r * 0.45f, c.y - r * 0.45f }));
    }

    inline void drawCloud (juce::Graphics& g, float cx, float cy, float s, float a)
    {
        using namespace juce;
        struct P { float x, y, w, h; };
        static const P puffs[] = { { -60, 2, 90, 38 }, { -20, -18, 82, 52 }, { 32, -10, 80, 44 }, { 72, 4, 70, 32 }, { 6, 10, 160, 30 } };
        for (auto& p : puffs)
        {
            Rectangle<float> r (cx + p.x * s - p.w * s * 0.5f, cy + p.y * s - p.h * s * 0.5f, p.w * s, p.h * s);
            ColourGradient gr (Colours::white.withAlpha (0.95f * a), r.getCentreX(), r.getY(),
                               Colour (0xffcfe9ff).withAlpha (0.65f * a), r.getCentreX(), r.getBottom(), false);
            g.setGradientFill (gr);
            g.fillEllipse (r);
        }
    }

    inline void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        using namespace juce;
        Path p; p.addRoundedRectangle (r, corner);
        DropShadow (Colour (0x55043a78), 16, { 0, 6 }).drawForPath (g, p);

        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.46f), 0, r.getY(),
                                           Colour (0xff9fe0ff).withAlpha (0.20f), 0, r.getBottom(), false));
        g.fillPath (p);

        Path gloss; gloss.addRoundedRectangle (r.getX() + 3, r.getY() + 3, r.getWidth() - 6, r.getHeight() * 0.42f, corner - 2.0f);
        g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.50f), 0, r.getY(),
                                           Colours::white.withAlpha (0.02f), 0, r.getY() + r.getHeight() * 0.45f, false));
        g.fillPath (gloss);

        g.setColour (Colours::white.withAlpha (0.85f));
        g.strokePath (p, PathStrokeType (1.6f));
        g.setColour (Colour (0xff48b4f0).withAlpha (0.35f));
        g.strokePath (p, PathStrokeType (0.8f));
    }

    inline void paintScene (juce::Graphics& g, int wi, int hi)
    {
        using namespace juce;
        const float w = (float) wi, h = (float) hi;

        // sky
        {
            ColourGradient sky (Colour (0xff0a6fe0), 0, 0, Colour (0xffa6e8ff), 0, h * 0.80f, false);
            sky.addColour (0.45, Colour (0xff3ea8f5));
            g.setGradientFill (sky);
            g.fillRect (0.0f, 0.0f, w, h);
        }

        // sun + rays
        const float sx = w * 0.84f, sy = 54.0f;
        {
            ColourGradient glow (Colours::white.withAlpha (0.95f), sx, sy, Colours::white.withAlpha (0.0f), sx + 330, sy, true);
            glow.addColour (0.18, Colour (0xfffffbd0).withAlpha (0.75f));
            glow.addColour (0.50, Colour (0xffbff0ff).withAlpha (0.25f));
            g.setGradientFill (glow);
            g.fillEllipse (sx - 330, sy - 330, 660, 660);

            for (int i = 0; i < 18; ++i)
            {
                const float a = (float) i * MathConstants<float>::twoPi / 18.0f + 0.1f, sp = 0.035f;
                Path ray;
                ray.startNewSubPath (sx, sy);
                ray.lineTo (sx + std::cos (a - sp) * 950, sy + std::sin (a - sp) * 950);
                ray.lineTo (sx + std::cos (a + sp) * 950, sy + std::sin (a + sp) * 950);
                ray.closeSubPath();
                g.setColour (Colours::white.withAlpha (0.07f));
                g.fillPath (ray);
            }
            g.setColour (Colours::white);
            g.fillEllipse (sx - 17, sy - 17, 34, 34);
            drawBubble (g, { sx - 120, sy + 90 }, 22, 0.55f);
            drawBubble (g, { sx - 190, sy + 140 }, 12, 0.5f);
            drawBubble (g, { sx - 60, sy + 170 }, 8, 0.5f);
        }

        // clouds
        drawCloud (g, w * 0.10f, 150, 1.0f, 0.90f);
        drawCloud (g, w * 0.50f, 112, 0.8f, 0.85f);
        drawCloud (g, w * 0.30f, 270, 1.2f, 0.75f);
        drawCloud (g, w * 0.90f, 300, 0.9f, 0.80f);

        // rainbow
        {
            const Colour cols[] = { Colour (0xffff4d4d), Colour (0xffffa64d), Colour (0xffffe44d), Colour (0xff6be04a),
                                    Colour (0xff4dc3ff), Colour (0xff4d6bff), Colour (0xffa24dff) };
            const float cx = w * 0.50f, cy = h * 0.86f;
            for (int i = 0; i < 7; ++i)
            {
                const float rad = 400.0f - (float) i * 9.0f;
                Path p; p.addCentredArc (cx, cy, rad, rad, 0.0f, -1.25f, 1.25f, true);
                g.setColour (cols[i].withAlpha (0.30f));
                g.strokePath (p, PathStrokeType (9.0f));
            }
        }

        // glass skyline
        {
            const float baseY = h * 0.80f;
            Random rng (7);
            float x = -10.0f;
            while (x < w)
            {
                const float tw = 28.0f + rng.nextFloat() * 36.0f;
                const float th = 60.0f + rng.nextFloat() * rng.nextFloat() * 230.0f;
                Rectangle<float> t (x, baseY - th, tw, th);
                g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.62f), 0, t.getY(),
                                                   Colour (0xff6fc2f5).withAlpha (0.38f), 0, t.getBottom(), false));
                g.fillRect (t);
                g.setColour (Colours::white.withAlpha (0.30f));
                g.fillRect (t.getX() + tw * 0.14f, t.getY(), tw * 0.14f, th);
                g.setColour (Colours::white.withAlpha (0.65f));
                g.drawRect (t, 0.8f);
                if (rng.nextFloat() > 0.72f)
                {
                    Path spire; spire.addTriangle (t.getX() + tw * 0.2f, t.getY(), t.getRight() - tw * 0.2f, t.getY(), t.getCentreX(), t.getY() - 26.0f);
                    g.setColour (Colours::white.withAlpha (0.55f));
                    g.fillPath (spire);
                }
                x += tw + 3.0f + rng.nextFloat() * 10.0f;
            }
            g.setGradientFill (ColourGradient (Colours::white.withAlpha (0.0f), 0, baseY - 70, Colours::white.withAlpha (0.55f), 0, baseY + 6, false));
            g.fillRect (0.0f, baseY - 70, w, 76.0f);
        }

        // grass hill
        {
            const float y0 = 0.84f * h, y1 = 0.78f * h, y2 = 0.88f * h, y3 = 0.81f * h;
            Path hill;
            hill.startNewSubPath (0, y0);
            hill.cubicTo (w * 0.25f, y1, w * 0.60f, y2, w, y3);
            hill.lineTo (w, h); hill.lineTo (0, h); hill.closeSubPath();
            ColourGradient gr (Colour (0xff9af05a), 0, h * 0.78f, Colour (0xff0f6b22), 0, h, false);
            gr.addColour (0.35, Colour (0xff3fb534));
            g.setGradientFill (gr);
            g.fillPath (hill);

            Random rng (42);
            for (int i = 0; i < 360; ++i)
            {
                const float x = rng.nextFloat() * w, t = x / w, mt = 1.0f - t;
                const float ridge = mt * mt * mt * y0 + 3 * mt * mt * t * y1 + 3 * mt * t * t * y2 + t * t * t * y3;
                const float y = ridge + rng.nextFloat() * (h - ridge);
                const float len = 10.0f + rng.nextFloat() * 22.0f, lean = (rng.nextFloat() - 0.5f) * 14.0f;
                Path blade; blade.startNewSubPath (x, y); blade.quadraticTo (x + lean * 0.4f, y - len * 0.6f, x + lean, y - len);
                g.setColour (Colour::fromHSV (0.25f + rng.nextFloat() * 0.07f, 0.75f, 0.60f + rng.nextFloat() * 0.40f, 0.85f));
                g.strokePath (blade, PathStrokeType (1.5f));
            }
        }

        // foreground glass spheres
        drawBubble (g, { w * 0.10f, h * 0.935f }, 46, 1.0f);
        drawBubble (g, { w * 0.90f, h * 0.915f }, 64, 1.0f);
        drawBubble (g, { w * 0.22f, h * 0.975f }, 22, 0.9f);
        drawBubble (g, { w * 0.78f, h * 0.975f }, 28, 0.9f);
    }
}
