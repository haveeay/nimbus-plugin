cmake_minimum_required(VERSION 3.22)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" NIMBUS_VERSION)
project(Nimbus VERSION ${NIMBUS_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE is fetched automatically (or point FETCHCONTENT_SOURCE_DIR_JUCE at a local copy)
include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
  set(NIMBUS_FORMATS AU VST3 Standalone)
else()
  set(NIMBUS_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Nimbus
  COMPANY_NAME "Javi"
  BUNDLE_ID "com.javi.nimbus"
  PLUGIN_MANUFACTURER_CODE Javi
  PLUGIN_CODE Nmbs
  FORMATS ${NIMBUS_FORMATS}
  PRODUCT_NAME "Nimbus"
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  AU_MAIN_TYPE kAudioUnitType_MusicDevice
  VST3_CATEGORIES Instrument Synth
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(Nimbus PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp)

target_compile_definitions(Nimbus PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Nimbus
  PRIVATE
    juce::juce_audio_utils
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
