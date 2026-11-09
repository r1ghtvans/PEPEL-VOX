// UI screenshot without a DAW:  PepelScreenshot out.png [presetIndex] [dirt 0..1] [inputLevel 0..1]
#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    PepelProcessor proc;
    const juce::String out = argc > 1 ? argv[1] : "pepel.png";
    if (argc > 2) proc.setCurrentProgram (juce::String (argv[2]).getIntValue());
    const float dirt = argc > 3 ? juce::String (argv[3]).getFloatValue() : proc.dirt.load();
    const float lvl = argc > 4 ? juce::String (argv[4]).getFloatValue() : 0.3f;

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    proc.dirt.store (dirt);
    proc.inputLevel.store (lvl);
    if (auto* e = dynamic_cast<Editor*> (ed.get())) e->advance (90);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (out));
    f.deleteFile();
    juce::FileOutputStream fos (f);
    juce::PNGImageFormat png;
    if (fos.openedOk() && png.writeImageToStream (img, fos)) { std::printf ("wrote %s (%dx%d)\n", out.toRawUTF8(), img.getWidth(), img.getHeight()); return 0; }
    return 1;
}
