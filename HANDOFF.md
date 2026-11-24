# Graupel — handoff to the Windows build machine

One page to type exactly once. Decisions live in `GLOSSARY.md` (vocabulary) and
`docs/adr/0001`–`0013` (why). This file is the mechanical part: what to create,
in what order, with which strings. Nothing here is a new decision — if something
below contradicts an ADR, the ADR wins and this file is wrong.

`prompts/` holds the three copy-paste prompts that drive this file: `prompt-1-dsp-port.txt`
(submodule → grain audio in a WAV), `prompt-2-plugin-wrapper.txt`, `prompt-3-editor-tests-ci.txt`.
Use them one milestone at a time; each ends in a gate you confirm before the next.

## 1. First commit

```
git init && mkdir -p dsp src test docs licenses
# copy in: GLOSSARY.md  docs/adr/*.md
LICENSE            AGPL-3.0 full text, whole project (ADR 0007)
NOTICE             itemises: ported Clouds DSP (Émilie Gillet, MIT, which files),
                   JUCE (AGPLv3 option), Graupel code (AGPL-3.0 as a whole)
licenses/Clouds-MIT.txt   the MIT text, verbatim, from eurorack/clouds/dsp/*
README.md          what it is + the trademark line: "unofficial project;
                   'Mutable Instruments' is a trademark of its owner; nothing is
                   affiliated or endorsed; the DSP derives from the Clouds
                   firmware by Émilie Gillet (MIT)"
CMakeLists.txt
prompts/           the three coding-agent prompts, one per milestone
```

## 2. Submodule, pinned, read-only (ADR 0001)

```
git submodule add https://github.com/pichenettes/eurorack.git eurorack
git -C eurorack checkout f4cfa7697095a28d292bff758beb51deb49b5a86
git submodule update --init --recursive     # stmlib is nested; five submodules total
git -C eurorack status --porcelain          # must stay empty, forever
```

Include root is the repo root of the submodule: upstream code includes
`"clouds/dsp/…"` and `"stmlib/stmlib.h"`, so add `eurorack/` to the include
path and nothing deeper.

## 3. What compiles, and the one define that makes it desktop

`eurorack/clouds/test/makefile` is upstream's own desktop build. It proves this
file set links without STM32, and it compiles with **`-DTEST`** — the guard that
switches `clouds/drivers/debug_pin.h` off the `stm32f4xx_conf.h` path. Define
`TEST` on the DSP sources only.

```
dsp (from submodule):  clouds/dsp/granular_processor.cc  clouds/dsp/correlator.cc
                       clouds/dsp/mu_law.cc
                       clouds/dsp/pvoc/frame_transformation.cc
                       clouds/dsp/pvoc/phase_vocoder.cc  clouds/dsp/pvoc/stft.cc
                       clouds/resources.cc
                       stmlib/dsp/atan.cc  stmlib/dsp/units.cc  stmlib/utils/random.cc
NOT compiled:          clouds.cc  cv_scaler.cc  settings.cc  ui.cc  drivers/*  bootloader/*
```

Everything else the DSP needs is header-only (`parameter_interpolator.h`,
`buffer_allocator.h`, `fx/*.h`, `audio_buffer.h`, the three sample players).
`resources.cc` is consumed as-is; `resources.py` is never run (ADR 0011).

> ⚠️ `TEST` is also a GoogleTest macro. Keep using plain asserts — the
> `GraupelDspTests` shape below — or the define will bite.

## 4. Plugin target

```cmake
cmake_minimum_required(VERSION 3.22)
project(Graupel VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.5
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

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
```

The VST3 class UID is derived from `Ddex` + `Grpl` alone
(`VST3Interface::jucePluginId`), so `PRODUCT_NAME` and `COMPANY_NAME` may be
edited later; the two codes may not.

## 5. The seam `dsp/` has to provide

Drive the processor the way `clouds/clouds.cc` did, minus the hardware:

- `GranularProcessor::Init(void* large_buffer, size_t large_buffer_size,
  void* small_buffer, size_t small_buffer_size)` — **we allocate the memory**;
  the Authentic budget is 128 KB plus workspace (ADR 0004).
- Fill `Parameters` (`clouds/dsp/parameters.h`) each block: `position, size,
  pitch, density, texture, dry_wet, stereo_spread, feedback, reverb, freeze,
  trigger, gate` + `granular{overlap, window_shape, stereo_spread,
  use_deterministic_seed}` + `spectral{quantization, refresh_rate,
  phase_randomization, warp}`. `trigger`/`gate` stay internal and unused
  (ADR 0002); `use_deterministic_seed` must stay honoured so presets don't lie.
- `set_playback_mode(PlaybackMode)` for the Mode dropdown; snapshot and restore
  the Recording Buffer around any reallocation, including the ones the spectral
  path insists on (ADR 0005).
- Process in **32-sample blocks at 32 kHz** (`kMaxCodecBlockSize = 32`;
  `clouds/test/clouds_test.cc` uses `kSampleRate = 32000, kBlockSize = 32`),
  int16 stereo frames in and out, exactly as `FillBuffer` did.
- Host rate → 32 kHz is **our own polyphase converter**, 44.1 kHz first-class
  (ADR 0012). HQ runs at host rate and skips the µ-law/companding only.
- Soft clip stays in the capture path in both Fidelity modes, at the meter's top
  threshold (ADR 0013).

## 6. Controls (ADR 0002/0004/0009, `GLOSSARY.md`)

Mode dropdown: `Grain | Stretch | Loop | Spectral`.
Quality: `1 s stereo | 2 s mono | 4 s stereo | 8 s mono`.
Fidelity: `Authentic | HQ` — tooltip must say duration does not change.
Core parameters: Position, Size, Pitch (semitones snapped + fine drift),
Density, Texture, Dry/Wet, Stereo Spread, Feedback, Reverberation, Input Gain
(−18…+6 dB), Freeze (latching), Summed Input (on/off). Mode extras appear only
for the selected Mode. Grain ceiling 40–60, identical in both Fidelity modes.
Meter shows input, output while Frozen, with an empty-buffer indicator.
Defaults: Grain, `1 s stereo`, Authentic, Density just past centre, everything
else centred/zero, Freeze off. No file loading, no memory slots, no CV, no
MIDI, no tempo sync, no voices control.

## 7. Other targets (ADR 0008)

```cmake
add_executable(render_wav      test/render_wav.cpp  <the dsp sources>)
add_executable(GraupelDspTests test/DspTests.cpp     <the dsp sources>)
enable_testing()
```
Both include `dsp/` and link **no JUCE** — that absence *is* the boundary.
`render_wav` starts from upstream's `clouds/test/clouds_test.cc` (it already has
the WAV header writer and the 32 kHz/32-block loop) plus scripted parameter
sweeps, so Authentic-vs-HQ and "the frozen audio survived the Mode switch" are
render diffs, not anecdotes.

## 8. CI (house pattern, ADR 0008)

`windows-latest`; `actions/checkout@v7`; cache `build/_deps` keyed on
`hashFiles('CMakeLists.txt')`; configure + build Release; **pluginval**
strictness 8, `--skip-gui-tests`, launched with `-Wait` because the exe is
GUI-subsystem; package `.vst3` + Standalone exe + `README.md LICENSE NOTICE
licenses/ docs/ GLOSSARY.md` into a zip named `v*` from the tag, else
`dev-<sha7>`.

## 9. Parked, not forgotten

CPU/instance budget beyond the grain ceiling · VST3 parameter migration for
future versions · installers · macOS/Linux builds · whether `beads/` joins the
scope (it is MIT in the same submodule already, and a different product).
