# Graupel

Graupel is an unofficial granular texture processor that fragments and
reassembles a Recording Buffer made from its own input. It is an audio effect,
not an instrument: there is no MIDI, CV, transport, file loading, or tempo sync.

Project: https://github.com/duddex/graupel

## Install

Copy `Graupel.vst3` to your system VST3 directory, rescan plugins in the DAW,
and insert Graupel as a stereo audio effect. The Standalone build is included
for use without a DAW.

## Controls

- **Mode:** Grain, Stretch, Loop, or Spectral.
- **Quality:** 1 s stereo, 2 s mono, 4 s stereo, or 8 s mono. Quality chooses
  how the fixed Recording Buffer budget reaches into the past.
- **Fidelity:** Authentic preserves the hardware signal path; HQ runs at the
  host rate without companding. Fidelity changes the glass, not the selected
  duration.
- **Core:** Position, Size, Pitch, fine pitch drift, Density, Texture, Dry/Wet,
  Stereo Spread, Feedback, Reverberation, and Input Gain (-18 to +6 dB).
- **Freeze:** latches the Recording Buffer so new input is not captured.
- **Summed Input:** sums the two input channels before capture; it does not
  change Quality or Recording Buffer duration.

The editor shows Grain and Spectral mode extras only when those modes are
selected. Its Meter follows input while capturing and output while Frozen, and
reports when the Recording Buffer is empty.

Build requirements are CMake 3.22+, C++20, and JUCE 9.0.5. Configure and build
with:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The Release targets are `Graupel_VST3`, `Graupel_Standalone`,
`GraupelDspTests`, and `render_wav`. Validate the VST3 with pluginval:

```powershell
pluginval.exe --strictness-level 8 --skip-gui-tests `
  --validate build\Graupel_artefacts\Release\VST3\Graupel.vst3
```

The DSP is derived from the Clouds firmware by Émilie Gillet and is kept behind
a JUCE-free seam. The Authentic path operates in the module's 32 kHz domain;
HQ runs at the host rate without companding while preserving the selected
Recording Buffer duration.

Unofficial project; “Mutable Instruments” is a trademark of its owner; nothing
is affiliated or endorsed; the DSP derives from the Clouds firmware by Émilie
Gillet (MIT).

The project is licensed under the GNU Affero General Public License, version 3.
See `LICENSE`, `NOTICE`, and `licenses/Clouds-MIT.txt` for attribution and
license details.
