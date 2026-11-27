cmake_minimum_required(VERSION 3.22)
project(GSideChain VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Default: a JUCE 9 checkout in your home folder. Override with -DJUCE_DIR=<path>.
file(TO_CMAKE_PATH "$ENV{USERPROFILE}/JUCE" _gsc_default_juce)
set(JUCE_DIR "${_gsc_default_juce}" CACHE PATH "Path to a JUCE 9 checkout")
add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)

set(GSC_SOURCES
    Source/SidechainCore.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

juce_add_plugin(GSideChain
    COMPANY_NAME "G Audio"
    BUNDLE_ID com.gaudio.gsidechain
    PLUGIN_MANUFACTURER_CODE GAud
    PLUGIN_CODE Gsch
    FORMATS VST3 Standalone
    PRODUCT_NAME "G-SideChain"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(GSideChain PRIVATE ${GSC_SOURCES})

set(GSC_DEFS JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_compile_definitions(GSideChain PUBLIC ${GSC_DEFS})

target_link_libraries(GSideChain
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Self-test: runs the analyzer + engine on synthetic kick/bass and renders the UI to a PNG.
juce_add_console_app(GSideChainTests PRODUCT_NAME "GSideChainTests")
target_sources(GSideChainTests PRIVATE Tests/TestMain.cpp ${GSC_SOURCES})
target_compile_definitions(GSideChainTests PRIVATE ${GSC_DEFS})
target_link_libraries(GSideChainTests
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
