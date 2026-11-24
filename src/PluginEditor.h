#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

class GraupelAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer {
 public:
  explicit GraupelAudioProcessorEditor(GraupelAudioProcessor&);
  ~GraupelAudioProcessorEditor() override = default;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
  using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
  using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

  void timerCallback() override;
  void addSlider(const char* parameter, const char* label, juce::Rectangle<int> area);
  void addCombo(const char* parameter, const char* label, const juce::StringArray& items);
  void updateModeVisibility();

  GraupelAudioProcessor& processor_;
  juce::Label title_, meter_label_, status_label_;
  juce::ComboBox mode_, quality_, fidelity_;
  juce::ToggleButton freeze_{"Freeze"}, summed_input_{"Summed Input"};
  juce::OwnedArray<juce::Slider> sliders_;
  juce::OwnedArray<juce::Label> slider_labels_;
  juce::OwnedArray<juce::ComboBox> combos_;
  juce::OwnedArray<juce::Label> combo_labels_;
  juce::Array<juce::Component*> spectral_controls_;
  juce::Array<juce::Component*> granular_controls_;
  std::vector<std::unique_ptr<SliderAttachment>> slider_attachments_;
  std::vector<std::unique_ptr<ComboAttachment>> combo_attachments_;
  std::unique_ptr<ButtonAttachment> freeze_attachment_;
  std::unique_ptr<ButtonAttachment> summed_attachment_;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraupelAudioProcessorEditor)
};
