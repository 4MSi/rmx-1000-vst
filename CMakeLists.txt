cmake_minimum_required(VERSION 3.22)

project(RMX1000RemixStation VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

set(RMX_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RMX_FORMATS AU)
endif()

juce_add_plugin(RMX1000
    COMPANY_NAME "4MSi Audio"
    COMPANY_WEBSITE "https://github.com/4MSi"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Msi4
    PLUGIN_CODE Rmx1
    FORMATS ${RMX_FORMATS}
    PRODUCT_NAME "RMX-1000"
)

target_sources(RMX1000
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/DSP/BpmEngine.h
        Source/DSP/IsolatorFX.h
        Source/DSP/SceneFX.h
        Source/DSP/XPadFX.h
        Source/DSP/ReleaseFX.h
        Source/UI/RmxLookAndFeel.h
        Source/UI/RmxComponents.h
)

target_compile_definitions(RMX1000
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(RMX1000
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(RMX1000 PRIVATE /W3 /wd4244 /wd4267 /wd4305 /wd4996)
endif()
