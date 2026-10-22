cmake_minimum_required(VERSION 3.22)
project(Tuman VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(TUMAN_FORMATS VST3 AU Standalone)
else()
    set(TUMAN_FORMATS VST3)
endif()

juce_add_plugin(Tuman
    COMPANY_NAME "Timmyjin"
    BUNDLE_ID "com.timmyjin.tuman"
    PLUGIN_MANUFACTURER_CODE Tmjn
    PLUGIN_CODE Tumn
    FORMATS ${TUMAN_FORMATS}
    PRODUCT_NAME "Tuman"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb Delay
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Tuman PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

juce_add_binary_data(TumanAssets SOURCES
    assets/fonts/SpaceGrotesk-Bold.ttf
    assets/fonts/SpaceMono-Regular.ttf
    assets/fonts/SpaceMono-Bold.ttf)

target_include_directories(Tuman PRIVATE src)

target_compile_definitions(Tuman PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(NOT APPLE)
    target_compile_definitions(Tuman PUBLIC JUCE_ALSA=0 JUCE_JACK=0)
endif()

# keep float arithmetic in program order (no fused multiply-add) like the original hardware
if(MSVC)
    target_compile_options(Tuman PRIVATE /fp:precise)
else()
    target_compile_options(Tuman PRIVATE -ffp-contract=off)
endif()

target_link_libraries(Tuman
    PRIVATE
        TumanAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline renderers (pure C++, no JUCE)
add_executable(tuman_render tools/render_demo.cpp)
target_compile_options(tuman_render PRIVATE -ffp-contract=off)
add_executable(tuman_ref tools/ref_render.cpp)
target_compile_options(tuman_ref PRIVATE -ffp-contract=off)

option(TUMAN_BUILD_SNAPSHOT "Build the UI snapshot tool (dev only)" OFF)
if(TUMAN_BUILD_SNAPSHOT)
    juce_add_console_app(tuman_snapshot PRODUCT_NAME "tuman_snapshot")
    target_sources(tuman_snapshot PRIVATE tools/snapshot.cpp src/PluginProcessor.cpp src/PluginEditor.cpp)
    target_include_directories(tuman_snapshot PRIVATE src)
    target_compile_definitions(tuman_snapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_compile_options(tuman_snapshot PRIVATE -ffp-contract=off)
    target_link_libraries(tuman_snapshot PRIVATE TumanAssets juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
