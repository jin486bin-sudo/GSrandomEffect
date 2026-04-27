cmake_minimum_required(VERSION 3.22)
project(GSrandomEffect VERSION 1.0.0)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(JUCE)

juce_add_plugin(GSrandomEffect
    COMPANY_NAME "GS"
    BUNDLE_ID "com.gs.gsrandomeffect"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Gsre
    PLUGIN_CODE Grfx
    FORMATS VST3
    PRODUCT_NAME "GSrandom effect")

target_sources(GSrandomEffect PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/GlitchEngine.cpp)

target_compile_definitions(GSrandomEffect PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GSrandomEffect PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# MSVC: treat source as UTF-8 to avoid encoding issues
if(MSVC)
    target_compile_options(GSrandomEffect PRIVATE /utf-8)
endif()
