cmake_minimum_required(VERSION 3.22)

project(PRYSM VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(PRYSM_BUILD_TESTS "Build the unit/integration tests" ON)
option(PRYSM_ENABLE_LTO  "Link-time optimisation (slower builds, release only)" ON)
# JUCE exposes ~2000 hidden "MIDI CC" parameters so that hosts using VST3 IMidiMapping can deliver pitch wheel /
# sustain pedal / CCs. Turn OFF only if a host lists them in its automation menu AND still delivers MIDI events.
option(PRYSM_VST3_MIDI_CC_PARAMS "Expose JUCE's emulated MIDI CC parameters (VST3 IMidiMapping)" ON)

# Static MSVC runtime: the plug-in must not require the Visual C++ Redistributable on the user's PC.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE 8.0.15 (commit 91ad83ae34a81e0833b1a2b0866f54846370ae53), pinned.
# Uses third_party/JUCE when present (source package / CI); otherwise fetches the exact same commit.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/CMakeLists.txt")
    add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        91ad83ae34a81e0833b1a2b0866f54846370ae53)
    FetchContent_MakeAvailable(juce)
endif()

#------------------------------------------------------------------------------
# prysm_core: engine + processor + editor (shared by the plugin and the tests)
#------------------------------------------------------------------------------
# Embedded fonts (SIL Open Font License, see assets/fonts and docs/DEPENDENCIES.md)
juce_add_binary_data(PrysmAssets
    SOURCES
        assets/fonts/Rajdhani-SemiBold.ttf
        assets/fonts/ShareTechMono-Regular.ttf
        assets/fonts/Orbitron.ttf)
set_target_properties(PrysmAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

# INTERFACE library (documented JUCE "shared code" pattern): each final target compiles these
# sources together with the JUCE modules once, which avoids duplicate module symbols.
add_library(prysm_core INTERFACE)

target_sources(prysm_core INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/Engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginEditor.cpp)

target_include_directories(prysm_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Source)

target_compile_definitions(prysm_core INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(prysm_core
    INTERFACE
        PrysmAssets
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Intentional exact float comparisons (change detection) and JUCE's double processBlock overload.
target_compile_options(prysm_core INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wno-float-equal -Wno-overloaded-virtual -Wno-switch-enum>)

if(PRYSM_ENABLE_LTO)
    target_link_libraries(prysm_core INTERFACE juce::juce_recommended_lto_flags)
endif()

#------------------------------------------------------------------------------
# The VST3 plugin
#------------------------------------------------------------------------------
juce_add_plugin(PRYSM
    COMPANY_NAME "PRYSM Audio"
    BUNDLE_ID com.prysmaudio.prysm
    PLUGIN_MANUFACTURER_CODE Prsm
    PLUGIN_CODE Prys
    FORMATS VST3
    PRODUCT_NAME "PRYSM"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth Sampler
    VERSION ${PROJECT_VERSION})

target_link_libraries(PRYSM PRIVATE prysm_core)
if(NOT PRYSM_VST3_MIDI_CC_PARAMS)
    target_compile_definitions(PRYSM PRIVATE JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0)
endif()

if(PRYSM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()
