#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace graupel_ui {

namespace palette {
inline const juce::Colour background{0xff0f1216};
inline const juce::Colour backgroundTop{0xff181c22};
inline const juce::Colour panel{0xff1a1e25};
inline const juce::Colour panelBorder{0xff2a303a};
inline const juce::Colour control{0xff252a33};
inline const juce::Colour controlBorder{0xff363d49};
inline const juce::Colour track{0xff2c323c};
inline const juce::Colour text{0xffe8edf2};
inline const juce::Colour textDim{0xff8a94a3};
inline const juce::Colour textFaint{0xff5c6673};
inline const juce::Colour frost{0xffdff4ff};
}  // namespace palette

class LookAndFeel final : public juce::LookAndFeel_V4 {
 public:
  LookAndFeel();

  void setAccent(juce::Colour colour) { accent_ = colour; }
  juce::Colour accent() const noexcept { return accent_; }

  void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height, float position,
                        float startAngle, float endAngle, juce::Slider&) override;
  juce::Label* createSliderTextBox(juce::Slider&) override;
  void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                            bool highlighted, bool down) override;
  void drawButtonText(juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
  juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
  void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
  void drawComboBox(juce::Graphics&, int width, int height, bool down, int buttonX, int buttonY,
                    int buttonW, int buttonH, juce::ComboBox&) override;
  juce::Font getComboBoxFont(juce::ComboBox&) override;
  void positionComboBoxText(juce::ComboBox&, juce::Label&) override;

 private:
  juce::Colour accent_{0xff6fd3e0};
};

// A rotary control with its name above and its value below.
class Knob final : public juce::Component {
 public:
  enum class Format { Percent, Semitones, Fine, Decibels };

  Knob(juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
       const juce::String& name, Format format, const juce::String& tooltip = {});

  void resized() override;

 private:
  juce::Slider slider_{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
  juce::Label name_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Knob)
};

class LevelMeter final : public juce::Component {
 public:
  void setLevel(float peak);
  void paint(juce::Graphics&) override;

 private:
  float peak_db_ = -100.0f;
  float level_ = 0.0f;
  float hold_ = 0.0f;
  int hold_frames_ = 0;
};

}  // namespace graupel_ui

class GraupelAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer {
 public:
  explicit GraupelAudioProcessorEditor(GraupelAudioProcessor&);
  ~GraupelAudioProcessorEditor() override;

  void paint(juce::Graphics&) override;
  void resized() override;

 private:
  using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
  using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
  using Format = graupel_ui::Knob::Format;

  struct Panel {
    juce::String title;
    juce::Rectangle<int> bounds;
  };

  void timerCallback() override;
  graupel_ui::Knob& addKnob(const char* parameter, const char* name, Format format,
                            const juce::String& tooltip = {});
  void addCombo(juce::ComboBox& combo, juce::Label& label, const char* parameter,
                const char* name, const juce::String& tooltip);
  void setMode(int mode);
  void drawPanel(juce::Graphics&, const Panel&) const;
  static void layoutKnobRow(juce::Rectangle<int> area, const juce::Array<graupel_ui::Knob*>& knobs);

  GraupelAudioProcessor& processor_;
  graupel_ui::LookAndFeel look_and_feel_;
  juce::TooltipWindow tooltip_window_{this, 700};

  juce::OwnedArray<graupel_ui::Knob> knobs_;
  juce::Array<graupel_ui::Knob*> core_knobs_, blend_knobs_, io_knobs_;

  juce::OwnedArray<juce::TextButton> mode_buttons_;
  std::unique_ptr<juce::ParameterAttachment> mode_attachment_;
  int mode_ = 0;

  juce::ComboBox quality_, fidelity_;
  juce::Label quality_label_, fidelity_label_;
  std::vector<std::unique_ptr<ComboAttachment>> combo_attachments_;

  juce::TextButton freeze_{"FREEZE"};
  juce::ToggleButton summed_input_{"Summed input"};
  std::unique_ptr<ButtonAttachment> freeze_attachment_;
  std::unique_ptr<ButtonAttachment> summed_attachment_;

  graupel_ui::LevelMeter meter_;
  enum class BufferStatus { Unknown, Empty, Capturing, Frozen };
  BufferStatus status_ = BufferStatus::Unknown;

  juce::Rectangle<int> header_, mode_bar_, footer_, status_area_;
  Panel core_panel_{"TEXTURE"}, blend_panel_{"BLEND"}, io_panel_{"TUNE / INPUT"};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GraupelAudioProcessorEditor)
};
