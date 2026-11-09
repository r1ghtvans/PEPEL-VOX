cmake_minimum_required(VERSION 3.22)
project(PepelVox VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(PEPEL_BUILD_TOOLS "Build DSP test and UI screenshot tool" OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.6
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

set(PEPEL_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PEPEL_FORMATS AU)
endif()

juce_add_plugin(PepelVox
    COMPANY_NAME "Pepel"
    PLUGIN_MANUFACTURER_CODE Pepl
    PLUGIN_CODE Pvox
    FORMATS ${PEPEL_FORMATS}
    PRODUCT_NAME "Pepel Vox"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

# Fonts (OFL) are embedded into the binary. Licences are shipped in Assets/.
juce_add_binary_data(PepelAssets SOURCES
    Assets/VT323-Regular.ttf
    Assets/PressStart2P-Regular.ttf)

target_sources(PepelVox PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Dsp.h
    Source/Params.h
    Source/Sprites.h)

target_compile_definitions(PepelVox PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(PepelVox
    PRIVATE
        PepelAssets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if(MSVC)
    target_compile_options(PepelVox PRIVATE /utf-8)
endif()

# ---------------------------------------------------------------- optional tools (DSP test + UI screenshot without a DAW)
if(PEPEL_BUILD_TOOLS)
    add_executable(PepelDspTest Tests/DspTest.cpp)
    target_include_directories(PepelDspTest PRIVATE Source)

    juce_add_console_app(PepelScreenshot PRODUCT_NAME "PepelScreenshot")
    target_sources(PepelScreenshot PRIVATE
        Tests/Screenshot.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(PepelScreenshot PRIVATE Source)
    target_compile_definitions(PepelScreenshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(PepelScreenshot PRIVATE
        PepelAssets juce::juce_audio_processors juce::juce_gui_basics
        juce::juce_recommended_config_flags)
endif()
