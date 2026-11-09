#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

// ------------------------------------------------------------------ palette: night sky + blood
namespace pal
{
inline const juce::Colour ink       { 0xff040533 }; // outlines, shadows, scanlines (deepest night)
inline const juce::Colour night     { 0xff08095B };
inline const juce::Colour indigo    { 0xff232284 };
inline const juce::Colour violetDk  { 0xff362A8E };
inline const juce::Colour mist      { 0xff60579E };
inline const juce::Colour glow      { 0xffB7A6F0 };
inline const juce::Colour moon      { 0xffEDE4FF };
inline const juce::Colour starBlue  { 0xff4F8FD0 };
inline const juce::Colour blue      { 0xff4F72E6 }; // panel colour: blue
inline const juce::Colour violet    { 0xff7F6FE0 }; // panel colour: violet
inline const juce::Colour blood     { 0xffE93426 }; // panel colour: blood
inline const juce::Colour coral     { 0xffE8634A }; // panel colour: coral
inline const juce::Colour crimson   { 0xffC10F1D };
inline const juce::Colour maroon    { 0xff730426 };
inline const juce::Colour deepRed   { 0xff530121 };
inline const juce::Colour peach     { 0xffEEA380 };
inline const juce::Colour paper     { 0xff0B0C46 }; // panel fill
inline const juce::Colour track     { 0xff1D1F78 }; // empty slider segment
inline const juce::Colour text      { 0xffE9E4FF }; // main text
inline const juce::Colour textSoft  { 0xffEEA380 }; // values
}

namespace pixelfont
{
juce::Font vhs (float height);   // VT323
juce::Font title (float height); // Press Start 2P
}

class PixelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PixelLookAndFeel();
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

// one parameter line: name | segmented slider | value
class ParamRow : public juce::Component
{
public:
    ParamRow (juce::AudioProcessorValueTreeState&, const char* paramId, const juce::String& label);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;

private:
    juce::RangedAudioParameter* param = nullptr;
    juce::String label;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

class Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int W = 1190, H = 864;
    explicit Editor (PepelProcessor&);
    ~Editor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void advance (int frames); // one animation frame = 1/15 s (also used by the screenshot tool)

private:
    struct Panel { juce::String title; juce::Colour col; juce::Rectangle<int> r; };
    struct Star { int x, y; int kind; int phase; };
    struct Cloud { float x, y, speed; int size; };
    struct Comet { float x, y; int life; };
    struct HeartP { float x, y; int life; };

    void timerCallback() override { advance (1); repaint(); }
    int addPanel (const juce::String& title, juce::Colour c, int x, int y,
                  std::initializer_list<std::pair<const char*, const char*>> rows);
    void buildStatic();
    void drawHeader (juce::Graphics&);
    void drawWeather (juce::Graphics&, juce::Rectangle<int> area);
    void drawPresetBox (juce::Graphics&);
    void nextPreset (int delta);

    PepelProcessor& proc;
    PixelLookAndFeel laf;
    std::vector<std::unique_ptr<ParamRow>> rows;
    std::vector<Panel> panels;
    juce::Image staticLayer, skyImage, scanlines;
    juce::Rectangle<int> weatherRect { 20, 762, 1150, 96 }, presetRect { 24, 70, 380, 34 };

    std::vector<Star> stars;
    std::vector<Cloud> hdrClouds, wxClouds;
    std::vector<Comet> comets;
    std::vector<HeartP> hearts;
    struct Drop { float x, y; };
    std::vector<Drop> drops;

    juce::Random rng { 20260 };
    int frame = 0, glitchLeft = 0, glitchTimer = 40, glitchY = 30, glitchH = 14, flash = 0, flashTimer = 40;
    float dirtS = 0.f, levelS = 0.f, boltX = 0.f;
    int bars[64] {};
};
