# Graupel

Graupel is an unofficial granular texture processor that fragments and
reassembles a recording of its own input. It is an audio effect, not an
instrument: there is no MIDI, CV, transport, file loading, or tempo sync.

Build requirements are CMake 3.22+, C++20, and JUCE 9.0.5. Configure and build
with:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The DSP is derived from the Clouds firmware by Émilie Gillet and is kept behind
a JUCE-free seam. The Authentic path operates in the module's 32 kHz domain;
HQ runs at the host rate without companding while preserving the selected
Recording Buffer duration.

Unofficial project; “Mutable Instruments” is a trademark of its owner; nothing
is affiliated or endorsed; the DSP derives from the Clouds firmware by Émilie
Gillet (MIT).
