cmake_minimum_required(VERSION 3.22)
project(Graupel VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
endif()

add_executable(render_wav test/render_wav.cpp ${GRAUPEL_DSP_SOURCES})
target_include_directories(render_wav PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/eurorack)
