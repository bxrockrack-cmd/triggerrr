cmake_minimum_required(VERSION 3.22)
project(SnareTrigger VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

# AU собирается только на macOS
if(APPLE)
    set(PLUGIN_FORMATS VST3 AU)
else()
    set(PLUGIN_FORMATS VST3)
endif()

juce_add_plugin(SnareTrigger
    COMPANY_NAME "SnareTrigger"
    PLUGIN_MANUFACTURER_CODE Snrt
    PLUGIN_CODE Trg1
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "SnareTrigger"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(SnareTrigger PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SampleBank.cpp
    Source/SnareImageData.cpp)

# ---- Семплы из папки Samples/ вшиваются в плагин при сборке ----
file(GLOB SAMPLE_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Samples/*.wav"
    "${CMAKE_CURRENT_SOURCE_DIR}/Samples/*.WAV")
if(SAMPLE_FILES)
    list(REMOVE_DUPLICATES SAMPLE_FILES)
endif()

if(SAMPLE_FILES)
    juce_add_binary_data(SnareSamples SOURCES ${SAMPLE_FILES})
    set_target_properties(SnareSamples PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    target_link_libraries(SnareTrigger PRIVATE SnareSamples)
    target_compile_definitions(SnareTrigger PRIVATE SNARE_HAS_SAMPLES=1)
    list(LENGTH SAMPLE_FILES SAMPLE_COUNT)
    message(STATUS "SnareTrigger: вшиваю семплов: ${SAMPLE_COUNT}")
else()
    target_compile_definitions(SnareTrigger PRIVATE SNARE_HAS_SAMPLES=0)
    message(STATUS "SnareTrigger: папка Samples/ пуста — плагин соберётся без вшитых семплов")
endif()

target_compile_definitions(SnareTrigger PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

if(MSVC)
    target_compile_options(SnareTrigger PRIVATE /utf-8)
endif()

target_link_libraries(SnareTrigger
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
