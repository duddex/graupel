#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
using APVTS = juce::AudioProcessorValueTreeState;
juce::String id(const char* name) { return juce::String(name); }
}

GraupelAudioProcessor::GraupelAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "PARAMETERS", createParameterLayout()) {}

APVTS::ParameterLayout GraupelAudioProcessor::createParameterLayout() {
  APVTS::ParameterLayout layout;
  layout.add(std::make_unique<juce::AudioParameterChoice>(id("mode"), "Mode",
      juce::StringArray{"Grain", "Stretch", "Loop", "Spectral"}, 0));
  layout.add(std::make_unique<juce::AudioParameterChoice>(id("quality"), "Quality",
      juce::StringArray{"1 s stereo", "2 s mono", "4 s stereo", "8 s mono"}, 0));
  layout.add(std::make_unique<juce::AudioParameterChoice>(id("fidelity"), "Fidelity",
      juce::StringArray{"Authentic", "HQ"}, 0));
  auto unit = [&layout](const char* name, const char* label, float value) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(id(name), label,
        juce::NormalisableRange<float>(0.0f, 1.0f), value));
  };
  unit("position", "Position", 0.5f);
  unit("size", "Size", 0.5f);
  layout.add(std::make_unique<juce::AudioParameterFloat>(id("pitch"), "Pitch",
      juce::NormalisableRange<float>(-48.0f, 48.0f, 1.0f), 0.0f));
  layout.add(std::make_unique<juce::AudioParameterFloat>(id("pitchFine"), "Pitch fine",
      juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
  unit("density", "Density", 0.55f);
  unit("texture", "Texture", 0.5f);
  unit("dryWet", "Dry/Wet", 0.5f);
  unit("stereoSpread", "Stereo Spread", 0.5f);
  unit("feedback", "Feedback", 0.0f);
  unit("reverb", "Reverberation", 0.0f);
  unit("granularOverlap", "Grain Overlap", 0.0f);
  unit("granularWindow", "Grain Window", 0.5f);
  unit("granularSpread", "Grain Spread", 0.5f);
  unit("spectralQuantization", "Spectral Quantization", 0.5f);
  unit("spectralRefresh", "Spectral Refresh", 0.5f);
  unit("spectralPhase", "Spectral Phase", 0.0f);
  unit("spectralWarp", "Spectral Warp", 0.5f);
  layout.add(std::make_unique<juce::AudioParameterFloat>(id("inputGain"), "Input Gain",
      juce::NormalisableRange<float>(-18.0f, 6.0f), 0.0f));
  layout.add(std::make_unique<juce::AudioParameterBool>(id("freeze"), "Freeze", false));
  layout.add(std::make_unique<juce::AudioParameterBool>(id("summedInput"), "Summed Input", false));
  return layout;
}

void GraupelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
  engine_.Prepare(sampleRate, samplesPerBlock);
}

void GraupelAudioProcessor::releaseResources() {}

bool GraupelAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() &&
         layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void GraupelAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&) {
  juce::ScopedNoDenormals noDenormals;
  auto value = [this](const char* name) { return parameters.getRawParameterValue(name)->load(); };
  // AudioParameterChoice stores its value in the normalised 0..1 parameter
  // representation; convert that once per block to the DSP's integer enums.
  auto choice = [this](const char* name, int maximum) {
    return juce::jlimit(0, maximum, juce::roundToInt(
        parameters.getRawParameterValue(name)->load() * static_cast<float>(maximum)));
  };
  graupel::Parameters p;
  p.mode = static_cast<graupel::Mode>(choice("mode", 3));
  p.quality = choice("quality", 3);
  p.fidelity = static_cast<graupel::Fidelity>(choice("fidelity", 1));
  p.position = value("position"); p.size = value("size");
  p.pitch = value("pitch") + value("pitchFine");
  p.density = value("density"); p.texture = value("texture");
  p.dry_wet = value("dryWet"); p.stereo_spread = value("stereoSpread");
  p.feedback = value("feedback"); p.reverb = value("reverb");
  p.granular_overlap = value("granularOverlap");
  p.granular_window_shape = value("granularWindow");
  p.granular_stereo_spread = value("granularSpread");
  p.spectral_quantization = value("spectralQuantization");
  p.spectral_refresh_rate = value("spectralRefresh");
  p.spectral_phase_randomization = value("spectralPhase");
  p.spectral_warp = value("spectralWarp");
  p.input_gain_db = value("inputGain");
  p.freeze = value("freeze") >= 0.5f;
  p.summed_input = value("summedInput") >= 0.5f;

  auto* left = buffer.getReadPointer(0);
  auto* right = buffer.getReadPointer(1);
  engine_.Process(left, right, buffer.getWritePointer(0), buffer.getWritePointer(1),
                  buffer.getNumSamples(), p);
  const auto status = engine_.GetStatus();
  meter_peak_.store(status.meter_peak);
  frozen_.store(status.frozen);
  empty_.store(status.empty);
}

juce::AudioProcessorEditor* GraupelAudioProcessor::createEditor() {
  return new GraupelAudioProcessorEditor(*this);
}

void GraupelAudioProcessor::getStateInformation(juce::MemoryBlock& dest) {
  if (auto xml = parameters.copyState().createXml()) copyXmlToBinary(*xml, dest);
}

void GraupelAudioProcessor::setStateInformation(const void* data, int size) {
  if (auto xml = getXmlFromBinary(data, size))
    if (xml->hasTagName(parameters.state.getType())) parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new GraupelAudioProcessor();
}
