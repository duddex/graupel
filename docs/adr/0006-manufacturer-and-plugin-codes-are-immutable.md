# The manufacturer and plugin codes are frozen; the name is not

A VST3 class UID is not a string a developer writes. In JUCE it is computed:
`getVST3InterfaceId()` returns `VST3Interface::jucePluginId(JucePlugin_ManufacturerCode,
JucePlugin_PluginCode, type)` (`juce_VST3ModuleInfo.h`), i.e. sixteen bytes built
from the two four-character codes — and only from them, because we build with
`JUCE_VST3_CAN_REPLACE_VST2=0`, which is the branch where a name hash would have
been used.

Chosen and frozen from the first commit: `PLUGIN_MANUFACTURER_CODE Ddex` —
duddex's own code, already shared with Aulos, Ladderine, Quadrilo and Tangier, so
it cannot be "fixed" per product — `PLUGIN_CODE Grpl`, `VST3_CATEGORIES Fx`,
`IS_SYNTH FALSE`, `NEEDS_MIDI_INPUT FALSE`, `NEEDS_MIDI_OUTPUT FALSE`,
`EDITOR_WANTS_KEYBOARD_FOCUS FALSE`, `COPY_PLUGIN_AFTER_BUILD FALSE`,
`JUCE_VST3_CAN_REPLACE_VST2=0`, two-in/two-out stereo only.

Display strings, safe to change later: `COMPANY_NAME "duddex"`,
`COMPANY_WEBSITE "https://github.com/duddex"`, `PRODUCT_NAME "Graupel"`,
`DESCRIPTION "Granular texture processor: fragments and reassembles a recording
of its own input"`, `project(Graupel VERSION 0.1.0 LANGUAGES C CXX)`.

Renaming the product, the company display name or the description costs nothing;
changing either code silently orphans every existing session's instance — its
parameters, its frozen audio, everything the host had recalled.

## Consequences

- `Grpl` is Graupel's code and nobody else's, never reused by another product,
  and written down here so no future build regenerates it by accident.
- `VST3_CATEGORIES`, channel layout, and parameter IDs join the list of things a
  future version must migrate rather than change; JUCE's VST3 parameter migration
  is the tool for that, not a licence to reshuffle.
