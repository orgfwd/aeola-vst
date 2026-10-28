cmake_minimum_required(VERSION 3.22)
# deployment target and architectures must be set before project() to take effect
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
project(AeolaDrone VERSION 1.13.1)
set(CMAKE_CXX_STANDARD 17)

add_subdirectory(../JUCE JUCE)

juce_add_plugin(AeolaDrone
    COMPANY_NAME "Alex Skepp"
    PLUGIN_MANUFACTURER_CODE Askp
    PLUGIN_CODE Aeo1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Aeola"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    BUNDLE_ID com.alexskepp.aeoladrone
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon-1024.png")

target_sources(AeolaDrone PRIVATE
    Source/PluginProcessor.cpp Source/PluginEditor.cpp
    Source/PluginProcessor.h Source/PluginEditor.h Source/DroneEngine.h Source/Tuning.h Source/Sky.h Source/SkyMapper.h
    Source/SkyFeed.h Source/Params.h Source/Presets.h Source/MidiLog.h)
target_compile_definitions(AeolaDrone PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0 JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(AeolaDrone PRIVATE
    juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics
    juce::juce_recommended_config_flags)

# --- headless tests that exercise the real plugin code (no audio device) ---
juce_add_console_app(AeolaPluginTest PRODUCT_NAME "AeolaPluginTest")
target_sources(AeolaPluginTest PRIVATE tests/plugin_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(AeolaPluginTest PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1
    AEOLA_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    JucePlugin_Name="Aeola" JucePlugin_Manufacturer="Alex Skepp" JucePlugin_ManufacturerCode=0x4173706b JucePlugin_PluginCode=0x41656f31
    JucePlugin_ProducesMidiOutput=0 JucePlugin_WantsMidiInput=1 JucePlugin_IsSynth=1 JucePlugin_IsMidiEffect=0 JucePlugin_EditorRequiresKeyboardFocus=0)
target_link_libraries(AeolaPluginTest PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp juce::juce_gui_basics juce::juce_recommended_config_flags)

# Host-side check (tools/vst3probe.cpp): what a DAW sees when it loads the built plug-in
juce_add_console_app(Vst3Probe PRODUCT_NAME "Vst3Probe")
target_sources(Vst3Probe PRIVATE tools/vst3probe.cpp)
target_compile_definitions(Vst3Probe PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_PLUGINHOST_AU=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(Vst3Probe PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags "-framework CoreAudioKit")
