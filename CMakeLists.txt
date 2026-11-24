cmake_minimum_required(VERSION 3.22)
project(Graupel VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

# The ported Clouds DSP, exactly the list in HANDOFF.md §3. Upstream's
# clouds/test/makefile agrees, and additionally names clouds_test.cc (its own
# test main), which we do not compile here.
set(GRAUPEL_DSP_SOURCES
    eurorack/clouds/dsp/granular_processor.cc
    eurorack/clouds/dsp/correlator.cc
    eurorack/clouds/dsp/mu_law.cc
    eurorack/clouds/dsp/pvoc/frame_transformation.cc
    eurorack/clouds/dsp/pvoc/phase_vocoder.cc
    eurorack/clouds/dsp/pvoc/stft.cc
    eurorack/clouds/resources.cc
    eurorack/stmlib/dsp/atan.cc
    eurorack/stmlib/dsp/units.cc
    eurorack/stmlib/utils/random.cc
)

# TEST selects upstream's non-STM32 paths (clouds/drivers/debug_pin.h,
# stmlib/dsp/dsp.h). It is applied to the DSP sources only: TEST is also a
# GoogleTest macro, and our tests use plain asserts.
set_source_files_properties(${GRAUPEL_DSP_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS "TEST")

if(MSVC)
    # Upstream stmlib/utils/dsp.h uses __attribute__((always_inline)) without a
    # platform guard. We never edit eurorack/, so force-include our shim on the
    # DSP sources only.
    set_source_files_properties(${GRAUPEL_DSP_SOURCES} PROPERTIES
        COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/dsp/msvc_compat.h")
    set_source_files_properties(dsp/graupel_engine.cpp PROPERTIES
        COMPILE_OPTIONS "/FI${CMAKE_CURRENT_SOURCE_DIR}/dsp/msvc_compat.h")
endif()

add_executable(render_wav test/render_wav.cpp ${GRAUPEL_DSP_SOURCES})
target_include_directories(render_wav PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/eurorack)

add_executable(GraupelDspTests test/DspTests.cpp
    ${GRAUPEL_DSP_SOURCES} dsp/graupel_engine.cpp dsp/polyphase_resampler.cpp)
target_include_directories(GraupelDspTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/eurorack)
enable_testing()
add_test(NAME GraupelDspTests COMMAND GraupelDspTests)

juce_add_plugin(Graupel
    COMPANY_NAME               "duddex"
    COMPANY_WEBSITE            "https://github.com/duddex"
    PLUGIN_MANUFACTURER_CODE   Ddex      # immutable, ADR 0006
    PLUGIN_CODE                Grpl      # immutable, ADR 0006
    FORMATS                    VST3 Standalone
    PRODUCT_NAME               "Graupel"
    DESCRIPTION                "Granular texture processor: fragments and reassembles a recording of its own input"
    VST3_CATEGORIES            Fx
    IS_SYNTH                   FALSE
    NEEDS_MIDI_INPUT           FALSE     # ADR 0002: an effect, not an instrument
    NEEDS_MIDI_OUTPUT          FALSE
    IS_MIDI_EFFECT             FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD    FALSE)
target_compile_definitions(Graupel PRIVATE JUCE_VST3_CAN_REPLACE_VST2=0)
target_sources(Graupel PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    ${GRAUPEL_DSP_SOURCES}
    dsp/graupel_engine.cpp
    dsp/polyphase_resampler.cpp)
target_include_directories(Graupel PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/eurorack)
target_link_libraries(Graupel PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)
