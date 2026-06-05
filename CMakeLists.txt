cmake_minimum_required(VERSION 3.22)

project(GuitarChannelizerMidiFx VERSION 0.1.0 LANGUAGES C CXX)

set(JUCE_DIR "" CACHE PATH "Path to a JUCE checkout or extracted JUCE folder")

if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE_DIR must point to a JUCE folder. Set it with: cmake -B build -S . -DJUCE_DIR=/path/to/JUCE")
endif()

add_subdirectory("${JUCE_DIR}" JUCE-build EXCLUDE_FROM_ALL)

juce_add_plugin(GuitarChannelizerMidiFx
    COMPANY_NAME "santismo"
    BUNDLE_ID "com.santismo.guitarizer"
    PLUGIN_MANUFACTURER_CODE "StTr"
    PLUGIN_CODE "GChz"
    FORMATS AU
    PRODUCT_NAME "Guitarizer"
    IS_SYNTH FALSE
    IS_MIDI_EFFECT TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    AU_MAIN_TYPE kAudioUnitType_MIDIProcessor
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_generate_juce_header(GuitarChannelizerMidiFx)

target_sources(GuitarChannelizerMidiFx
    PRIVATE
        Source/GuitarFingering.cpp
        Source/GuitarFingering.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h)

target_compile_features(GuitarChannelizerMidiFx PRIVATE cxx_std_17)

target_compile_definitions(GuitarChannelizerMidiFx
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

target_link_libraries(GuitarChannelizerMidiFx
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
