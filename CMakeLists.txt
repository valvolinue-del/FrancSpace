cmake_minimum_required(VERSION 3.22)

project(FrancSpace
    VERSION 1.0.0
    DESCRIPTION "FrancSpace sidechain spectral carve audio plugin"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Optional local JUCE checkout. When empty, JUCE 8 is fetched from GitHub.
set(FRANCSPACE_JUCE_PATH "" CACHE PATH
    "Path to a local JUCE checkout. When set, it is added with add_subdirectory instead of FetchContent.")

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_WEB_BROWSER OFF CACHE BOOL "Disable the JUCE web browser (and its Linux webkit dependency)" FORCE)
set(JUCE_USE_CURL OFF CACHE BOOL "Disable JUCE curl support" FORCE)

if(FRANCSPACE_JUCE_PATH)
    message(STATUS "FrancSpace: using local JUCE from ${FRANCSPACE_JUCE_PATH}")
    add_subdirectory("${FRANCSPACE_JUCE_PATH}" "${CMAKE_BINARY_DIR}/juce-local")
else()
    message(STATUS "FrancSpace: fetching JUCE 8.0.15")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(FRANCSPACE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/SpaceDisplay.cpp
    Source/SpaceDisplay.h
    Source/SpectralEngine.cpp
    Source/SpectralEngine.h
    Source/Palette.h)

set(FRANCSPACE_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FRANCSPACE_FORMATS AU)
endif()

juce_add_plugin(FrancSpace
    COMPANY_NAME "Franc Audio"
    BUNDLE_ID com.francaudio.francspace
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Frnc
    PLUGIN_CODE FrSp
    FORMATS ${FRANCSPACE_FORMATS}
    PRODUCT_NAME "FrancSpace"
    VST3_CATEGORIES "Fx" "Dynamics" "EQ" "Analyzer"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Icon.png")

target_sources(FrancSpace PRIVATE ${FRANCSPACE_SOURCES})

target_compile_features(FrancSpace PRIVATE cxx_std_20)
set_target_properties(FrancSpace PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS OFF)

target_compile_definitions(FrancSpace
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

target_link_libraries(FrancSpace
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(FRANCSPACE_BUILD_TESTS "Build the offline FrancSpace sanity test" ON)

if(FRANCSPACE_BUILD_TESTS)
    juce_add_console_app(FrancSpaceSanity PRODUCT_NAME "FrancSpaceSanity")
    target_sources(FrancSpaceSanity
        PRIVATE
            tests/Sanity.cpp
            ${FRANCSPACE_SOURCES})
    target_include_directories(FrancSpaceSanity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_features(FrancSpaceSanity PRIVATE cxx_std_20)
    target_compile_definitions(FrancSpaceSanity
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(FrancSpaceSanity
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
