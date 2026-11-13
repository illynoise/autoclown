# ==============================================================================
#  Auto Clown — real-time pitch correction (VST3 / AUv2)
# ==============================================================================
cmake_minimum_required(VERSION 3.22)

# Universal binary + deployment target must be set before project().
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary" FORCE)
    endif()
endif()

project(AutoClown VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(AUTOCLOWN_BUILD_PLUGIN "Build the JUCE plugin targets" ON)
option(AUTOCLOWN_BUILD_TESTS  "Build DSP unit tests and the CPU benchmark" ON)
option(AUTOCLOWN_STANDALONE   "Also build a Standalone app (handy for UI work)" OFF)
set(AUTOCLOWN_JUCE_PATH "" CACHE PATH "Use a local JUCE checkout instead of fetching it")

# ------------------------------------------------------------------------------
#  Branding — PLACEHOLDERS. Replace before shipping.
#  The manufacturer code must contain at least one upper-case letter (Apple rule)
#  and the plugin code must be unique among your products.
# ------------------------------------------------------------------------------
set(AC_COMPANY_NAME      "YourCompany"            CACHE STRING "PLACEHOLDER manufacturer name")
set(AC_COMPANY_WEBSITE   "https://example.com"    CACHE STRING "PLACEHOLDER website")
set(AC_BUNDLE_ID         "com.yourcompany.autoclown" CACHE STRING "PLACEHOLDER bundle id")
set(AC_MANUFACTURER_CODE "Ycmp"                   CACHE STRING "PLACEHOLDER 4-char manufacturer code")
set(AC_PLUGIN_CODE       "AcLn"                   CACHE STRING "PLACEHOLDER 4-char plugin code")

# ------------------------------------------------------------------------------
#  DSP core (no JUCE dependency — unit-testable and benchmarkable on any OS)
# ------------------------------------------------------------------------------
add_library(AutoClownDSP STATIC
    src/dsp/PitchDetector.cpp
    src/dsp/ScaleMapper.cpp
    src/dsp/RetuneEngine.cpp
    src/dsp/PsolaShifter.cpp
    src/dsp/ClassicShifter.cpp
    src/dsp/Analysis.cpp
    src/dsp/AutoClownEngine.cpp)
target_include_directories(AutoClownDSP PUBLIC src)
if(MSVC)
    target_compile_options(AutoClownDSP PRIVATE /W4)
else()
    target_compile_options(AutoClownDSP PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wno-sign-conversion
                                                $<$<CONFIG:Release>:-O3>)
endif()

# ------------------------------------------------------------------------------
#  Plugin
# ------------------------------------------------------------------------------
if(AUTOCLOWN_BUILD_PLUGIN)
    if(AUTOCLOWN_JUCE_PATH)
        add_subdirectory(${AUTOCLOWN_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        9.0.5
            GIT_SHALLOW    TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(AC_FORMATS VST3)
    if(APPLE)
        list(APPEND AC_FORMATS AU)
    endif()
    if(AUTOCLOWN_STANDALONE)
        list(APPEND AC_FORMATS Standalone)
    endif()

    juce_add_plugin(AutoClown
        PRODUCT_NAME                 "Auto Clown"
        COMPANY_NAME                 "${AC_COMPANY_NAME}"
        COMPANY_WEBSITE              "${AC_COMPANY_WEBSITE}"
        BUNDLE_ID                    "${AC_BUNDLE_ID}"
        PLUGIN_MANUFACTURER_CODE     "${AC_MANUFACTURER_CODE}"
        PLUGIN_CODE                  "${AC_PLUGIN_CODE}"
        DESCRIPTION                  "Real-time pitch correction"
        FORMATS                      ${AC_FORMATS}
        IS_SYNTH                     FALSE
        NEEDS_MIDI_INPUT             TRUE     # MIDI target notes -> AU type 'aumf'
        NEEDS_MIDI_OUTPUT            FALSE
        IS_MIDI_EFFECT               FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS  FALSE
        COPY_PLUGIN_AFTER_BUILD      FALSE
        VST3_CATEGORIES              "Fx" "Pitch Shift"
        AU_MAIN_TYPE                 "kAudioUnitType_MusicEffect"
        AU_SANDBOX_SAFE              TRUE
        HARDENED_RUNTIME_ENABLED     TRUE
        MICROPHONE_PERMISSION_ENABLED TRUE)

    juce_add_binary_data(AutoClownAssets
        HEADER_NAME AutoClownAssets.h
        NAMESPACE   AutoClownAssets
        SOURCES
            resources/fonts/Rye-Regular.ttf
            resources/fonts/BarlowSemiCondensed-SemiBold.ttf
            resources/fonts/BarlowSemiCondensed-Bold.ttf)

    target_sources(AutoClown PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/Parameters.cpp
        src/plugin/Presets.cpp
        src/plugin/GlobalSettings.cpp
        src/ui/PluginEditor.cpp
        src/ui/CircusLookAndFeel.cpp
        src/ui/Logo.cpp
        src/ui/BigTopMeter.cpp
        src/ui/KeyboardStrip.cpp
        src/ui/Widgets.cpp)

    target_compile_definitions(AutoClown PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

    target_link_libraries(AutoClown
        PRIVATE
            AutoClownDSP
            AutoClownAssets
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# Optional: offline UI snapshot renderer (used for docs/screenshots and visual checks)
option(AUTOCLOWN_BUILD_TOOLS "Build developer tools (UI snapshot renderer)" OFF)
if(AUTOCLOWN_BUILD_PLUGIN AND AUTOCLOWN_BUILD_TOOLS)
    juce_add_console_app(AutoClownSnapshot PRODUCT_NAME "AutoClownSnapshot")
    target_sources(AutoClownSnapshot PRIVATE
        tools/Snapshot.cpp
        src/plugin/PluginProcessor.cpp src/plugin/Parameters.cpp src/plugin/Presets.cpp src/plugin/GlobalSettings.cpp
        src/ui/PluginEditor.cpp src/ui/CircusLookAndFeel.cpp src/ui/Logo.cpp src/ui/BigTopMeter.cpp
        src/ui/KeyboardStrip.cpp src/ui/Widgets.cpp)
    target_include_directories(AutoClownSnapshot PRIVATE src tests)
    target_compile_definitions(AutoClownSnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Auto Clown" JucePlugin_VersionString="1.0.0")
    target_link_libraries(AutoClownSnapshot PRIVATE AutoClownDSP AutoClownAssets
        juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()

# ------------------------------------------------------------------------------
#  Tests + benchmark
# ------------------------------------------------------------------------------
if(AUTOCLOWN_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
