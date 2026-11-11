#include "PluginEditor.h"

namespace {
const juce::Colour kBody(0xff1b1b1d), kPanel(0xff252528), kText(0xffe9e4da), kDim(0xff8d8a84);
const juce::Colour cDco(0xffe8833a), cHpf(0xffe8c547), cVcf(0xffd9534f), cEnv(0xff4fa3a5),
    cCho(0xff7d8cc4), cChar(0xffb0aaa0);
constexpr int kFaderW = 50, kFaderH = 200, kChoiceW = 84, kPad = 14, kHeader = 26;
}  // namespace

// ---------------------------------------------------------------------------
NimbusLookAndFeel::NimbusLookAndFeel() {
  setColour(juce::Slider::textBoxTextColourId, kText);
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
  setColour(juce::Label::textColourId, kText);
  setColour(juce::ComboBox::backgroundColourId, kPanel);
  setColour(juce::ComboBox::outlineColourId, kDim.withAlpha(0.4f));
  setColour(juce::ComboBox::textColourId, kText);
  setColour(juce::PopupMenu::backgroundColourId, kPanel);
  setColour(juce::PopupMenu::textColourId, kText);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, cDco.withAlpha(0.7f));
}

void NimbusLookAndFeel::drawLinearSlider(juce::Graphics & g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle, juce::Slider & s) {
  const auto accent = s.findColour(juce::Slider::thumbColourId);
  const float cx = x + w * 0.5f;
  // slot
  g.setColour(juce::Colours::black.withAlpha(0.6f));
  g.fillRoundedRectangle(cx - 2.5f, (float)y, 5.f, (float)h, 2.5f);
  // scale ticks
  g.setColour(kDim.withAlpha(0.5f));
  for (int i = 0; i <= 10; i++) {
    const float ty = y + h * i / 10.f;
    const float len = (i % 5 == 0) ? 9.f : 5.f;
    g.drawLine(cx - 9.f - len, ty, cx - 9.f, ty, 1.f);
    g.drawLine(cx + 9.f, ty, cx + 9.f + len, ty, 1.f);
  }
  // cap
  const juce::Rectangle<float> cap(cx - 11.f, pos - 7.f, 22.f, 14.f);
  g.setColour(juce::Colour(0xfff1ede4));
  g.fillRoundedRectangle(cap, 2.f);
  g.setColour(accent);
  g.fillRect(cap.withSizeKeepingCentre(22.f, 2.5f));
  g.setColour(juce::Colours::black.withAlpha(0.35f));
  g.drawRoundedRectangle(cap, 2.f, 1.f);
}

void NimbusLookAndFeel::drawButtonBackground(juce::Graphics & g, juce::Button & b, const juce::Colour &, bool over,
                                             bool) {
  auto r = b.getLocalBounds().toFloat().reduced(1.f);
  const auto accent = b.findColour(juce::TextButton::buttonOnColourId);
  g.setColour(b.getToggleState() ? accent : kPanel.brighter(over ? 0.25f : 0.1f));
  g.fillRoundedRectangle(r, 3.f);
  g.setColour(juce::Colours::black.withAlpha(0.5f));
  g.drawRoundedRectangle(r, 3.f, 1.f);
  // LED
  g.setColour(b.getToggleState() ? juce::Colour(0xffff5a3c) : juce::Colour(0xff3a2422));
  g.fillEllipse(r.getX() + 8.f, r.getCentreY() - 3.f, 6.f, 6.f);
}

void NimbusLookAndFeel::drawButtonText(juce::Graphics & g, juce::TextButton & b, bool, bool) {
  g.setColour(b.getToggleState() ? juce::Colours::black : kText);
  g.setFont(juce::FontOptions(13.f, juce::Font::bold));
  g.drawText(b.getButtonText(), b.getLocalBounds().withTrimmedLeft(14), juce::Justification::centred);
}

// ---------------------------------------------------------------------------
Fader::Fader(juce::AudioProcessorValueTreeState & s, const juce::String & id, const juce::String & text) {
  slider.setSliderStyle(juce::Slider::LinearVertical);
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, kFaderW, 16);
  slider.setPopupDisplayEnabled(false, false, nullptr);
  addAndMakeVisible(slider);
  label.setText(text, juce::dontSendNotification);
  label.setJustificationType(juce::Justification::centred);
  label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
  addAndMakeVisible(label);
  attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s, id, slider);
}

void Fader::resized() {
  auto r = getLocalBounds();
  label.setBounds(r.removeFromTop(16));
  slider.setBounds(r);
}

// ---------------------------------------------------------------------------
ChoiceButtons::ChoiceButtons(juce::AudioProcessorValueTreeState & s, const juce::String & id,
                             juce::StringArray labels, int group)
    : param(s.getParameter(id)) {
  for (int i = 0; i < labels.size(); i++) {
    auto * b = buttons.add(new juce::TextButton(labels[i]));
    b->setClickingTogglesState(false);
    b->setRadioGroupId(group);
    b->onClick = [this, i] {
      param->beginChangeGesture();
      param->setValueNotifyingHost(param->convertTo0to1((float)i));
      param->endChangeGesture();
    };
    addAndMakeVisible(b);
  }
  timerCallback();
  startTimerHz(20);
}

void ChoiceButtons::resized() {
  auto r = getLocalBounds();
  const int h = r.getHeight() / juce::jmax(1, buttons.size());
  for (auto * b : buttons) b->setBounds(r.removeFromTop(h).reduced(0, 3));
}

void ChoiceButtons::timerCallback() {
  const int idx = juce::roundToInt(param->convertFrom0to1(param->getValue()));
  for (int i = 0; i < buttons.size(); i++)
    if (buttons[i]->getToggleState() != (i == idx)) buttons[i]->setToggleState(i == idx, juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
Fader * NimbusEditor::add(const juce::String & id, const juce::String & label) {
  auto * f = faders.add(new Fader(proc.apvts, id, label));
  addAndMakeVisible(f);
  return f;
}

NimbusEditor::NimbusEditor(NimbusProcessor & p) : AudioProcessorEditor(&p), proc(p) {
  setLookAndFeel(&lnf);

  for (auto [id, name] : std::initializer_list<std::pair<const char *, const char *>>{
           {"pw", "PW"}, {"pwmdep", "PWM"}, {"pwmrate", "RATE"}, {"saw", "SAW"}, {"pulse", "PULSE"},
           {"sub", "SUB"}, {"noise", "NOISE"},
           {"cutoff", "FREQ"}, {"reso", "RES"}, {"envamt", "ENV"}, {"kybd", "KYBD"},
           {"attack", "A"}, {"decay", "D"}, {"sustain", "S"}, {"release", "R"},
           {"hiss", "HISS"}, {"drift", "DRIFT"}, {"age", "AGE"}, {"spread", "SPREAD"}, {"volume", "VOL"}})
    add(id, name);

  hpf = std::make_unique<ChoiceButtons>(proc.apvts, "hpf", juce::StringArray{"0", "1", "2", "3"}, 1001);
  chorus = std::make_unique<ChoiceButtons>(proc.apvts, "chorus", juce::StringArray{"OFF", "I", "II", "I+II"}, 1002);
  for (auto * b : hpf->buttons) b->setColour(juce::TextButton::buttonOnColourId, cHpf);
  for (auto * b : chorus->buttons) b->setColour(juce::TextButton::buttonOnColourId, cCho);
  addAndMakeVisible(*hpf);
  addAndMakeVisible(*chorus);

  for (int i = 0; i < proc.getNumPrograms(); i++) presetBox.addItem(proc.getProgramName(i), i + 1);
  presetBox.setTextWhenNothingSelected("Presets");
  presetBox.onChange = [this] {
    const int i = presetBox.getSelectedItemIndex();
    if (i >= 0) proc.setCurrentProgram(i);
  };
  addAndMakeVisible(presetBox);

  // fader accent colours by section
  auto colourise = [this](int from, int to, juce::Colour c) {
    for (int i = from; i < to; i++) faders[i]->slider.setColour(juce::Slider::thumbColourId, c);
  };
  colourise(0, 7, cDco);
  colourise(7, 11, cVcf);
  colourise(11, 15, cEnv);
  colourise(15, 19, cChar);
  colourise(19, 20, kText);

  setSize(2 * kPad + 7 * kFaderW + kChoiceW + 4 * kFaderW + 2 * kPad + 2 * kPad,
          66 + 2 * (kHeader + kFaderH + 2 * kPad) + 2 * kPad);
  startTimerHz(4);
}

NimbusEditor::~NimbusEditor() { setLookAndFeel(nullptr); }

void NimbusEditor::timerCallback() {
  if (presetBox.getSelectedItemIndex() != proc.getCurrentProgram())
    presetBox.setSelectedItemIndex(proc.getCurrentProgram(), juce::dontSendNotification);
}

void NimbusEditor::paint(juce::Graphics & g) {
  g.fillAll(kBody);

  // title bar
  auto top = getLocalBounds().removeFromTop(52);
  g.setColour(juce::Colour(0xff111112));
  g.fillRect(top);
  g.setColour(kText);
  g.setFont(juce::FontOptions(26.f, juce::Font::bold));
  g.drawText("NIMBUS", top.withTrimmedLeft(kPad), juce::Justification::centredLeft);
  g.setColour(kDim);
  g.setFont(juce::FontOptions(12.f));
  g.drawText("DCO POLYPHONIC PAD SYNTHESIZER", top.withTrimmedLeft(kPad + 128), juce::Justification::centredLeft);
  // stripe
  const juce::Colour stripe[] = {cDco, cHpf, cVcf, cEnv, cCho};
  const int sw = getWidth() / 5;
  for (int i = 0; i < 5; i++) {
    g.setColour(stripe[i]);
    g.fillRect(i * sw, 52, (i == 4 ? getWidth() - i * sw : sw), 4);
  }

  for (auto & s : sections) {
    g.setColour(kPanel);
    g.fillRoundedRectangle(s.bounds.toFloat(), 6.f);
    auto head = s.bounds.withHeight(kHeader);
    g.setColour(s.colour);
    g.fillRoundedRectangle(head.toFloat(), 6.f);
    g.fillRect(head.withTrimmedTop(kHeader / 2));
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.setFont(juce::FontOptions(13.f, juce::Font::bold));
    g.drawText(s.title, head, juce::Justification::centred);
  }
}

void NimbusEditor::resized() {
  presetBox.setBounds(getWidth() - kPad - 190, 13, 190, 26);

  sections.clear();
  const int rowH = kHeader + kFaderH + 2 * kPad;
  auto area = getLocalBounds().withTrimmedTop(66).reduced(kPad, 0);
  auto row1 = area.removeFromTop(rowH);
  area.removeFromTop(kPad);
  auto row2 = area.removeFromTop(rowH);

  auto place = [&](juce::Rectangle<int> & row, const juce::String & title, juce::Colour c, int width) {
    auto b = row.removeFromLeft(width);
    row.removeFromLeft(kPad);
    sections.push_back({title, c, b});
    return b.withTrimmedTop(kHeader).reduced(kPad / 2, kPad);
  };
  auto layFaders = [&](juce::Rectangle<int> r, int from, int to) {
    for (int i = from; i < to; i++) faders[i]->setBounds(r.removeFromLeft(kFaderW));
  };

  // row 1: DCO | HPF | VCF
  layFaders(place(row1, "DCO", cDco, 7 * kFaderW + kPad), 0, 7);
  hpf->setBounds(place(row1, "HPF", cHpf, kChoiceW).withTrimmedTop(18).withTrimmedBottom(18));
  layFaders(place(row1, "VCF", cVcf, row1.getWidth()), 7, 11);

  // row 2: ENV | CHORUS | CHARACTER | VOLUME
  layFaders(place(row2, "ENV", cEnv, 4 * kFaderW + kPad), 11, 15);
  chorus->setBounds(place(row2, "CHORUS", cCho, kChoiceW + 24).withTrimmedTop(18).withTrimmedBottom(18));
  layFaders(place(row2, "CHARACTER", cChar, 4 * kFaderW + kPad), 15, 19);
  layFaders(place(row2, "OUT", kText, row2.getWidth()), 19, 20);
}
