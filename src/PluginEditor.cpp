#include "PluginEditor.h"

namespace graupel_ui {

namespace {
juce::Font uiFont(float height, bool bold = false, float kerning = 0.0f) {
  auto options = juce::FontOptions{}.withHeight(height);
  if (bold) options = options.withStyle("Bold");
  return juce::Font(options).withExtraKerningFactor(kerning);
}

juce::String signedString(double value, int decimals) {
  return (value > 0.0 ? "+" : "") + juce::String(value, decimals);
}

double parseNumber(const juce::String& text) {
  return text.retainCharacters("0123456789.-+").getDoubleValue();
}
}  // namespace

//==============================================================================
LookAndFeel::LookAndFeel() {
  using namespace palette;
  setColour(juce::ResizableWindow::backgroundColourId, background);
  setColour(juce::Label::textColourId, text);
  setColour(juce::Slider::textBoxTextColourId, text);
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
  setColour(juce::Slider::textBoxHighlightColourId, juce::Colour(0x6646a0b0));
  setColour(juce::TextEditor::backgroundColourId, control);
  setColour(juce::TextEditor::textColourId, text);
  setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
  setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
  setColour(juce::CaretComponent::caretColourId, text);
  setColour(juce::ComboBox::backgroundColourId, control);
  setColour(juce::ComboBox::textColourId, text);
  setColour(juce::ComboBox::outlineColourId, controlBorder);
  setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff1f242c));
  setColour(juce::PopupMenu::textColourId, text);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff313946));
  setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
  setColour(juce::TooltipWindow::backgroundColourId, juce::Colour(0xff222831));
  setColour(juce::TooltipWindow::textColourId, text);
  setColour(juce::TooltipWindow::outlineColourId, controlBorder);
}

void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float position, float startAngle, float endAngle,
                                   juce::Slider& slider) {
  const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height))
                          .reduced(4.0f);
  const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
  const auto centre = bounds.getCentre();
  const float lineWidth = juce::jmax(2.5f, radius * 0.085f);
  const float arcRadius = radius - lineWidth * 0.5f;
  const float angle = startAngle + position * (endAngle - startAngle);
  const float origin = static_cast<float>(
      static_cast<double>(slider.getProperties().getWithDefault("origin", 0.0)));
  const float originAngle = startAngle + origin * (endAngle - startAngle);
  const auto colour = slider.isEnabled() ? accent_ : palette::textFaint;
  const juce::PathStrokeType stroke(lineWidth, juce::PathStrokeType::curved,
                                    juce::PathStrokeType::rounded);

  juce::Path track;
  track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
  g.setColour(palette::track);
  g.strokePath(track, stroke);

  if (std::abs(angle - originAngle) > 0.001f) {
    juce::Path value;
    value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        juce::jmin(originAngle, angle), juce::jmax(originAngle, angle), true);
    g.setColour(colour.withAlpha(0.16f));
    g.strokePath(value, juce::PathStrokeType(lineWidth * 2.8f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
    g.setColour(colour);
    g.strokePath(value, stroke);
  }

  const float bodyRadius = arcRadius - lineWidth * 2.0f;
  const auto body = juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);
  g.setColour(juce::Colours::black.withAlpha(0.35f));
  g.fillEllipse(body.translated(0.0f, bodyRadius * 0.08f).expanded(1.5f));
  g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3d4451), centre.x, body.getY(),
                                         juce::Colour(0xff1c2027), centre.x, body.getBottom(), false));
  g.fillEllipse(body);
  g.setColour(slider.isMouseOverOrDragging() ? colour.withAlpha(0.55f)
                                             : juce::Colours::white.withAlpha(0.07f));
  g.drawEllipse(body.reduced(0.5f), 1.0f);

  const auto inner = centre.getPointOnCircumference(bodyRadius * 0.30f, angle);
  const auto outer = centre.getPointOnCircumference(bodyRadius * 0.82f, angle);
  g.setColour(slider.isEnabled() ? palette::text : palette::textFaint);
  g.drawLine({inner, outer}, juce::jmax(2.0f, bodyRadius * 0.09f));
}

juce::Label* LookAndFeel::createSliderTextBox(juce::Slider& slider) {
  auto* label = LookAndFeel_V4::createSliderTextBox(slider);
  label->setFont(uiFont(12.5f));
  label->setJustificationType(juce::Justification::centred);
  return label;
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                       bool highlighted, bool down) {
  const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
  const bool on = button.getToggleState();
  const bool isFreeze = button.getProperties().getWithDefault("freeze", false);
  const float corner = 7.0f;

  juce::Path shape;
  shape.addRoundedRectangle(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(),
                            corner, corner,
                            !button.isConnectedOnLeft(), !button.isConnectedOnRight(),
                            !button.isConnectedOnLeft(), !button.isConnectedOnRight());

  if (on) {
    const auto base = isFreeze ? palette::frost : accent_;
    if (isFreeze) {
      g.setColour(base.withAlpha(0.18f));
      g.fillRoundedRectangle(bounds.expanded(3.0f), corner + 3.0f);
    }
    g.setGradientFill(juce::ColourGradient(base.brighter(0.15f), 0.0f, bounds.getY(),
                                           base.darker(down ? 0.35f : 0.12f), 0.0f,
                                           bounds.getBottom(), false));
    g.fillPath(shape);
    g.setColour(base.brighter(0.3f).withAlpha(0.8f));
    g.strokePath(shape, juce::PathStrokeType(1.0f));
    return;
  }

  auto fill = palette::control;
  if (highlighted) fill = fill.brighter(0.08f);
  if (down) fill = fill.darker(0.15f);
  g.setColour(fill);
  g.fillPath(shape);
  g.setColour(highlighted ? accent_.withAlpha(0.5f) : palette::controlBorder);
  g.strokePath(shape, juce::PathStrokeType(1.0f));
}

void LookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button, bool highlighted, bool) {
  const bool on = button.getToggleState();
  const bool isFreeze = button.getProperties().getWithDefault("freeze", false);
  auto area = button.getLocalBounds();

  if (isFreeze) {
    auto led = area.removeFromLeft(30).toFloat();
    const auto dot = juce::Rectangle<float>(8.0f, 8.0f).withCentre(led.getCentre().translated(6.0f, 0.0f));
    g.setColour(on ? juce::Colour(0xff2a7fa8) : palette::textFaint);
    g.fillEllipse(dot);
    area.removeFromRight(14);
  }

  g.setFont(getTextButtonFont(button, button.getHeight()));
  g.setColour(on ? palette::background : (highlighted ? palette::text : palette::textDim));
  g.drawText(button.getButtonText(), area, juce::Justification::centred, false);
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight) {
  return uiFont(juce::jmin(13.0f, static_cast<float>(buttonHeight) * 0.42f), true, 0.12f);
}

void LookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool) {
  const bool on = button.getToggleState();
  auto area = button.getLocalBounds().toFloat();
  const auto pill = area.removeFromLeft(34.0f).withSizeKeepingCentre(34.0f, 18.0f);

  g.setColour(on ? accent_ : palette::track);
  g.fillRoundedRectangle(pill, 9.0f);
  g.setColour(on ? accent_.brighter(0.3f) : palette::controlBorder);
  g.drawRoundedRectangle(pill.reduced(0.5f), 9.0f, 1.0f);

  const float knobSize = 12.0f;
  const auto knob = juce::Rectangle<float>(knobSize, knobSize)
                        .withCentre({on ? pill.getRight() - 9.0f : pill.getX() + 9.0f, pill.getCentreY()});
  g.setColour(on ? palette::background : palette::textDim);
  g.fillEllipse(knob);

  area.removeFromLeft(10.0f);
  g.setFont(uiFont(13.0f));
  g.setColour(on || highlighted ? palette::text : palette::textDim);
  g.drawText(button.getButtonText(), area, juce::Justification::centredLeft, true);
}

void LookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool down, int, int, int, int,
                               juce::ComboBox& box) {
  const auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width),
                                             static_cast<float>(height)).reduced(0.5f);
  g.setColour(down ? palette::control.darker(0.1f)
                   : (box.isMouseOver(true) ? palette::control.brighter(0.06f) : palette::control));
  g.fillRoundedRectangle(bounds, 7.0f);
  g.setColour(box.hasKeyboardFocus(true) || box.isMouseOver(true) ? accent_.withAlpha(0.6f)
                                                                   : palette::controlBorder);
  g.drawRoundedRectangle(bounds, 7.0f, 1.0f);

  const float cx = static_cast<float>(width) - 16.0f;
  const float cy = static_cast<float>(height) * 0.5f;
  juce::Path chevron;
  chevron.startNewSubPath(cx - 4.0f, cy - 2.0f);
  chevron.lineTo(cx, cy + 2.5f);
  chevron.lineTo(cx + 4.0f, cy - 2.0f);
  g.setColour(accent_);
  g.strokePath(chevron, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));
}

juce::Font LookAndFeel::getComboBoxFont(juce::ComboBox&) { return uiFont(13.5f); }

void LookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label) {
  label.setBounds(6, 1, box.getWidth() - 30, box.getHeight() - 2);
  label.setFont(getComboBoxFont(box));
}

//==============================================================================
Knob::Knob(juce::AudioProcessorValueTreeState& state, const juce::String& parameterId,
           const juce::String& name, Format format, const juce::String& tooltip) {
  name_.setText(name.toUpperCase(), juce::dontSendNotification);
  name_.setFont(uiFont(11.0f, true, 0.1f));
  name_.setColour(juce::Label::textColourId, palette::textDim);
  name_.setJustificationType(juce::Justification::centred);
  name_.setInterceptsMouseClicks(false, false);
  addAndMakeVisible(name_);

  slider_.setTooltip(tooltip.isNotEmpty() ? tooltip : name);
  slider_.setMouseDragSensitivity(220);
  addAndMakeVisible(slider_);
  attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
      state, parameterId, slider_);

  switch (format) {
    case Format::Percent:
      slider_.textFromValueFunction = [](double v) { return juce::String(juce::roundToInt(v * 100.0)) + " %"; };
      slider_.valueFromTextFunction = [](const juce::String& t) { return parseNumber(t) / 100.0; };
      break;
    case Format::Semitones:
      slider_.textFromValueFunction = [](double v) { return signedString(v, 0) + " st"; };
      slider_.valueFromTextFunction = [](const juce::String& t) { return parseNumber(t); };
      break;
    case Format::Fine:
      slider_.textFromValueFunction = [](double v) { return signedString(v, 2); };
      slider_.valueFromTextFunction = [](const juce::String& t) { return parseNumber(t); };
      break;
    case Format::Decibels:
      slider_.textFromValueFunction = [](double v) { return signedString(v, 1) + " dB"; };
      slider_.valueFromTextFunction = [](const juce::String& t) { return parseNumber(t); };
      break;
  }
  slider_.updateText();

  if (format != Format::Percent)
    slider_.getProperties().set("origin", slider_.valueToProportionOfLength(0.0));

  if (auto* parameter = state.getParameter(parameterId))
    slider_.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
}

void Knob::resized() {
  auto area = getLocalBounds();
  name_.setBounds(area.removeFromTop(16));
  slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, area.getWidth(), 18);
  slider_.setBounds(area);
}

//==============================================================================
void LevelMeter::setLevel(float peak) {
  peak_db_ = juce::Decibels::gainToDecibels(peak, -100.0f);
  const float norm = juce::jlimit(0.0f, 1.0f, juce::jmap(peak_db_, -60.0f, 0.0f, 0.0f, 1.0f));
  level_ = juce::jmax(norm, level_ - 0.025f);
  if (norm >= hold_) {
    hold_ = norm;
    hold_frames_ = 45;
  } else if (--hold_frames_ <= 0) {
    hold_ = juce::jmax(norm, hold_ - 0.012f);
  }
  repaint();
}

void LevelMeter::paint(juce::Graphics& g) {
  auto area = getLocalBounds().toFloat();
  auto readout = area.removeFromRight(70.0f);
  area.removeFromRight(8.0f);

  auto accent = palette::frost;
  if (auto* lnf = dynamic_cast<LookAndFeel*>(&getLookAndFeel())) accent = lnf->accent();

  const auto bar = area.withTrimmedTop(5.0f).withHeight(10.0f);
  g.setColour(palette::track);
  g.fillRoundedRectangle(bar, 5.0f);

  if (level_ > 0.0f) {
    juce::ColourGradient gradient(accent.withAlpha(0.55f), bar.getX(), 0.0f,
                                  juce::Colour(0xffff5d5d), bar.getRight(), 0.0f, false);
    gradient.addColour(0.70, accent);
    gradient.addColour(0.88, juce::Colour(0xfff5c15c));
    juce::Graphics::ScopedSaveState save(g);
    g.reduceClipRegion(bar.withWidth(bar.getWidth() * level_).toNearestInt());
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bar, 5.0f);
  }

  if (hold_ > 0.01f) {
    const float hx = bar.getX() + bar.getWidth() * hold_;
    g.setColour(palette::text.withAlpha(0.85f));
    g.fillRect(juce::Rectangle<float>(2.0f, bar.getHeight()).withCentre({hx, bar.getCentreY()}));
  }

  g.setFont(uiFont(9.5f));
  g.setColour(palette::textFaint);
  for (const float db : {-48.0f, -36.0f, -24.0f, -12.0f, -6.0f, 0.0f}) {
    const float tx = bar.getX() + bar.getWidth() * juce::jmap(db, -60.0f, 0.0f, 0.0f, 1.0f);
    g.fillRect(juce::Rectangle<float>(1.0f, 3.0f).withPosition(tx, bar.getBottom() + 3.0f));
    g.drawText(juce::String(static_cast<int>(db)),
               juce::Rectangle<float>(30.0f, 12.0f).withCentre({tx, bar.getBottom() + 14.0f}),
               juce::Justification::centred, false);
  }

  g.setFont(uiFont(15.0f, true));
  g.setColour(peak_db_ > -0.1f ? juce::Colour(0xffff5d5d) : palette::text);
  g.drawText(peak_db_ <= -60.0f ? juce::String("-inf") : juce::String(peak_db_, 1),
             readout.removeFromTop(22.0f), juce::Justification::centredRight, false);
  g.setFont(uiFont(9.5f, true, 0.15f));
  g.setColour(palette::textFaint);
  g.drawText("dBFS PEAK", readout, juce::Justification::topRight, false);
}

}  // namespace graupel_ui

//==============================================================================
namespace {
using namespace graupel_ui;

const juce::StringArray kModeNames{"Grain", "Stretch", "Loop", "Spectral"};
const juce::Colour kModeAccents[] = {juce::Colour(0xff6fd3e0), juce::Colour(0xffa995ff),
                                     juce::Colour(0xfff2b65e), juce::Colour(0xffef84b6)};

constexpr int kWidth = 920;
constexpr int kHeight = 590;
constexpr int kLeftColumn = 600;
}  // namespace

GraupelAudioProcessorEditor::GraupelAudioProcessorEditor(GraupelAudioProcessor& processor)
    : AudioProcessorEditor(processor), processor_(processor) {
  setLookAndFeel(&look_and_feel_);
  setOpaque(true);

  core_knobs_.add(&addKnob("position", "Position", Format::Percent,
                           "Where in the Recording Buffer playback reads from."));
  core_knobs_.add(&addKnob("size", "Size", Format::Percent, "Grain or segment length."));
  core_knobs_.add(&addKnob("pitch", "Pitch", Format::Semitones, "Transposition in semitones."));
  core_knobs_.add(&addKnob("density", "Density", Format::Percent, "Grain generation rate."));
  core_knobs_.add(&addKnob("texture", "Texture", Format::Percent, "Grain envelope / diffusion."));

  blend_knobs_.add(&addKnob("dryWet", "Dry / Wet", Format::Percent));
  blend_knobs_.add(&addKnob("stereoSpread", "Spread", Format::Percent, "Stereo spread"));
  blend_knobs_.add(&addKnob("feedback", "Feedback", Format::Percent));
  blend_knobs_.add(&addKnob("reverb", "Reverb", Format::Percent, "Reverberation"));

  io_knobs_.add(&addKnob("pitchFine", "Fine", Format::Fine, "Pitch fine drift"));
  io_knobs_.add(&addKnob("inputGain", "Input", Format::Decibels, "Input gain"));

  granular_knobs_.add(&addKnob("granularOverlap", "Overlap", Format::Percent));
  granular_knobs_.add(&addKnob("granularWindow", "Window", Format::Percent, "Window shape"));
  granular_knobs_.add(&addKnob("granularSpread", "Grain Spread", Format::Percent));

  spectral_knobs_.add(&addKnob("spectralQuantization", "Quantize", Format::Percent, "Quantization"));
  spectral_knobs_.add(&addKnob("spectralRefresh", "Refresh", Format::Percent, "Refresh rate"));
  spectral_knobs_.add(&addKnob("spectralPhase", "Phase Rand", Format::Percent, "Phase randomization"));
  spectral_knobs_.add(&addKnob("spectralWarp", "Warp", Format::Percent));

  for (int i = 0; i < kModeNames.size(); ++i) {
    auto* button = mode_buttons_.add(new juce::TextButton(kModeNames[i].toUpperCase()));
    int edges = 0;
    if (i > 0) edges |= juce::Button::ConnectedOnLeft;
    if (i < kModeNames.size() - 1) edges |= juce::Button::ConnectedOnRight;
    button->setConnectedEdges(edges);
    button->onClick = [this, i] { mode_attachment_->setValueAsCompleteGesture(static_cast<float>(i)); };
    addAndMakeVisible(button);
  }

  addCombo(quality_, quality_label_, "quality", "Quality",
           "Recording Buffer length and channel count.");
  addCombo(fidelity_, fidelity_label_, "fidelity", "Fidelity",
           "Authentic models the hardware signal path. HQ uses cleaner glass; duration does not change.");

  freeze_.setClickingTogglesState(true);
  freeze_.getProperties().set("freeze", true);
  freeze_.setTooltip("Latch the Recording Buffer; no new input is captured while Frozen.");
  summed_input_.setTooltip("Feed the effect a summed mono input without changing buffer Quality.");
  addAndMakeVisible(freeze_);
  addAndMakeVisible(summed_input_);
  freeze_attachment_ = std::make_unique<ButtonAttachment>(processor_.parameters, "freeze", freeze_);
  summed_attachment_ = std::make_unique<ButtonAttachment>(processor_.parameters, "summedInput", summed_input_);

  addAndMakeVisible(meter_);

  mode_attachment_ = std::make_unique<juce::ParameterAttachment>(
      *processor_.parameters.getParameter("mode"),
      [this](float value) { setMode(juce::roundToInt(value)); });
  mode_attachment_->sendInitialUpdate();

  setSize(kWidth, kHeight);
  timerCallback();
  startTimerHz(30);
}

GraupelAudioProcessorEditor::~GraupelAudioProcessorEditor() {
  setLookAndFeel(nullptr);
}

graupel_ui::Knob& GraupelAudioProcessorEditor::addKnob(const char* parameter, const char* name,
                                                       Format format, const juce::String& tooltip) {
  auto* knob = knobs_.add(new Knob(processor_.parameters, parameter, name, format, tooltip));
  addAndMakeVisible(knob);
  return *knob;
}

void GraupelAudioProcessorEditor::addCombo(juce::ComboBox& combo, juce::Label& label,
                                           const char* parameter, const char* name,
                                           const juce::String& tooltip) {
  if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(processor_.parameters.getParameter(parameter)))
    combo.addItemList(choice->choices, 1);
  combo.setTooltip(tooltip);
  addAndMakeVisible(combo);

  label.setText(juce::String(name).toUpperCase(), juce::dontSendNotification);
  label.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f).withStyle("Bold")).withExtraKerningFactor(0.12f));
  label.setColour(juce::Label::textColourId, palette::textDim);
  label.setBorderSize({});
  addAndMakeVisible(label);

  combo_attachments_.push_back(std::make_unique<ComboAttachment>(processor_.parameters, parameter, combo));
}

void GraupelAudioProcessorEditor::setMode(int mode) {
  mode_ = juce::jlimit(0, 3, mode);
  look_and_feel_.setAccent(kModeAccents[mode_]);
  for (int i = 0; i < mode_buttons_.size(); ++i)
    mode_buttons_[i]->setToggleState(i == mode_, juce::dontSendNotification);
  for (auto* knob : granular_knobs_) knob->setVisible(mode_ == 0);
  for (auto* knob : spectral_knobs_) knob->setVisible(mode_ == 3);
  extras_panel_.title = kModeNames[mode_].toUpperCase() + " EXTRAS";
  repaint();
}

void GraupelAudioProcessorEditor::timerCallback() {
  meter_.setLevel(juce::jlimit(0.0f, 1.0f, processor_.meterPeak()));

  const auto status = processor_.isBufferEmpty() ? BufferStatus::Empty
                    : processor_.isFrozen()      ? BufferStatus::Frozen
                                                 : BufferStatus::Capturing;
  if (status != status_) {
    status_ = status;
    repaint(status_area_);
  }
}

void GraupelAudioProcessorEditor::drawPanel(juce::Graphics& g, const Panel& panel) const {
  const auto bounds = panel.bounds.toFloat();
  g.setColour(palette::panel);
  g.fillRoundedRectangle(bounds, 10.0f);
  g.setColour(palette::panelBorder);
  g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

  if (panel.title.isEmpty()) return;
  g.setColour(look_and_feel_.accent());
  g.fillEllipse(bounds.getX() + 14.0f, bounds.getY() + 15.0f, 6.0f, 6.0f);
  g.setColour(palette::textDim);
  g.setFont(juce::Font(juce::FontOptions{}.withHeight(11.0f).withStyle("Bold")).withExtraKerningFactor(0.16f));
  g.drawText(panel.title, panel.bounds.getX() + 26, panel.bounds.getY() + 9, 240, 18,
             juce::Justification::centredLeft, false);
}

void GraupelAudioProcessorEditor::paint(juce::Graphics& g) {
  const auto bounds = getLocalBounds().toFloat();
  g.setGradientFill(juce::ColourGradient(palette::backgroundTop, 0.0f, 0.0f,
                                         palette::background, 0.0f, bounds.getHeight(), false));
  g.fillAll();

  // Scattered "hail" specks behind the header.
  juce::Random random(0x6a7e1);
  for (int i = 0; i < 90; ++i) {
    const float x = random.nextFloat() * bounds.getWidth();
    const float y = random.nextFloat() * 110.0f;
    const float r = 0.6f + random.nextFloat() * 1.4f;
    g.setColour(palette::frost.withAlpha((0.03f + random.nextFloat() * 0.10f) * (1.0f - y / 110.0f)));
    g.fillEllipse(x, y, r, r);
  }

  // Title block.
  auto title = header_;
  g.setColour(palette::text);
  g.setFont(juce::Font(juce::FontOptions{}.withHeight(30.0f).withStyle("Bold")).withExtraKerningFactor(0.32f));
  g.drawText("GRAUPEL", title.removeFromTop(34), juce::Justification::bottomLeft, false);
  g.setColour(look_and_feel_.accent());
  g.fillRoundedRectangle(static_cast<float>(header_.getX()), static_cast<float>(title.getY() + 5), 28.0f, 2.0f, 1.0f);
  g.setColour(palette::textFaint);
  g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f).withStyle("Bold")).withExtraKerningFactor(0.22f));
  g.drawText("GRANULAR TEXTURE PROCESSOR", title.withTrimmedLeft(36).withTrimmedTop(-2),
             juce::Justification::centredLeft, false);

  drawPanel(g, core_panel_);
  drawPanel(g, blend_panel_);
  drawPanel(g, io_panel_);
  drawPanel(g, extras_panel_);
  drawPanel(g, {{}, footer_});

  if (mode_ == 1 || mode_ == 2) {
    g.setColour(palette::textFaint);
    g.setFont(juce::Font(juce::FontOptions{}.withHeight(13.0f)));
    g.drawFittedText(kModeNames[mode_] + " mode has no\nadditional controls.",
                     extras_message_area_, juce::Justification::centred, 2);
  }

  // Buffer status pill.
  const auto pill = status_area_.toFloat();
  g.setColour(palette::panel);
  g.fillRoundedRectangle(pill, 7.0f);
  g.setColour(palette::panelBorder);
  g.drawRoundedRectangle(pill.reduced(0.5f), 7.0f, 1.0f);

  juce::String state = "Empty";
  juce::Colour led = palette::textFaint;
  if (status_ == BufferStatus::Capturing) { state = "Capturing input"; led = juce::Colour(0xff5fd98a); }
  if (status_ == BufferStatus::Frozen)    { state = "Frozen  /  output"; led = palette::frost; }

  auto content = status_area_.reduced(14, 0);
  const auto dot = juce::Rectangle<float>(8.0f, 8.0f).withCentre(content.removeFromLeft(8).getCentre().toFloat());
  if (status_ != BufferStatus::Empty) {
    g.setColour(led.withAlpha(0.25f));
    g.fillEllipse(dot.expanded(4.0f));
  }
  g.setColour(led);
  g.fillEllipse(dot);
  content.removeFromLeft(10);
  g.setColour(palette::textFaint);
  g.setFont(juce::Font(juce::FontOptions{}.withHeight(10.5f).withStyle("Bold")).withExtraKerningFactor(0.14f));
  g.drawText("BUFFER", content.removeFromLeft(56), juce::Justification::centredLeft, false);
  g.setColour(palette::text);
  g.setFont(juce::Font(juce::FontOptions{}.withHeight(13.0f)));
  g.drawText(state, content, juce::Justification::centredLeft, true);
}

void GraupelAudioProcessorEditor::layoutKnobRow(juce::Rectangle<int> area,
                                                const juce::Array<graupel_ui::Knob*>& knobs) {
  if (knobs.isEmpty()) return;
  const int width = area.getWidth() / knobs.size();
  for (auto* knob : knobs) knob->setBounds(area.removeFromLeft(width).reduced(2, 0));
}

void GraupelAudioProcessorEditor::resized() {
  auto area = getLocalBounds().reduced(18);

  header_ = area.removeFromTop(54);
  {
    auto combos = header_.withTrimmedLeft(header_.getWidth() - 330).withTrimmedTop(4);
    auto qualityColumn = combos.removeFromLeft(170);
    combos.removeFromLeft(14);
    quality_label_.setBounds(qualityColumn.removeFromTop(16).withTrimmedLeft(2));
    quality_.setBounds(qualityColumn.removeFromTop(32));
    fidelity_label_.setBounds(combos.removeFromTop(16).withTrimmedLeft(2));
    fidelity_.setBounds(combos.removeFromTop(32));
  }
  area.removeFromTop(12);

  auto modeBar = area.removeFromTop(40);
  mode_bar_ = modeBar.removeFromLeft(kLeftColumn);
  modeBar.removeFromLeft(12);
  status_area_ = modeBar;
  {
    auto buttons = mode_bar_;
    const int width = buttons.getWidth() / mode_buttons_.size();
    for (auto* button : mode_buttons_) button->setBounds(buttons.removeFromLeft(width).withTrimmedRight(2));
  }
  area.removeFromTop(12);

  footer_ = area.removeFromBottom(66);
  area.removeFromBottom(12);

  auto left = area.removeFromLeft(kLeftColumn);
  area.removeFromLeft(12);
  extras_panel_.bounds = area;

  core_panel_.bounds = left.removeFromTop(196);
  left.removeFromTop(12);
  blend_panel_.bounds = left.removeFromLeft(396);
  left.removeFromLeft(12);
  io_panel_.bounds = left;

  auto content = [](const Panel& panel) { return panel.bounds.withTrimmedTop(32).reduced(10, 10); };
  layoutKnobRow(content(core_panel_), core_knobs_);
  layoutKnobRow(content(blend_panel_), blend_knobs_);
  layoutKnobRow(content(io_panel_), io_knobs_);

  extras_message_area_ = content(extras_panel_);
  for (const auto* group : {&granular_knobs_, &spectral_knobs_}) {
    const int rows = (group->size() + 1) / 2;
    auto grid = extras_message_area_;
    const int rowHeight = grid.getHeight() / rows;
    for (int row = 0; row < rows; ++row) {
      auto line = grid.removeFromTop(rowHeight).reduced(0, 6);
      const int first = row * 2;
      const int count = juce::jmin(2, group->size() - first);
      const int cell = line.getWidth() / 2;
      if (count == 1) line = line.withSizeKeepingCentre(cell, line.getHeight());
      for (int i = 0; i < count; ++i)
        (*group)[first + i]->setBounds(line.removeFromLeft(cell).reduced(4, 0));
    }
  }

  auto footer = footer_.reduced(14, 12);
  freeze_.setBounds(footer.removeFromLeft(150));
  footer.removeFromLeft(20);
  summed_input_.setBounds(footer.removeFromLeft(150).withSizeKeepingCentre(150, 28));
  footer.removeFromLeft(20);
  meter_.setBounds(footer.withSizeKeepingCentre(footer.getWidth(), 38));
}
