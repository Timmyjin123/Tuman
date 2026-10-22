// Dev tool: renders the editor to PNG files (one per algorithm passed on the command line).
#include <juce_audio_processors/juce_audio_processors.h>
#include "../src/PluginEditor.h"

int main(int argc, char** argv) {
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String out = argc > 1 ? argv[1] : "tuman";
    const float scale = argc > 2 ? (float)std::atof(argv[2]) : 1.0f;
    TumanProcessor proc;
    proc.setPlayConfigDetails(2, 2, 48000, 256);
    proc.prepareToPlay(48000, 256);
    for (int alg : { 0, 9, 13 }) {
        if (auto* p = proc.apvts.getParameter("alg")) p->setValueNotifyingHost(p->convertTo0to1((float)alg));
        std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
        ed->setSize((int)(960 * scale), (int)(540 * scale));
        ed->resized();
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::File f(out + "_" + juce::String(alg) + ".png");
        f.deleteFile();
        juce::FileOutputStream os(f);
        juce::PNGImageFormat().writeImageToStream(img, os);
    }
    return 0;
}
