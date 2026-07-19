cmake_minimum_required(VERSION 3.22)
project(DeltaBlind VERSION 0.2.0)

add_subdirectory(libs/JUCE)

if(APPLE)
    set(DELTABLIND_FORMATS VST3 AU Standalone)
else()
    set(DELTABLIND_FORMATS VST3 Standalone)
endif()

juce_add_plugin(DeltaBlind
    COMPANY_NAME "Bumpin Audio"
    BUNDLE_ID "com.bumpinaudio.deltablind"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Nabs
    PLUGIN_CODE DBld
    FORMATS ${DELTABLIND_FORMATS}
    PRODUCT_NAME "Delta Blind")

target_sources(DeltaBlind PRIVATE
    Source/DeltaMatchProcessor.cpp
    Source/PluginEditor.cpp
    Source/PluginEntry.cpp)

target_compile_definitions(DeltaBlind PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DeltaBlind
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
