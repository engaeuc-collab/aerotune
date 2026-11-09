cmake_minimum_required(VERSION 3.22)
project(AeroTune VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(AEROTUNE_COPY_AFTER_BUILD "Copy the VST3 into the system plugin folder after building" OFF)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(AeroTune
    COMPANY_NAME                AeroAudio
    BUNDLE_ID                   com.aeroaudio.aerotune
    PLUGIN_MANUFACTURER_CODE    Aero
    PLUGIN_CODE                 Atun
    PRODUCT_NAME                "AeroTune"
    FORMATS                     VST3 Standalone
    VST3_CATEGORIES             Fx "Pitch Shift"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     ${AEROTUNE_COPY_AFTER_BUILD})

target_sources(AeroTune PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(AeroTune PRIVATE Source)

target_compile_definitions(AeroTune PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(AeroTune
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
