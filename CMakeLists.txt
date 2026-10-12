cmake_minimum_required(VERSION 3.22)

project(PYRA VERSION 0.9.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    # Universal binary: Apple Silicon (M1/M2/...) + Intel
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    endif()
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

if(MSVC)
    # Static runtime: friends don't need to install Visual C++ redistributables
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(JUCE)

option(PYRA_LOCAL_LINUX "Minimal Linux build used only for automated testing" OFF)

set(PYRA_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PYRA_FORMATS AU)
endif()
if(PYRA_LOCAL_LINUX)
    set(PYRA_FORMATS VST3)
endif()

juce_add_plugin(PYRA
    COMPANY_NAME "PYRA Audio"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.pyraaudio.pyra"
    PLUGIN_MANUFACTURER_CODE Pyra
    PLUGIN_CODE Pyr1
    FORMATS ${PYRA_FORMATS}
    PRODUCT_NAME "PYRA"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Art/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Art/icon_small.png"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    COPY_PLUGIN_AFTER_BUILD FALSE)

set(PYRA_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Params.cpp
    Source/ParamsExtra.cpp
    Source/ParamsPhase6.cpp
    Source/ParamsPhase7.cpp
    Source/dsp/ChordMoods.cpp
    Source/Layers.cpp
    Source/ui/PageLayers.cpp
    Source/History.cpp
    Source/Randomizer.cpp
    Source/dsp/Creative.cpp
    Source/dsp/Chords.cpp
    Source/Presets.cpp
    Source/dsp/Effects.cpp
    Source/dsp/Arp.cpp
    Source/dsp/Movement.cpp
    Source/dsp/Samples.cpp
    Source/Lang.cpp
    Source/LangExtra.cpp
    Source/LangTables.cpp
    Source/ParamHelp.cpp
    Source/ParamHelpTable.cpp
    Source/ui/Prefs.cpp
    Source/dsp/Wavetables.cpp
    Source/dsp/Voice.cpp
    Source/dsp/Engine.cpp
    Source/ui/PyraLookAndFeel.cpp
    Source/ui/HellArt.cpp
    Source/ui/Widgets.cpp
    Source/ui/Pages.cpp
    Source/ui/Pages3.cpp
    Source/ui/Pages5.cpp
    Source/ui/PageChords.cpp
    Source/ui/PresetUi.cpp)

juce_add_binary_data(PyraData SOURCES Resources/FactoryPresets.json.gz
    Resources/Fonts/Barlow-Medium.ttf Resources/Fonts/Barlow-SemiBold.ttf
    Resources/Fonts/BarlowCondensed-SemiBold.ttf Resources/Fonts/BarlowCondensed-Bold.ttf
    Resources/Fonts/UnifrakturCook-Bold.ttf Resources/Fonts/NewRocker-Regular.ttf
    Resources/Art/logo.png Resources/Art/wordmark.png
    Resources/Art/stone.jpg Resources/Art/magma.jpg Resources/Art/bg.jpg
    Resources/Art/face_chaos.png Resources/Art/face_chaos_cold.png Resources/Art/face_focus.png
    Resources/Art/face_focus_cold.png Resources/Art/face_motion.png Resources/Art/face_motion_cold.png)
set_target_properties(PyraData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

target_sources(PYRA PRIVATE ${PYRA_SOURCES})
target_include_directories(PYRA PRIVATE Source)

set(PYRA_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

if(PYRA_LOCAL_LINUX)
    list(APPEND PYRA_DEFINITIONS
        JUCE_ALSA=0 JUCE_JACK=0
        JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XRENDER=0 JUCE_USE_XCURSOR=0)
endif()

target_compile_definitions(PYRA PUBLIC ${PYRA_DEFINITIONS})
if(MSVC)
    target_compile_options(PYRA PRIVATE /utf-8 /bigobj)
endif()

target_link_libraries(PYRA
    PRIVATE
        PyraData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# Headless test tool: renders notes through the engine and checks the audio.
option(PYRA_BUILD_TESTS "Build the offline render test" OFF)
if(PYRA_BUILD_TESTS)
    juce_add_console_app(PyraRenderTest PRODUCT_NAME "PyraRenderTest")
    target_sources(PyraRenderTest PRIVATE Tools/RenderTest.cpp ${PYRA_SOURCES})
    target_include_directories(PyraRenderTest PRIVATE Source)
    target_compile_definitions(PyraRenderTest PRIVATE ${PYRA_DEFINITIONS}
        JucePlugin_Name="PYRA" JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.9.0"
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(PyraRenderTest PRIVATE
        PyraData juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
