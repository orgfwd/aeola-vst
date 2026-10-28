// Loads a built plug-in through JUCE's own VST3/AU hosting (like a DAW) and lists what the host sees:
// every parameter, and how many are automatable. Ableton Live fills its parameter list by itself only when the
// plug-in has at most 64 automatable parameters (Options.txt -_PluginAutoPopulateThreshold).
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { printf("usage: Vst3Probe <path to .vst3 or .component> [-v]\n"); return 2; }
    const juce::String path = juce::String::fromUTF8(argv[1]); const bool verbose = argc > 2;
    juce::AudioPluginFormatManager fm; fm.addDefaultFormats();
    juce::OwnedArray<juce::PluginDescription> found;
    for (auto* f : fm.getFormats()) if (f->fileMightContainThisPluginType(path)) f->findAllTypesForFile(found, path);
    if (found.isEmpty()) { printf("no plug-in found in %s\n", path.toRawUTF8()); return 1; }
    juce::String err;
    auto inst = fm.createPluginInstance(*found[0], 48000.0, 512, err);
    if (!inst) { printf("could not load: %s\n", err.toRawUTF8()); return 1; }
    int nAll = 0, nAuto = 0; juce::StringArray autos;
    for (auto* p : inst->getParameters())
    {
        ++nAll; const bool a = p->isAutomatable(); if (a) { ++nAuto; autos.add(p->getName(64)); }
        if (verbose) printf("%4d %s %s\n", p->getParameterIndex(), a ? "auto" : "    ", p->getName(64).toRawUTF8());
    }
    printf("format %s, version %s: %d parameters, %d automatable\n", found[0]->pluginFormatName.toRawUTF8(), found[0]->version.toRawUTF8(), nAll, nAuto);
    printf("automatable: %s\n", autos.joinIntoString(", ").toRawUTF8());
    printf("programs: %d\n", inst->getNumPrograms());
    return 0;
}
