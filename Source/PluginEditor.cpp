#include "PluginEditor.h"

namespace {
const juce::Colour kBody(0xff1b1b1d), kPanel(0xff252528), kText(0xffe9e4da), kDim(0xff8d8a84);
const juce::Colour cDco(0xffe8833a), cHpf(0xffe8c547), cVcf(0xffd9534f), cEnv(0xff4fa3a5), cLfo(0xff9bc25b),
    cCho(0xff7d8cc4), cChar(0xffb0aaa0), cDrive(0xffd9734f), cTape(0xffc9a46b), cCloud(0xff8fb7d9);
constexpr int kW = 1010, kPad = 12, kHeader = 26, kTop = 64, kFaderH = 172;
constexpr int kRowH = kHeader + kFaderH + 2 * kPad;
}  // namespace

// ---------------------------------------------------------------------------
NimbusLookAndFeel::NimbusLookAndFeel() {
  setColour(juce::Slider::textBoxTextColourId, kText);
  setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
  setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
  setColour(juce::Slider::textBoxHighlightColourId, cDco.withAlpha(0.5f));
  setColour(juce::Label::textColourId, kText);
  setColour(juce::ComboBox::backgroundColourId, kPanel.brighter(0.08f));
  setColour(juce::ComboBox::outlineColourId, kDim.withAlpha(0.4f));
  setColour(juce::ComboBox::textColourId, kText);
  setColour(juce::ComboBox::arrowColourId, kText);
  setColour(juce::PopupMenu::backgroundColourId, kPanel);
  setColour(juce::PopupMenu::textColourId, kText);
  setColour(juce::PopupMenu::headerTextColourId, kDim);
  setColour(juce::PopupMenu::highlightedBackgroundColourId, cDco.withAlpha(0.7f));
  setColour(juce::AlertWindow::backgroundColourId, kPanel);
  setColour(juce::AlertWindow::textColourId, kText);
  setColour(juce::TextEditor::backgroundColourId, kBody);
  setColour(juce::TextEditor::textColourId, kText);
  setColour(juce::TextButton::buttonOnColourId, cDco);
}

void NimbusLookAndFeel::drawLinearSlider(juce::Graphics & g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle, juce::Slider & s) {
  const auto accent = s.findColour(juce::Slider::thumbColourId);
  const float cx = x + w * 0.5f;
  g.setColour(juce::Colours::black.withAlpha(0.6f));
  g.fillRoundedRectangle(cx - 2.5f, (float)y, 5.f, (float)h, 2.5f);
  g.setColour(kDim.withAlpha(0.5f));
  for (int i = 0; i <= 10; i++) {
    const float ty = y + h * i / 10.f;
    const float len = (i % 5 == 0) ? 8.f : 4.f;
    g.drawLine(cx - 9.f - len, ty, cx - 9.f, ty, 1.f);
    g.drawLine(cx + 9.f, ty, cx + 9.f + len, ty, 1.f);
  }
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
  const bool led = b.getProperties().getWithDefault("led", false);
  g.setColour(b.getToggleState() ? accent : kPanel.brighter(over ? 0.25f : 0.1f));
  g.fillRoundedRectangle(r, 3.f);
  g.setColour(juce::Colours::black.withAlpha(0.5f));
  g.drawRoundedRectangle(r, 3.f, 1.f);
  if (led) {
    g.setColour(b.getToggleState() ? juce::Colour(0xffff5a3c) : juce::Colour(0xff3a2422));
    g.fillEllipse(r.getX() + 6.f, r.getCentreY() - 2.5f, 5.f, 5.f);
  }
}

void NimbusLookAndFeel::drawButtonText(juce::Graphics & g, juce::TextButton & b, bool, bool) {
  const bool led = b.getProperties().getWithDefault("led", false);
  g.setColour(b.getToggleState() ? juce::Colours::black : kText);
  g.setFont(juce::FontOptions(juce::jmin(13.f, b.getHeight() * 0.55f), juce::Font::bold));
  g.drawText(b.getButtonText(), b.getLocalBounds().withTrimmedLeft(led ? 10 : 0), juce::Justification::centred);
}

// power switch drawn as a small pill with an LED
void NimbusLookAndFeel::drawToggleButton(juce::Graphics & g, juce::ToggleButton & b, bool over, bool) {
  auto r = b.getLocalBounds().toFloat().reduced(2.f);
  g.setColour(juce::Colours::black.withAlpha(over ? 0.55f : 0.45f));
  g.fillRoundedRectangle(r, r.getHeight() * 0.5f);
  const float d = r.getHeight() - 6.f;
  g.setColour(b.getToggleState() ? juce::Colour(0xffff5a3c) : juce::Colour(0xff4a3532));
  g.fillEllipse(r.getX() + 4.f, r.getCentreY() - d * 0.5f, d, d);
  g.setColour(juce::Colours::white.withAlpha(0.9f));
  g.setFont(juce::FontOptions(10.f, juce::Font::bold));
  g.drawText(b.getToggleState() ? "ON" : "OFF", r.withTrimmedLeft(d + 6.f), juce::Justification::centred);
}

// ---------------------------------------------------------------------------
Fader::Fader(juce::AudioProcessorValueTreeState & s, const juce::String & id, const juce::String & text) {
  slider.setSliderStyle(juce::Slider::LinearVertical);
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
  addAndMakeVisible(slider);
  label.setText(text, juce::dontSendNotification);
  label.setJustificationType(juce::Justification::centred);
  label.setFont(juce::FontOptions(11.f, juce::Font::bold));
  addAndMakeVisible(label);
  attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(s, id, slider);
}

void Fader::resized() {
  auto r = getLocalBounds();
  label.setBounds(r.removeFromTop(16));
  slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, juce::jmax(20, getWidth() - 6), 16);
  slider.setBounds(r);
}

// ---------------------------------------------------------------------------
ChoiceButtons::ChoiceButtons(juce::AudioProcessorValueTreeState & s, const juce::String & id,
                             juce::StringArray labels, int group, juce::Colour accent)
    : param(s.getParameter(id)) {
  for (int i = 0; i < labels.size(); i++) {
    auto * b = buttons.add(new juce::TextButton(labels[i]));
    b->setRadioGroupId(group);
    b->getProperties().set("led", true);
    b->setColour(juce::TextButton::buttonOnColourId, accent);
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
  for (auto * b : buttons) b->setBounds(r.removeFromTop(h).reduced(0, 2));
}

void ChoiceButtons::timerCallback() {
  const int idx = juce::roundToInt(param->convertFrom0to1(param->getValue()));
  for (int i = 0; i < buttons.size(); i++)
    if (buttons[i]->getToggleState() != (i == idx)) buttons[i]->setToggleState(i == idx, juce::dontSendNotification);
}

// ---------------------------------------------------------------------------
int NimbusEditor::add(const juce::String & id, const juce::String & label, juce::Colour c) {
  auto * f = faders.add(new Fader(proc.apvts, id, label));
  f->slider.setColour(juce::Slider::thumbColourId, c);
  addAndMakeVisible(f);
  return faders.size() - 1;
}

juce::ToggleButton * NimbusEditor::makePower(const juce::String & id) {
  auto * b = powers.add(new juce::ToggleButton());
  addAndMakeVisible(b);
  buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(proc.apvts, id, *b));
  return b;
}

NimbusEditor::NimbusEditor(NimbusProcessor & p) : AudioProcessorEditor(&p), proc(p) {
  setLookAndFeel(&lnf);

  auto range = [this](std::initializer_list<std::pair<const char *, const char *>> list, juce::Colour c) {
    std::vector<int> idx;
    for (auto & kv : list) idx.push_back(add(kv.first, kv.second, c));
    return idx;
  };
  auto choiceCol = [this](const char * id, juce::StringArray labels, juce::Colour c) {
    auto * cb = choices.add(new ChoiceButtons(proc.apvts, id, labels, 1000 + choices.size(), c));
    addAndMakeVisible(cb);
    return cb;
  };

  // ---- row 1: DCO | HPF | VCF | ENV
  {
    std::vector<Section> r;
    r.push_back({"DCO", cDco, 7.f, range({{"pw", "PW"}, {"pwmdep", "PWM"}, {"pwmrate", "RATE"}, {"saw", "SAW"},
                                          {"pulse", "PULSE"}, {"sub", "SUB"}, {"noise", "NOISE"}}, cDco)});
    Section hpf{"HPF", cHpf, 1.7f};
    hpf.column = choiceCol("hpf", {"0", "1", "2", "3"}, cHpf);
    r.push_back(hpf);
    r.push_back({"VCF", cVcf, 4.f, range({{"cutoff", "FREQ"}, {"reso", "RES"}, {"envamt", "ENV"},
                                          {"kybd", "KYBD"}}, cVcf)});
    r.push_back({"ENV", cEnv, 4.f, range({{"attack", "A"}, {"decay", "D"}, {"sustain", "S"}, {"release", "R"}}, cEnv)});
    rows.push_back(r);
  }
  // ---- row 2: LFO | CHORUS | CHARACTER | DRIVE | OUT
  {
    std::vector<Section> r;
    r.push_back({"LFO", cLfo, 4.f, range({{"lfo_rate", "RATE"}, {"lfo_delay", "DELAY"}, {"lfo_pitch", "PITCH"},
                                          {"lfo_vcf", "VCF"}}, cLfo)});
    Section cho{"CHORUS", cCho, 2.f};
    cho.column = choiceCol("chorus", {"OFF", "I", "II", "I+II"}, cCho);
    r.push_back(cho);
    r.push_back({"CHARACTER", cChar, 4.f, range({{"hiss", "HISS"}, {"drift", "DRIFT"}, {"age", "AGE"},
                                                {"spread", "SPREAD"}}, cChar)});
    Section drv{"DRIVE", cDrive, 3.f, range({{"drive", "DRIVE"}, {"drive_tone", "TONE"}, {"drive_mix", "MIX"}}, cDrive)};
    drv.power = makePower("drive_on");
    r.push_back(drv);
    r.push_back({"OUT", kText, 1.2f, range({{"volume", "VOL"}}, kText)});
    rows.push_back(r);
  }
  // ---- row 3: TAPE ECHO | CLOUD
  {
    std::vector<Section> r;
    Section tape{"TAPE ECHO", cTape, 7.f, range({{"tape_time", "TIME"}, {"tape_intens", "INTENS"}, {"tape_echo", "ECHO"},
                                                {"tape_tone", "TONE"}, {"tape_wow", "WOW"}}, cTape)};
    tape.column = choiceCol("tape_heads", {"1", "2", "3", "1+2", "2+3", "1+3", "ALL"}, cTape);
    syncBox.addItemList(proc.apvts.getParameter("tape_sync")->getAllValueStrings(), 1);
    addAndMakeVisible(syncBox);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(proc.apvts, "tape_sync", syncBox);
    tape.columnExtra = &syncBox;
    tape.power = makePower("tape_on");
    r.push_back(tape);

    Section cloud{"CLOUD", cCloud, 9.f, range({{"cloud_act", "ACTIVITY"}, {"cloud_time", "TIME"}, {"cloud_shape", "SHAPE"},
                                              {"cloud_rpt", "REPEATS"}, {"cloud_filter", "FILTER"},
                                              {"cloud_space", "SPACE"}, {"cloud_mix", "MIX"}}, cCloud)};
    cloud.column = choiceCol("cloud_mode", {"MOSAIC", "HAZE", "GLIDE", "REVERSE"}, cCloud);
    freezeButton.setClickingTogglesState(true);
    freezeButton.getProperties().set("led", true);
    freezeButton.setColour(juce::TextButton::buttonOnColourId, cCloud);
    addAndMakeVisible(freezeButton);
    buttonAttachments.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(proc.apvts, "cloud_freeze", freezeButton));
    cloud.columnExtra = &freezeButton;
    cloud.power = makePower("cloud_on");
    r.push_back(cloud);
    rows.push_back(r);
  }

  // ---- preset bar
  presetBox.setTextWhenNothingSelected("(unsaved)");
  presetBox.onChange = [this] {
    const int id = presetBox.getSelectedId();
    if (id > 0 && id - 1 != proc.presets.current) {
      proc.presets.load(id - 1);
      refreshPresetList();
    }
  };
  prevButton.onClick = [this] { step(-1); };
  nextButton.onClick = [this] { step(+1); };
  saveButton.onClick = [this] { savePreset(); };
  deleteButton.onClick = [this] { deletePreset(); };
  folderButton.onClick = [] { PresetManager::userFolder().startAsProcess(); };
  for (auto * b : {&prevButton, &nextButton, &saveButton, &deleteButton, &folderButton}) addAndMakeVisible(b);
  addAndMakeVisible(presetBox);
  refreshPresetList();

  setSize(kW, kTop + 3 * kRowH + 3 * kPad);
  startTimerHz(5);
}

NimbusEditor::~NimbusEditor() { setLookAndFeel(nullptr); }

void NimbusEditor::refreshPresetList() {
  auto & pm = proc.presets;
  presetBox.clear(juce::dontSendNotification);
  presetBox.addSectionHeading("FACTORY");
  auto fn = pm.factoryNames();
  for (int i = 0; i < fn.size(); i++) presetBox.addItem(fn[i], i + 1);
  auto un = pm.userNames();
  if (!un.isEmpty()) {
    presetBox.addSeparator();
    presetBox.addSectionHeading("USER");
    for (int i = 0; i < un.size(); i++) presetBox.addItem(un[i], pm.numFactory() + i + 1);
  }
  lastPresetCount = pm.total();
  if (pm.current >= 0 && pm.current < pm.total()) presetBox.setSelectedId(pm.current + 1, juce::dontSendNotification);
  else presetBox.setSelectedId(0, juce::dontSendNotification);
  deleteButton.setEnabled(pm.current >= 0 && pm.isUser(pm.current));
  lastPresetIndex = pm.current;
}

void NimbusEditor::step(int delta) {
  auto & pm = proc.presets;
  const int n = pm.total();
  if (n == 0) return;
  const int next = ((pm.current < 0 ? 0 : pm.current) + delta + n) % n;
  pm.load(next);
  refreshPresetList();
}

void NimbusEditor::savePreset() {
  auto * aw = new juce::AlertWindow("Save Preset", "Name this sound:", juce::MessageBoxIconType::NoIcon, this);
  aw->setLookAndFeel(&lnf);
  const auto & pm = proc.presets;
  aw->addTextEditor("name", pm.isUser(pm.current) ? pm.currentName : juce::String(), "Name:");
  aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
  aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
  juce::Component::SafePointer<NimbusEditor> safe(this);
  aw->enterModalState(true, juce::ModalCallbackFunction::create([safe, aw](int result) {
                        if (result != 1 || safe == nullptr) return;
                        const auto name = aw->getTextEditorContents("name");
                        if (!safe->proc.presets.saveUser(name))
                          juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Couldn't save",
                                                                 "Give the preset a name and try again.");
                        safe->refreshPresetList();
                      }),
                      true);
}

void NimbusEditor::deletePreset() {
  auto & pm = proc.presets;
  if (!pm.isUser(pm.current)) return;
  const int index = pm.current;
  juce::Component::SafePointer<NimbusEditor> safe(this);
  juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::WarningIcon, "Delete Preset",
                                     "Delete \"" + pm.currentName + "\"? This can't be undone.", "Delete", "Cancel",
                                     this, juce::ModalCallbackFunction::create([safe, index](int result) {
                                       if (result == 1 && safe != nullptr) {
                                         safe->proc.presets.deleteUser(index);
                                         safe->refreshPresetList();
                                       }
                                     }));
}

void NimbusEditor::timerCallback() {
  auto & pm = proc.presets;
  if (pm.current != lastPresetIndex || pm.total() != lastPresetCount) refreshPresetList();
}

void NimbusEditor::paint(juce::Graphics & g) {
  g.fillAll(kBody);
  auto top = getLocalBounds().removeFromTop(50);
  g.setColour(juce::Colour(0xff111112));
  g.fillRect(top);
  g.setColour(kText);
  g.setFont(juce::FontOptions(26.f, juce::Font::bold));
  g.drawText("NIMBUS", top.withTrimmedLeft(kPad + 2), juce::Justification::centredLeft);
  g.setColour(kDim);
  g.setFont(juce::FontOptions(11.f));
  g.drawText("DCO PAD SYNTHESIZER", top.withTrimmedLeft(kPad + 132), juce::Justification::centredLeft);

  const juce::Colour stripe[] = {cDco, cHpf, cVcf, cEnv, cCho, cTape, cCloud};
  const int sw = getWidth() / 7;
  for (int i = 0; i < 7; i++) {
    g.setColour(stripe[i]);
    g.fillRect(i * sw, 50, (i == 6 ? getWidth() - i * sw : sw), 4);
  }

  for (auto & row : rows)
    for (auto & s : row) {
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
  // preset bar
  auto bar = getLocalBounds().removeFromTop(50).reduced(kPad, 12);
  folderButton.setBounds(bar.removeFromRight(66));
  bar.removeFromRight(6);
  deleteButton.setBounds(bar.removeFromRight(66));
  bar.removeFromRight(6);
  saveButton.setBounds(bar.removeFromRight(56));
  bar.removeFromRight(10);
  nextButton.setBounds(bar.removeFromRight(28));
  presetBox.setBounds(bar.removeFromRight(240));
  prevButton.setBounds(bar.removeFromRight(28));

  // panel rows: each row fills the full width, sections share it by "units"
  const int rowW = getWidth() - 2 * kPad;
  int y = kTop;
  for (auto & row : rows) {
    float units = 0.f;
    for (auto & s : row) units += s.units;
    const float unitW = (float)(rowW - kPad * ((int)row.size() - 1)) / units;
    int x = kPad;
    for (size_t i = 0; i < row.size(); i++) {
      auto & s = row[i];
      const int w = (i + 1 == row.size()) ? (kPad + rowW - x) : juce::roundToInt(s.units * unitW);
      s.bounds = {x, y, w, kRowH};
      x += w + kPad;

      auto inner = s.bounds.withTrimmedTop(kHeader).reduced(kPad / 2, kPad);
      if (s.power) s.power->setBounds(s.bounds.getRight() - 64, s.bounds.getY() + 3, 58, kHeader - 6);
      if (s.column) {
        const int colW = juce::jmin(inner.getWidth(), juce::roundToInt(unitW * 1.7f));
        auto col = s.faders.empty() ? inner : inner.removeFromRight(colW);
        col.reduce(4, 0);
        if (s.columnExtra) {
          s.columnExtra->setBounds(col.removeFromBottom(24));
          col.removeFromBottom(6);
        }
        s.column->setBounds(s.faders.empty() ? col.reduced(0, 14) : col);
      }
      if (!s.faders.empty()) {
        const float fw = (float)inner.getWidth() / (float)s.faders.size();
        for (size_t k = 0; k < s.faders.size(); k++)
          faders[s.faders[k]]->setBounds(inner.getX() + juce::roundToInt(fw * (float)k), inner.getY(),
                                         juce::roundToInt(fw), inner.getHeight());
      }
    }
    y += kRowH + kPad;
  }
}
