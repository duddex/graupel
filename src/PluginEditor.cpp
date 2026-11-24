#include "PluginEditor.h"

namespace {
constexpr int kMargin = 18;
constexpr int kRow = 43;
}

GraupelAudioProcessorEditor::GraupelAudioProcessorEditor(GraupelAudioProcessor& processor)
    : AudioProcessorEditor(processor), processor_(processor) {
  setOpaque(true);
  setSize(760, 800);
  title_.setText("GRAUPEL", juce::dontSendNotification);
  title_.setFont(juce::Font(juce::FontOptions{}.withHeight(24.0f).withStyle("Bold")));
  addAndMakeVisible(title_);

  addCombo("mode", "Mode", {"Grain", "Stretch", "Loop", "Spectral"});
  addCombo("quality", "Quality", {"1 s stereo", "2 s mono", "4 s stereo", "8 s mono"});
  addCombo("fidelity", "Fidelity", {"Authentic", "HQ"});
  addSlider("position", "Position", {});
  addSlider("size", "Size", {});
  addSlider("pitch", "Pitch (semitones)", {});
  addSlider("pitchFine", "Pitch fine drift", {});
  addSlider("density", "Density", {});
  addSlider("texture", "Texture", {});
  addSlider("dryWet", "Dry/Wet", {});
  addSlider("stereoSpread", "Stereo Spread", {});
  addSlider("feedback", "Feedback", {});
  addSlider("reverb", "Reverberation", {});
  addSlider("inputGain", "Input Gain (dB)", {});
  addSlider("granularOverlap", "Overlap", {});
  addSlider("granularWindow", "Window Shape", {});
  addSlider("granularSpread", "Grain Spread", {});
  addSlider("spectralQuantization", "Quantization", {});
  addSlider("spectralRefresh", "Refresh Rate", {});
  addSlider("spectralPhase", "Phase Randomization", {});
  addSlider("spectralWarp", "Warp", {});

  freeze_.setTooltip("Latch the Recording Buffer; no new input is captured while Frozen.");
  summed_input_.setTooltip("Feed the effect a summed mono input without changing buffer Quality.");
  freeze_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff1f3f5));
  summed_input_.setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff1f3f5));
  addAndMakeVisible(freeze_);
  addAndMakeVisible(summed_input_);
  freeze_attachment_ = std::make_unique<ButtonAttachment>(processor_.parameters, "freeze", freeze_);
  summed_attachment_ = std::make_unique<ButtonAttachment>(processor_.parameters, "summedInput", summed_input_);

  meter_label_.setText("Meter", juce::dontSendNotification);
  meter_label_.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(meter_label_);
  status_label_.setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(status_label_);
  startTimerHz(30);
  updateModeVisibility();
}

void GraupelAudioProcessorEditor::addCombo(const char* parameter, const char* label,
                                           const juce::StringArray& items) {
  auto* combo = combos_.add(new juce::ComboBox());
  combo->addItemList(items, 1);
  combo->setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff343943));
  combo->setColour(juce::ComboBox::textColourId, juce::Colour(0xfff1f3f5));
  combo->setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff718096));
  combo->setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff77c7c0));
  combo->setTooltip(juce::String(parameter) == "fidelity"
      ? "Authentic models the hardware signal path. HQ uses cleaner glass; duration does not change."
      : label);
  addAndMakeVisible(combo);
  auto* text = combo_labels_.add(new juce::Label({}, label));
  text->setColour(juce::Label::textColourId, juce::Colour(0xffd9e0e8));
  text->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(text);
  combo_attachments_.push_back(std::make_unique<ComboAttachment>(processor_.parameters, parameter, *combo));
}

void GraupelAudioProcessorEditor::addSlider(const char* parameter, const char* label,
                                            juce::Rectangle<int>) {
  auto* slider = sliders_.add(new juce::Slider(juce::Slider::LinearHorizontal,
                                                juce::Slider::TextBoxRight));
  slider->setRange(0.0, 1.0);
  if (juce::String(parameter) == "pitch") slider->setRange(-48.0, 48.0, 1.0);
  if (juce::String(parameter) == "pitchFine") slider->setRange(-1.0, 1.0, 0.01);
  if (juce::String(parameter) == "inputGain") slider->setRange(-18.0, 6.0, 0.01);
  slider->setTextValueSuffix(juce::String(parameter) == "inputGain" ? " dB" : "");
  slider->setColour(juce::Slider::trackColourId, juce::Colour(0xff77c7c0));
  slider->setColour(juce::Slider::backgroundColourId, juce::Colour(0xff343943));
  slider->setColour(juce::Slider::thumbColourId, juce::Colour(0xfff1f3f5));
  slider->setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff1f3f5));
  slider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff343943));
  slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0xff718096));
  addAndMakeVisible(slider);
  auto* text = slider_labels_.add(new juce::Label({}, label));
  text->setColour(juce::Label::textColourId, juce::Colour(0xffd9e0e8));
  text->setJustificationType(juce::Justification::centredLeft);
  addAndMakeVisible(text);
  if (juce::String(parameter).startsWith("granular")) { granular_controls_.add(slider); granular_controls_.add(text); }
  if (juce::String(parameter).startsWith("spectral")) { spectral_controls_.add(slider); spectral_controls_.add(text); }
  slider_attachments_.push_back(std::make_unique<SliderAttachment>(processor_.parameters, parameter, *slider));
}

void GraupelAudioProcessorEditor::updateModeVisibility() {
  const auto mode = juce::roundToInt(processor_.parameters.getRawParameterValue("mode")->load() * 3.0f);
  for (auto* component : granular_controls_) component->setVisible(mode == 0);
  for (auto* component : spectral_controls_) component->setVisible(mode == 3);
}

void GraupelAudioProcessorEditor::timerCallback() {
  const float peak = juce::jlimit(0.0f, 1.0f, processor_.meterPeak());
  meter_label_.setText("Meter  " + juce::String(juce::Decibels::gainToDecibels(juce::jmax(peak, 1.0e-5f)), 1) + " dB",
                       juce::dontSendNotification);
  status_label_.setText(processor_.isBufferEmpty() ? "Recording Buffer: empty"
                                                    : (processor_.isFrozen() ? "Recording Buffer: Frozen (output meter)"
                                                                              : "Recording Buffer: capturing (input meter)"),
                         juce::dontSendNotification);
  updateModeVisibility();
}

void GraupelAudioProcessorEditor::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(0xff17191d));
  g.setColour(juce::Colour(0xff2a2e36));
  g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(8.0f), 8.0f);
  g.setColour(juce::Colour(0xff343943));
  g.fillRoundedRectangle(12.0f, 50.0f, 736.0f, 110.0f, 6.0f);
  g.fillRoundedRectangle(12.0f, 170.0f, 360.0f, 535.0f, 6.0f);
  g.fillRoundedRectangle(380.0f, 170.0f, 368.0f, 535.0f, 6.0f);
  g.setColour(juce::Colour(0xff9aa7b5));
  g.setFont(juce::Font(juce::FontOptions{}.withHeight(12.0f)));
  g.drawText("MODE / QUALITY / FIDELITY", 28, 48, 290, 18, juce::Justification::centredLeft);
  g.drawText("CORE PARAMETERS", 28, 168, 250, 18, juce::Justification::centredLeft);
  g.drawText("MODE EXTRAS", 396, 168, 250, 18, juce::Justification::centredLeft);
  g.setColour(juce::Colour(0xff77c7c0));
  g.fillRect(juce::Rectangle<float>(kMargin, 92.0f, 720.0f * processor_.meterPeak(), 5.0f));
}

void GraupelAudioProcessorEditor::resized() {
  title_.setBounds(kMargin, 14, 200, 36);
  int y = 62;
  for (int i = 0; i < combos_.size(); ++i) {
    const int x = kMargin + i * 240;
    combo_labels_[i]->setBounds(x, y, 220, 20);
    combos_[i]->setBounds(x, y + 22, 220, 30);
  }
  constexpr int coreY = 190;
  constexpr int left = 28;
  constexpr int right = 400;
  for (int i = 0; i < sliders_.size(); ++i) {
    const int column = i < 11 ? 0 : 1;
    const int row = column == 0 ? i : i - 11;
    const int x = column == 0 ? left : right;
    const int yy = coreY + row * kRow;
    slider_labels_[i]->setBounds(x, yy, 135, 28);
    sliders_[i]->setBounds(x + 135, yy, column == 0 ? 205 : 320, 28);
  }
  freeze_.setBounds(28, 720, 150, 28);
  summed_input_.setBounds(190, 720, 180, 28);
  meter_label_.setBounds(400, 710, 330, 28);
  status_label_.setBounds(400, 742, 330, 28);
}
