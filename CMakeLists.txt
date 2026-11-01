cmake_minimum_required(VERSION 3.22)

project(Veneer VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Local dev: -DFETCHCONTENT_SOURCE_DIR_JUCE=$HOME/JUCE reuses an existing checkout.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.10
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(VENEER_FORMATS VST3 AU Standalone)
else()
    set(VENEER_FORMATS VST3 Standalone)
endif()

juce_add_plugin(Veneer
    COMPANY_NAME "Veneer Project"
    BUNDLE_ID io.github.umatt1.veneer
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Vnrp
    PLUGIN_CODE Vnr1
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Dynamics
    FORMATS ${VENEER_FORMATS}
    PRODUCT_NAME "Veneer")

set(VENEER_DSP_SOURCES
    Source/dsp/BandCompressor.cpp
    Source/dsp/Crossover.cpp
    Source/dsp/Engine.cpp
    Source/dsp/Limiter.cpp
    Source/dsp/Loudness.cpp
    Source/dsp/TruePeak.cpp
    Source/dsp/Voicing.cpp)

target_sources(Veneer PRIVATE
    Source/PluginProcessor.cpp
    ${VENEER_DSP_SOURCES})

target_include_directories(Veneer PRIVATE Source)

target_compile_definitions(Veneer PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Veneer
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

if(APPLE)
    set(VENEER_CODESIGN_IDENTITY "-" CACHE STRING "codesign identity for built bundles (- is ad-hoc)")

    foreach(fmt IN LISTS VENEER_FORMATS)
        add_custom_command(TARGET Veneer_${fmt} POST_BUILD
            COMMAND codesign --force --deep --sign "${VENEER_CODESIGN_IDENTITY}"
                    "$<GENEX_EVAL:$<TARGET_PROPERTY:Veneer_${fmt},JUCE_PLUGIN_ARTEFACT_FILE>>"
            VERBATIM)
    endforeach()
endif()

option(VENEER_BUILD_TOOLS "Build the offline render tool" OFF)

if(VENEER_BUILD_TOOLS)
    add_executable(VeneerRender Tools/Render.cpp ${VENEER_DSP_SOURCES})

    target_include_directories(VeneerRender PRIVATE Source)

    target_compile_definitions(VeneerRender PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)

    target_link_libraries(VeneerRender PRIVATE
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()

option(VENEER_BUILD_TESTS "Build the unit tests" OFF)

if(VENEER_BUILD_TESTS)
    enable_testing()

    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)

    add_executable(VeneerTests
        Tests/CompressorTests.cpp
        Tests/CrossoverTests.cpp
        Tests/EngineTests.cpp
        Tests/LimiterTests.cpp
        Tests/LoudnessTests.cpp
        Tests/PerfTests.cpp
        Tests/ProcessorTests.cpp
        Tests/TruePeakTests.cpp
        Tests/WindowedTests.cpp
        Source/PluginProcessor.cpp
        ${VENEER_DSP_SOURCES})

    target_include_directories(VeneerTests PRIVATE Source Tests)

    target_compile_definitions(VeneerTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="Veneer")

    target_link_libraries(VeneerTests PRIVATE
        Catch2::Catch2WithMain
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

    catch_discover_tests(VeneerTests)
endif()
