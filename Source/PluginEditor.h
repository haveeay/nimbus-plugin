#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class NimbusLookAndFeel : public juce::LookAndFeel_V4 {
 public:
  NimbusLookAndFeel();
  void drawLinearSlider(juce::Graphics &, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                        juce::Slider::SliderStyle, juce::Slider &) override;
  void drawButtonBackground(juce::Graphics &, juce::Button &, const juce::Colour &, bool over, bool down) override;
  void drawButtonText(juce::Graphics &, juce::TextButton &, bool over, bool down) override;
  void drawToggleButton(juce::Graphics &, juce::ToggleButton &, bool over, bool down) override;
};

struct Fader : public juce::Component {
  Fader(juce::AudioProcessorValueTreeState & s, const juce::String & id, const juce::String & label);
  void resized() override;
  juce::Slider slider;
  juce::Label label;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

struct ChoiceButtons : public juce::Component, private juce::Timer {
  ChoiceButtons(juce::AudioProcessorValueTreeState & s, const juce::String & id, juce::StringArray labels, int group,
                juce::Colour accent);
  void resized() override;
  void timerCallback() override;
  juce::RangedAudioParameter * param;
  juce::OwnedArray<juce::TextButton> buttons;
};

// A section of the panel: coloured header, optional power switch, faders and
// an optional side column (selector buttons, sync menu, freeze).
struct Section {
  juce::String title;
  juce::Colour colour;
  float units = 1.f;                       // relative width
  std::vector<int> faders;                 // indices into editor faders
  juce::Component * column = nullptr;      // selector column (right side)
  juce::Component * columnExtra = nullptr; // under the column (sync / freeze)
  juce::ToggleButton * power = nullptr;
  juce::Rectangle<int> bounds;
};

class NimbusEditor : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit NimbusEditor(NimbusProcessor &);
  ~NimbusEditor() override;
  void paint(juce::Graphics &) override;
  void resized() override;

 private:
  void timerCallback() override;
  int add(const juce::String & id, const juce::String & label, juce::Colour c);
  juce::ToggleButton * makePower(const juce::String & id);
  void refreshPresetList();
  void savePreset();
  void deletePreset();
  void step(int delta);

  NimbusProcessor & proc;
  NimbusLookAndFeel lnf;
  juce::OwnedArray<Fader> faders;
  juce::OwnedArray<ChoiceButtons> choices;
  juce::OwnedArray<juce::ToggleButton> powers;
  juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachments;

  juce::ComboBox syncBox;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> syncAttachment;
  juce::TextButton freezeButton{"FREEZE"};

  juce::ComboBox presetBox;
  juce::TextButton prevButton{"<"}, nextButton{">"}, saveButton{"SAVE"}, deleteButton{"DELETE"},
      folderButton{"FOLDER"};
  int lastPresetIndex = -2, lastPresetCount = -1;

  std::vector<std::vector<Section>> rows;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NimbusEditor)
};
