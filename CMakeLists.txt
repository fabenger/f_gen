cmake_minimum_required(VERSION 3.22)
project(f_gen VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# static MSVC runtime -> no Visual C++ redistributable needed on the target machine
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(FGEN_BUILD_TESTS "Build the offline test tool" OFF)

if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(f_gen
    COMPANY_NAME "fabian"
    PRODUCT_NAME "f_gen"
    BUNDLE_ID "com.fabian.fgen"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Fabn
    PLUGIN_CODE Fgen
    VST3_CATEGORIES Instrument Sampler
    FORMATS VST3 Standalone)

juce_generate_juce_header(f_gen)

juce_add_binary_data(f_gen_assets SOURCES Resources/avatar.png)
set_target_properties(f_gen_assets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(f_gen PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(f_gen PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(f_gen
    PRIVATE
        f_gen_assets
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(FGEN_BUILD_TESTS)
    juce_add_console_app(fgen_tests PRODUCT_NAME "fgen_tests")
    target_sources(fgen_tests PRIVATE tests/TestMain.cpp)
    # reuse the plugin's compiled JUCE code: same include paths and defines, link the shared-code lib
    target_include_directories(fgen_tests PRIVATE Source $<TARGET_PROPERTY:f_gen,INCLUDE_DIRECTORIES>)
    target_compile_definitions(fgen_tests PRIVATE $<TARGET_PROPERTY:f_gen,COMPILE_DEFINITIONS>)
    target_link_libraries(fgen_tests PRIVATE f_gen f_gen_assets)
endif()
