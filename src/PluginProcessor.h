#pragma once

#include "dsp/graupel_engine.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>

class GraupelAudioProcessor final : public juce::AudioProcessor {
 public:
  GraupelAudioProcessor();
  ~GraupelAudioProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }
  const juce::String getName() const override { return "Graupel"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}
  void getStateInformation(juce::MemoryBlock&) override;
  void setStateInformation(const void*, int) override;

  juce::AudioProcessorValueTreeState parameters;

  static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  float meterPeak() const noexcept { return meter_peak_.load(); }
  bool isFrozen() const noexcept { return frozen_.load(); }
  bool isBufferEmpty() const noexcept { return empty_.load(); }

 private:
  graupel::Engine engine_;
  std::atomic<float> meter_peak_{0.0f};
  std::atomic<bool> frozen_{false};
  std::atomic<bool> empty_{true};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraupelAudioProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();
