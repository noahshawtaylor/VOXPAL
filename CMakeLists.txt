cmake_minimum_required(VERSION 3.22)
project(VocalChain VERSION 0.3.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: either set -DJUCE_DIR=/path/to/JUCE or let CMake fetch it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Formats.
#   Ableton Live : VST3 (or AU)
#   Logic Pro    : AU
#   Pro Tools    : AAX  -> needs the Avid AAX SDK. Pass -DAAX_SDK_PATH=/path/to/aax-sdk
#                  and it is added automatically. Release Pro Tools also needs the
#                  bundle PACE-signed (see README "Pro Tools / AAX").
set(FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND FORMATS AU)
endif()
if(DEFINED AAX_SDK_PATH AND EXISTS "${AAX_SDK_PATH}")
    juce_set_aax_sdk_path("${AAX_SDK_PATH}")
    list(APPEND FORMATS AAX)
    message(STATUS "AAX enabled from ${AAX_SDK_PATH}")
endif()

juce_add_plugin(VocalChain
    COMPANY_NAME "Noah Shaw Taylor"
    BUNDLE_ID "com.noahshawtaylor.voxpal"
    PLUGIN_MANUFACTURER_CODE Nsta          # 4 chars, first must be uppercase
    PLUGIN_CODE Vxpl
    FORMATS ${FORMATS}
    PRODUCT_NAME "VOXPAL"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY AAX_ePlugInCategory_Dynamics)

target_sources(VocalChain PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(VocalChain PRIVATE Source)

target_compile_definitions(VocalChain PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(VocalChain PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Standalone DSP checks (no JUCE needed): cmake --build . --target dsp_test && ./dsp_test
add_executable(dsp_test tests/dsp_test.cpp)
target_include_directories(dsp_test PRIVATE Source)

# Headless runtime smoke test of the real processor (cmake --build build --target host_smoke && ./build/host_smoke)
juce_add_console_app(host_smoke PRODUCT_NAME "host_smoke")
target_sources(host_smoke PRIVATE tests/host_smoke.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_include_directories(host_smoke PRIVATE Source)
target_compile_definitions(host_smoke PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=0 JUCE_MODAL_LOOPS_PERMITTED=0
    JucePlugin_Name="VOXPAL" JucePlugin_IsSynth=0 JucePlugin_WantsMidiInput=0 JucePlugin_ProducesMidiOutput=0
    JucePlugin_IsMidiEffect=0 JucePlugin_EditorRequiresKeyboardFocus=0 JucePlugin_VersionString="0.3.0"
    JucePlugin_Manufacturer="Noah Shaw Taylor" JucePlugin_ManufacturerCode=0x4e737461 JucePlugin_PluginCode=0x5678706c
    JucePlugin_VSTUniqueID=0x5678706c JucePlugin_VSTCategory=0 JucePlugin_Vst3Category="Fx" JucePlugin_AUMainType=0
    JucePlugin_AUSubType=0 JucePlugin_AUExportPrefix=x JucePlugin_AUExportPrefixQuoted="x" JucePlugin_AUManufacturerCode=0
    JucePlugin_CFBundleIdentifier=x JucePlugin_AAXIdentifier=x JucePlugin_AAXManufacturerCode=0 JucePlugin_AAXProductId=0
    JucePlugin_AAXCategory=0 JucePlugin_AAXDisableBypass=0 JucePlugin_AAXDisableMultiMono=0 JucePlugin_Desc="x"
    JucePlugin_Version=0.2 JucePlugin_VersionCode=0x200 JucePlugin_Build_VST3=0)
target_link_libraries(host_smoke PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp PUBLIC juce::juce_recommended_config_flags)
