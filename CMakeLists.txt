cmake_minimum_required(VERSION 3.22)

project(HouseComp VERSION 1.0.0)

# ---------------------------------------------------------------------------
# JUCE
# Expects JUCE as a sibling/subdir. Either:
#   git clone https://github.com/juce-framework/JUCE  (into ./JUCE)
# or set -DJUCE_DIR=/path/to/JUCE when configuring.
# ---------------------------------------------------------------------------
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# melatonin_blur — fast drop-shadows / glow for the UI (JUCE module).
# Cloned into ./modules/melatonin_blur. Added AFTER JUCE so juce_add_module exists.
# ---------------------------------------------------------------------------
# Direct2D path pulls a JUCE-internal header that differs across JUCE 8 point
# releases (missing in 8.0.4) — force the portable path so it builds cleanly.
set(MELATONIN_BLUR_USE_DIRECT2D OFF CACHE BOOL "" FORCE)
add_subdirectory(modules/melatonin_blur)

# On macOS also build Audio Unit (.component) for Logic/GarageBand; other
# platforms stay VST3 + Standalone.
if(APPLE)
    set(HOUSECOMP_FORMATS VST3 AU Standalone)
else()
    set(HOUSECOMP_FORMATS VST3 Standalone)
endif()

juce_add_plugin(HouseComp
    COMPANY_NAME            "ACRONA AUDIO"
    PLUGIN_MANUFACTURER_CODE Acrn
    PLUGIN_CODE             Hcmp
    FORMATS                 ${HOUSECOMP_FORMATS}
    PRODUCT_NAME            "HOUZY"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

# Embedded animated-background sprite sheet (24 frames stacked vertically) and a
# UI font that includes Cyrillic (so Russian tooltips render, no mojibake).
juce_add_binary_data(HouseCompData SOURCES Resources/bg_sprite.png Resources/AppFont.ttf Resources/DrukWide.ttf)

target_sources(HouseComp PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Config.h
)

target_compile_definitions(HouseComp PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(HouseComp PRIVATE
    HouseCompData
    melatonin_blur
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
