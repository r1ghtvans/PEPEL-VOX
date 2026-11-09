#include "PluginEditor.h"
#include "Sprites.h"
#include <BinaryData.h>

using juce::Colour;
using juce::Graphics;
using juce::Rectangle;

// ------------------------------------------------------------------ fonts
namespace pixelfont
{
static juce::Typeface::Ptr tfVhs()
{
    static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::VT323Regular_ttf, (size_t) BinaryData::VT323Regular_ttfSize);
    return t;
}
static juce::Typeface::Ptr tfTitle()
{
    static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::PressStart2PRegular_ttf, (size_t) BinaryData::PressStart2PRegular_ttfSize);
    return t;
}
juce::Font vhs (float h) { return juce::Font (juce::FontOptions (tfVhs()).withHeight (h)); }
juce::Font title (float h) { return juce::Font (juce::FontOptions (tfTitle()).withHeight (h)); }
}

// ------------------------------------------------------------------ pixel helpers
namespace
{
struct SpritePal
{
    Colour c[128];
    void set (char ch, Colour col) { c[(int) ch] = col; }
};

template <size_t N>
void drawSprite (Graphics& g, const char* const (&rows)[N], int x, int y, int s, const SpritePal& pal, float alpha = 1.f)
{
    for (size_t j = 0; j < N; ++j)
        for (const char* p = rows[j]; *p; ++p)
        {
            if (*p == '.') continue;
            const Colour col = pal.c[(int) *p];
            if (col.isTransparent()) continue;
            g.setColour (col.withMultipliedAlpha (alpha));
            g.fillRect (x + (int) (p - rows[j]) * s, y + (int) j * s, s, s);
        }
}
template <size_t N> int spriteW (const char* const (&rows)[N]) { return (int) std::strlen (rows[0]); }

SpritePal mono (Colour c) { SpritePal p; p.set ('#', c); return p; }

juce::Path steppedPath (Rectangle<int> r, int s)
{
    const float x = (float) r.getX(), y = (float) r.getY(), w = (float) r.getWidth(), h = (float) r.getHeight(), t = (float) s;
    juce::Path p;
    p.startNewSubPath (x + 2 * t, y);
    p.lineTo (x + w - 2 * t, y);         p.lineTo (x + w - 2 * t, y + t);
    p.lineTo (x + w - t, y + t);         p.lineTo (x + w - t, y + 2 * t);
    p.lineTo (x + w, y + 2 * t);         p.lineTo (x + w, y + h - 2 * t);
    p.lineTo (x + w - t, y + h - 2 * t); p.lineTo (x + w - t, y + h - t);
    p.lineTo (x + w - 2 * t, y + h - t); p.lineTo (x + w - 2 * t, y + h);
    p.lineTo (x + 2 * t, y + h);         p.lineTo (x + 2 * t, y + h - t);
    p.lineTo (x + t, y + h - t);         p.lineTo (x + t, y + h - 2 * t);
    p.lineTo (x, y + h - 2 * t);         p.lineTo (x, y + 2 * t);
    p.lineTo (x + t, y + 2 * t);         p.lineTo (x + t, y + t);
    p.lineTo (x + 2 * t, y + t);
    p.closeSubPath();
    return p;
}

// plate with stepped (cut) corners, double frame, hard shadow
void pixelBox (Graphics& g, Rectangle<int> r, Colour frame, Colour fill, int step = 3)
{
    g.setColour (pal::ink.withAlpha (0.55f));
    g.fillPath (steppedPath (r.translated (4, 4), step));
    g.setColour (frame);            g.fillPath (steppedPath (r, step));
    g.setColour (fill);             g.fillPath (steppedPath (r.reduced (3), step));
    g.setColour (frame);            g.fillPath (steppedPath (r.reduced (5), step));
    g.setColour (fill);             g.fillPath (steppedPath (r.reduced (7), step));
}

Colour mixc (Colour a, Colour b, float t) { return a.interpolatedWith (b, juce::jlimit (0.f, 1.f, t)); }

// banded gradient: 'band' px stripes, quantised to 'steps' levels
void bandedGradient (Graphics& g, Rectangle<int> r, std::initializer_list<std::pair<float, Colour>> stops, int band, int steps)
{
    std::vector<std::pair<float, Colour>> st (stops);
    for (int y = 0; y < r.getHeight(); y += band)
    {
        float t = (float) y / (float) std::max (1, r.getHeight() - band);
        t = std::floor (t * (float) steps) / (float) (steps - 1);
        t = juce::jlimit (0.f, 1.f, t);
        Colour c = st.back().second;
        for (size_t i = 1; i < st.size(); ++i)
            if (t <= st[i].first) { c = mixc (st[i - 1].second, st[i].second, (t - st[i - 1].first) / (st[i].first - st[i - 1].first)); break; }
        g.setColour (c);
        g.fillRect (r.getX(), r.getY() + y, r.getWidth(), std::min (band, r.getHeight() - y));
    }
}
} // namespace

// ------------------------------------------------------------------ look and feel
PixelLookAndFeel::PixelLookAndFeel()
{
    setColour (juce::Slider::backgroundColourId, pal::track);
}

void PixelLookAndFeel::drawLinearSlider (Graphics& g, int x, int y, int w, int h, float, float, float,
                                         juce::Slider::SliderStyle, juce::Slider& s)
{
    const int n = std::max (1, (w + 2) / 8);
    const float prop = (float) s.valueToProportionOfLength (s.getValue());
    const int filled = juce::jlimit (0, n, (int) std::lround (prop * (float) n));
    const int cy = y + h / 2, top = cy - 7;
    for (int i = 0; i < n; ++i)
    {
        const int sx = x + i * 8;
        if (i < filled)
        {
            const float t = n > 1 ? (float) i / (float) (n - 1) : 0.f;
            const Colour c = t < 0.5f ? mixc (pal::blood, pal::violet, t * 2.f) : mixc (pal::violet, pal::starBlue, (t - 0.5f) * 2.f);
            g.setColour (c);
            g.fillRect (sx, top, 6, 14);
            g.setColour (c.brighter (0.55f));
            g.fillRect (sx, top, 6, 2);
        }
        else
        {
            g.setColour (pal::track);
            g.fillRect (sx, top, 6, 14);
        }
    }
    const int mi = std::max (0, filled - 1);
    const Rectangle<int> m (x + mi * 8 - 2, cy - 9, 10, 18);
    g.setColour (pal::ink);  g.fillRect (m);
    g.setColour (pal::moon); g.fillRect (m.reduced (2));
}

// ------------------------------------------------------------------ ParamRow
ParamRow::ParamRow (juce::AudioProcessorValueTreeState& apvts, const char* id, const juce::String& lab)
    : label (lab)
{
    param = apvts.getParameter (id);
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider.setLookAndFeel (nullptr);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, slider);
    setBufferedToImage (true);
}
void ParamRow::resized() { slider.setBounds (102, 0, getWidth() - 102 - 82, getHeight()); }
void ParamRow::paint (Graphics& g)
{
    g.setFont (pixelfont::vhs (25.f));
    g.setColour (pal::text);
    g.drawText (label, 0, 0, 100, getHeight(), juce::Justification::centredLeft, false);
    g.setColour (pal::textSoft);
    g.drawText (param->getText (param->getValue(), 24), getWidth() - 80, 0, 80, getHeight(), juce::Justification::centredRight, false);
}

// ------------------------------------------------------------------ Editor
int Editor::addPanel (const juce::String& title, Colour c, int x, int y,
                      std::initializer_list<std::pair<const char*, const char*>> list)
{
    const int h = 48 + 30 * (int) list.size();
    panels.push_back ({ title, c, { x, y, 370, h } });
    int ry = y + 42;
    for (auto& kv : list)
    {
        auto row = std::make_unique<ParamRow> (proc.apvts, kv.first, kv.second);
        row->slider.setLookAndFeel (&laf);
        row->setBounds (x + 10, ry, 350, 30);
        addAndMakeVisible (*row);
        rows.push_back (std::move (row));
        ry += 30;
    }
    return y + h + 14;
}

Editor::Editor (PepelProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    using namespace pid;
    const int top = 126;
    int y = addPanel ("AUTO-TUNE", pal::blood, 20, top,
                      { { atKey, "Key" }, { atScale, "Scale" }, { atSpeed, "Speed" }, { atHuman, "Humanize" },
                        { atAmount, "Strength" }, { atLock, "Syn-Lock" }, { atLockMode, "Lock Mode" } });
    y = addPanel ("GRANULAR", pal::blue, 20, y, { { grSize, "Size" }, { grDens, "Density" }, { grPitch, "Pitch" }, { grScat, "Scatter" }, { grMix, "Mix" } });
    addPanel ("BITCRUSHER", pal::violet, 20, y, { { bcBits, "Bits" }, { bcRate, "Downsmp" }, { bcMix, "Mix" } });

    y = addPanel ("PITCH + HARMONY", pal::blue, 410, top,
                  { { psShift, "Shift" }, { psFine, "Fine" }, { psMix, "Pitch Mix" }, { hmV1, "Voice 1" }, { hmV2, "Voice 2" }, { hmMix, "Harm Mix" }, { hmSpread, "Spread" } });
    y = addPanel ("PHASER", pal::violet, 410, y, { { phRate, "Rate" }, { phDepth, "Depth" }, { phFeed, "Feedback" }, { phMix, "Mix" } });
    addPanel ("FLANGER", pal::coral, 410, y, { { flRate, "Rate" }, { flDepth, "Depth" }, { flFeed, "Feedback" }, { flMix, "Mix" } });

    y = addPanel ("MASTER", pal::blue, 800, top, { { inGain, "Input" }, { outGain, "Output" }, { mix, "Dry/Wet" } });
    y = addPanel ("DISTORTION", pal::blood, 800, y, { { diDrive, "Drive" }, { diTone, "Tone" }, { diMix, "Mix" } });
    y = addPanel ("EXCITER", pal::coral, 800, y, { { exFreq, "Freq" }, { exDrive, "Drive" }, { exMix, "Mix" } });
    addPanel ("TREMOLO", pal::violet, 800, y, { { trRate, "Rate" }, { trDepth, "Depth" }, { trShape, "Shape" } });

    // sky: stars, clouds
    for (int i = 0; i < 46; ++i) stars.push_back ({ rng.nextInt ({ 10, W - 10 }), rng.nextInt ({ 6, 104 }), i % 9 == 0 ? 1 : 0, rng.nextInt (12) });
    for (int i = 0; i < 5; ++i) hdrClouds.push_back ({ (float) rng.nextInt (W), (float) (48 + rng.nextInt (56)), 0.35f + rng.nextFloat() * 0.5f, i % 3 });
    for (int i = 0; i < 8; ++i) wxClouds.push_back ({ (float) rng.nextInt (1150), (float) (4 + rng.nextInt (34)), 0.4f + rng.nextFloat() * 0.7f, i % 3 });
    for (int i = 0; i < 110; ++i) drops.push_back ({ (float) rng.nextInt (1150), (float) rng.nextInt (96) });
    for (auto& b : bars) b = rng.nextInt (100);

    setSize (W, H);
    setOpaque (true);
    buildStatic();
    startTimerHz (15); // deliberately choppy retro frame rate
}
Editor::~Editor()
{
    for (auto& r : rows) r->slider.setLookAndFeel (nullptr);
}

void Editor::buildStatic()
{
    staticLayer = juce::Image (juce::Image::ARGB, W, H, true);
    Graphics g (staticLayer);
    bandedGradient (g, { 0, 0, W, H }, { { 0.f, pal::ink }, { 0.30f, Colour (0xff1E2181) }, { 0.68f, pal::maroon }, { 1.f, pal::crimson } }, 6, 14);
    for (auto& p : panels)
    {
        pixelBox (g, p.r, p.col, pal::paper);
        const Rectangle<int> head (p.r.getX() + 7, p.r.getY() + 7, p.r.getWidth() - 14, 26);
        g.setColour (p.col.withAlpha (0.45f));
        g.fillRect (head);
        static const SpritePal heartPal = mono (pal::moon);
        drawSprite (g, sprites::heart, head.getX() + 9, head.getY() + 7, 2, mono (pal::ink.withAlpha (0.6f)));
        drawSprite (g, sprites::heart, head.getX() + 7, head.getY() + 5, 2, heartPal);
        g.setFont (pixelfont::title (13.f));
        g.setColour (pal::ink.withAlpha (0.7f));
        g.drawText (p.title, head.withTrimmedLeft (34).translated (1, 1), juce::Justification::centredLeft, false);
        g.setColour (pal::text);
        g.drawText (p.title, head.withTrimmedLeft (34), juce::Justification::centredLeft, false);
    }
    // sky gradient for header
    skyImage = juce::Image (juce::Image::ARGB, W, 118, true);
    {
        Graphics s (skyImage);
        bandedGradient (s, { 0, 0, W, 112 }, { { 0.f, Colour (0xff00003E) }, { 0.6f, pal::night }, { 1.f, Colour (0xff2A2490) } }, 6, 14);
        s.setColour (pal::blood);   s.fillRect (0, 112, W, 3);
        s.setColour (pal::ink);     s.fillRect (0, 115, W, 3);
    }
    // scanlines: every 3rd pixel row
    scanlines = juce::Image (juce::Image::ARGB, W, H, true);
    {
        Graphics s (scanlines);
        s.setColour (pal::ink.withAlpha (0.16f));
        for (int y = 0; y < H; y += 3) s.fillRect (0, y, W, 1);
    }
}

void Editor::nextPreset (int d)
{
    const int n = proc.getNumPrograms();
    proc.setCurrentProgram (((proc.getCurrentProgram() + d) % n + n) % n);
    repaint();
}
void Editor::mouseDown (const juce::MouseEvent& e)
{
    if (! presetRect.contains (e.getPosition())) return;
    if (e.x < presetRect.getX() + 44) nextPreset (-1);
    else nextPreset (1);
}

void Editor::advance (int frames)
{
    for (int f = 0; f < frames; ++f)
    {
        ++frame;
        dirtS += 0.10f * (proc.dirt.load() - dirtS);
        levelS += 0.5f * (proc.inputLevel.load() - levelS);
        for (auto& c : hdrClouds) { c.x -= c.speed; if (c.x < -60 * (c.size + 1)) c.x = (float) W + 20; }
        for (auto& c : wxClouds) { c.x -= c.speed * (1.f + dirtS * 2.f); if (c.x < -220) c.x = 1150.f + 10; }
        for (auto& c : comets) { c.x -= 16.f; c.y += 5.f; --c.life; }
        comets.erase (std::remove_if (comets.begin(), comets.end(), [] (const Comet& c) { return c.life <= 0 || c.y > 110.f || c.x < -30.f; }), comets.end());
        if (comets.empty() && rng.nextInt (80) == 0) comets.push_back ({ (float) rng.nextInt ({ 500, W - 40 }), (float) rng.nextInt ({ 2, 30 }), 40 });
        for (auto& h : hearts) { h.y -= 3.f; h.x += (float) (rng.nextInt (3) - 1); --h.life; }
        hearts.erase (std::remove_if (hearts.begin(), hearts.end(), [] (const HeartP& h) { return h.life <= 0; }), hearts.end());
        if (levelS > 0.08f && frame % 5 == 0) hearts.push_back ({ (float) (W - 120 + rng.nextInt (60)), 84.f, 14 });
        const float rainSpeed = 10.f + 8.f * dirtS;
        for (auto& d : drops) { d.y += rainSpeed; d.x -= 3.f; if (d.y > 96.f) { d.y = (float) -rng.nextInt (30); d.x = (float) rng.nextInt (1160); } }
        // VHS glitch roughly every 4 s for 2-4 frames
        if (glitchLeft > 0) --glitchLeft;
        else if (--glitchTimer <= 0)
        {
            glitchLeft = 2 + rng.nextInt (3);
            glitchY = rng.nextInt (H - 40); glitchH = 10 + rng.nextInt (22);
            glitchTimer = 52 + rng.nextInt (16);
        }
        // lightning
        if (flash > 0) --flash;
        else if (dirtS > 0.8f && --flashTimer <= 0) { flash = 2; flashTimer = 20 + rng.nextInt (40); boltX = (float) rng.nextInt (1000) + 80.f; }
        for (int i = 0; i < 64; ++i) bars[i] = juce::jlimit (0, 100, bars[i] + rng.nextInt (41) - 20);
    }
}

// ------------------------------------------------------------------ painting
void Editor::drawPresetBox (Graphics& g)
{
    pixelBox (g, presetRect, pal::blood, pal::paper);
    const auto r = presetRect;
    g.setFont (pixelfont::title (13.f));
    g.setColour (pal::peach);
    g.drawText ("<", r.withWidth (40), juce::Justification::centred, false);
    g.drawText (">", r.withTrimmedLeft (r.getWidth() - 40), juce::Justification::centred, false);
    const int idx = proc.getCurrentProgram();
    g.setColour (pal::text);
    g.drawText (juce::String (idx + 1).paddedLeft ('0', 2) + " " + proc.getProgramName (idx), r.reduced (40, 0), juce::Justification::centred, false);
}

void Editor::drawHeader (Graphics& g)
{
    g.drawImageAt (skyImage, 0, 0);
    const bool gl = glitchLeft > 0;
    // stars
    static const SpritePal sparkPal = mono (pal::starBlue);
    for (auto& s : stars)
    {
        const int ph = (frame / 3 + s.phase) % 6;
        const float a = ph == 0 ? 0.25f : (ph < 3 ? 1.f : 0.6f);
        if (s.kind == 1)
            drawSprite (g, sprites::sparkle, s.x, s.y, 2, (s.x % 2) ? sparkPal : mono (pal::moon), a);
        else
        {
            g.setColour ((s.x % 3 == 0 ? Colour (0xff9BA77A) : pal::starBlue).withAlpha (a));
            g.fillRect (s.x, s.y, 2, 2);
        }
    }
    // crescent moon
    {
        SpritePal mp; mp.set ('#', pal::glow);
        drawSprite (g, sprites::moon, 756, 12, 4, mp, 0.35f);
        SpritePal mp2; mp2.set ('#', pal::moon);
        drawSprite (g, sprites::moon, 760, 16, 4, mp2);
    }
    // comets
    for (auto& c : comets)
        for (int k = 0; k < 9; ++k)
        {
            g.setColour (pal::moon.withAlpha (1.f - (float) k / 9.f));
            const int sz = k == 0 ? 4 : 3;
            g.fillRect ((int) (c.x + (float) k * 8.f), (int) (c.y - (float) k * 2.5f), sz, sz);
        }
    // clouds (night-sky purple)
    SpritePal cp; cp.set ('a', pal::indigo); cp.set ('b', pal::violetDk); cp.set ('c', pal::mist);
    for (auto& c : hdrClouds)
    {
        const int sx = (int) c.x, sy = (int) c.y;
        if (c.size == 0) drawSprite (g, sprites::cloudS, sx, sy, 3, cp, 0.75f);
        else if (c.size == 1) drawSprite (g, sprites::cloudM, sx, sy - 12, 3, cp, 0.75f);
        else drawSprite (g, sprites::cloudL, sx, sy - 24, 3, cp, 0.75f);
    }
    // kanji 灰 愛 (ash, love)
    SpritePal kp = mono (pal::moon), ks = mono (pal::ink.withAlpha (0.7f)), kr = mono (pal::blood);
    drawSprite (g, sprites::ash, 432, 28, 3, ks);   drawSprite (g, sprites::ash, 429, 25, 3, kp);
    drawSprite (g, sprites::love, 494, 28, 3, ks);  drawSprite (g, sprites::love, 491, 25, 3, kr);
    // title with chromatic aberration
    g.setFont (pixelfont::title (30.f));
    const int sh = gl ? 4 : 2;
    const Rectangle<int> tr (24, 16, 380, 44);
    g.setColour (Colour (0xff7FE7FF).withAlpha (0.9f)); g.drawText ("PEPEL VOX", tr.translated (-sh, 0), juce::Justification::centredLeft, false);
    g.setColour (Colour (0xffFF4A5A).withAlpha (0.9f)); g.drawText ("PEPEL VOX", tr.translated (sh, 0), juce::Justification::centredLeft, false);
    g.setColour (pal::text);                            g.drawText ("PEPEL VOX", tr, juce::Justification::centredLeft, false);
    drawPresetBox (g);
    // chibi
    SpritePal ch;
    ch.set ('o', pal::ink); ch.set ('h', pal::violetDk); ch.set ('H', pal::mist); ch.set ('s', Colour (0xffF6CDB5));
    ch.set ('b', pal::coral); ch.set ('e', pal::ink); ch.set ('w', pal::moon); ch.set ('E', pal::blood);
    ch.set ('r', pal::blood); ch.set ('R', pal::crimson); ch.set ('l', pal::moon);
    const int bob = (levelS > 0.06f && (frame / 3) % 2 == 0) ? -4 : 0;
    drawSprite (g, sprites::chibi, W - 20 - 88 - 6, 8 + bob + 4, 4, ch);
    for (auto& h : hearts) drawSprite (g, sprites::heart, (int) h.x, (int) h.y, 2, mono (pal::blood), (float) h.life / 14.f);
}

void Editor::drawWeather (Graphics& g, Rectangle<int> r)
{
    const float d = dirtS;
    // sky keyframes: pale blue day -> blue->red dawn -> dusk -> bruised -> blood storm
    struct K { float t; Colour top, bot; };
    static const K keys[] = {
        { 0.00f, Colour (0xff3F6FE0), Colour (0xff9CC8F8) },
        { 0.20f, Colour (0xff2E3BA8), Colour (0xffEE7A5A) },
        { 0.42f, Colour (0xff232284), Colour (0xff8A2A70) },
        { 0.62f, Colour (0xff1A1560), Colour (0xff8A1630) },
        { 0.85f, Colour (0xff730426), Colour (0xffC10F1D) } };
    Colour top = keys[4].top, bot = keys[4].bot;
    for (int i = 1; i < 5; ++i)
        if (d <= keys[i].t) { const float t = (d - keys[i - 1].t) / (keys[i].t - keys[i - 1].t); top = mixc (keys[i - 1].top, keys[i].top, t); bot = mixc (keys[i - 1].bot, keys[i].bot, t); break; }

    pixelBox (g, r, d > 0.55f ? pal::blood : pal::blue, pal::paper);
    const auto in = r.reduced (9);
    g.saveState();
    g.reduceClipRegion (in);
    for (int y = 0; y < in.getHeight(); y += 4)
    {
        float t = (float) y / (float) in.getHeight();
        t = std::floor (t * 14.f) / 13.f;
        g.setColour (mixc (top, bot, t));
        g.fillRect (in.getX(), in.getY() + y, in.getWidth(), 4);
    }
    const int ground = in.getBottom() - 22;

    // sun (clear -> dawn) and blood moon (storm)
    {
        const float sunA = 1.f - juce::jlimit (0.f, 1.f, (d - 0.28f) / 0.14f);
        if (sunA > 0.01f)
        {
            const float rise = juce::jlimit (0.f, 1.f, (d - 0.04f) / 0.17f);
            const int cx = in.getX() + 880, cy = (int) juce::jmap (rise, (float) in.getY() + 30.f, (float) ground + 4.f);
            const Colour sc = mixc (Colour (0xffFFF3CC), pal::blood, rise * 0.8f), sc2 = mixc (Colour (0xffFFFFFF), pal::peach, rise);
            for (int dy = -7; dy <= 7; ++dy)
                for (int dx = -7; dx <= 7; ++dx)
                    if (dx * dx + dy * dy <= 45)
                    {
                        g.setColour ((dx + dy < -2 ? sc2 : sc).withAlpha (sunA));
                        g.fillRect (cx + dx * 4, cy + dy * 4, 4, 4);
                    }
        }
        const float moonA = juce::jlimit (0.f, 1.f, (d - 0.60f) / 0.2f);
        if (moonA > 0.01f)
        {
            const int cx = in.getX() + 300, cy = in.getY() + 34;
            for (int dy = -8; dy <= 8; ++dy)
                for (int dx = -8; dx <= 8; ++dx)
                    if (dx * dx + dy * dy <= 66)
                    {
                        const int lit = dx + dy;
                        const Colour c = lit > 5 ? Colour (0xffEEA380) : (lit > 0 ? pal::coral : pal::blood);
                        g.setColour (c.withAlpha (moonA));
                        g.fillRect (cx + dx * 4, cy + dy * 4, 4, 4);
                    }
        }
    }
    // clouds: white/blue -> purple -> maroon as it gets dirtier
    {
        const Colour ca = mixc (mixc (Colour (0xffB4C8F4), pal::violetDk, d * 2.2f), pal::maroon, (d - 0.55f) * 2.5f);
        const Colour cb = mixc (mixc (Colour (0xffD6E2FF), pal::mist, d * 2.2f), pal::crimson, (d - 0.55f) * 2.5f);
        const Colour cc = mixc (mixc (Colour (0xffF4F8FF), pal::glow, d * 2.2f), pal::coral, (d - 0.55f) * 2.5f);
        SpritePal cp; cp.set ('a', ca); cp.set ('b', cb); cp.set ('c', cc);
        for (size_t i = 0; i < wxClouds.size(); ++i)
        {
            const float appear = juce::jlimit (0.f, 1.f, (d - 0.04f * (float) i) / 0.12f);
            if (appear <= 0.01f) continue;
            auto& c = wxClouds[i];
            const int sx = in.getX() + (int) c.x, sy = in.getY() + (int) c.y;
            if (c.size == 0) drawSprite (g, sprites::cloudS, sx, sy, 3, cp, 0.9f * appear);
            else if (c.size == 1) drawSprite (g, sprites::cloudM, sx, sy - 6, 3, cp, 0.9f * appear);
            else drawSprite (g, sprites::cloudL, sx, sy - 12, 3, cp, 0.9f * appear);
        }
    }
    // rain
    if (d > 0.45f)
    {
        const int cnt = (int) (juce::jlimit (0.f, 1.f, (d - 0.45f) / 0.4f) * (float) drops.size());
        g.setColour (mixc (Colour (0xff9FB4F0), pal::peach, (d - 0.7f) * 2.f).withAlpha (0.85f));
        for (int i = 0; i < cnt; ++i) g.fillRect (in.getX() + (int) drops[(size_t) i].x, in.getY() + (int) drops[(size_t) i].y, 2, 8);
    }
    // ground: a skyline of EQ bars that dance with the input level
    {
        const Colour gc = mixc (pal::night, pal::deepRed, d);
        g.setColour (gc);
        g.fillRect (in.getX(), ground + 12, in.getWidth(), 12);
        const int n = in.getWidth() / 18;
        for (int i = 0; i < n; ++i)
        {
            const int hgt = 6 + (int) ((float) bars[i % 64] * 0.08f * (0.3f + levelS * 3.f)) + (i * 7 % 5) * 2;
            g.setColour (mixc (gc, pal::ink, 0.35f + 0.1f * (float) (i % 3)));
            g.fillRect (in.getX() + i * 18 + 2, ground + 12 - hgt, 14, hgt);
            g.setColour (mixc (pal::peach, pal::moon, 0.5f).withAlpha (d < 0.5f ? 0.0f : 0.5f));
            if (hgt > 12) g.fillRect (in.getX() + i * 18 + 6, ground + 12 - hgt + 4, 2, 2);
        }
    }
    // lightning
    if (flash > 0)
    {
        g.setColour (pal::moon.withAlpha (0.38f));
        g.fillRect (in);
        SpritePal bp = mono (Colour (0xffFFF3CC));
        drawSprite (g, sprites::bolt, in.getX() + (int) boltX, in.getY() + 8, 6, bp);
    }
    g.restoreState();
}

void Editor::paint (Graphics& g)
{
    g.drawImageAt (staticLayer, 0, 0);
    drawHeader (g);
    drawWeather (g, weatherRect);
    // signature
    g.setFont (pixelfont::title (10.f));
    const auto sig = juce::String::fromUTF8 ("\xD0\x9F\xD0\x95\xD0\x9F\xD0\x95L");
    const Rectangle<int> sr (W - 110, H - 24, 96, 14);
    g.setColour (pal::ink.withAlpha (0.8f)); g.drawText (sig, sr.translated (1, 1), juce::Justification::centredRight, false);
    g.setColour (pal::text.withAlpha (0.85f)); g.drawText (sig, sr, juce::Justification::centredRight, false);
}

void Editor::paintOverChildren (Graphics& g)
{
    g.drawImageAt (scanlines, 0, 0);
    // film grain
    for (int i = 0; i < 24; ++i)
    {
        g.setColour ((i & 1 ? pal::moon : pal::ink).withAlpha (i & 1 ? 0.22f : 0.35f));
        g.fillRect (rng.nextInt (W), rng.nextInt (H), 2, 1);
    }
    // rare tape glitch: translucent white band with cyan / pink tears
    if (glitchLeft > 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.fillRect (0, glitchY, W, glitchH);
        g.setColour (Colour (0xff7FE7FF).withAlpha (0.35f));
        g.fillRect (rng.nextInt (W / 2), glitchY + 2, 120 + rng.nextInt (300), 3);
        g.setColour (Colour (0xffFF7FBF).withAlpha (0.35f));
        g.fillRect (rng.nextInt (W / 2), glitchY + glitchH - 5, 120 + rng.nextInt (300), 3);
    }
}
