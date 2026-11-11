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
};

// One labelled vertical fader bound to a parameter.
struct Fader : public juce::Component {
  Fader(juce::AudioProcessorValueTreeState & s, const juce::String & id, const juce::String & label);
  void resized() override;
  juce::Slider slider;
  juce::Label label;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// Radio buttons bound to a choice parameter (HPF position, chorus mode).
struct ChoiceButtons : public juce::Component, private juce::Timer {
  ChoiceButtons(juce::AudioProcessorValueTreeState & s, const juce::String & id, juce::StringArray labels, int group);
  void resized() override;
  void timerCallback() override;
  juce::RangedAudioParameter * param;
  juce::OwnedArray<juce::TextButton> buttons;
};

struct Section {
  juce::String title;
  juce::Colour colour;
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
  Fader * add(const juce::String & id, const juce::String & label);

  NimbusProcessor & proc;
  NimbusLookAndFeel lnf;
  juce::OwnedArray<Fader> faders;
  std::unique_ptr<ChoiceButtons> hpf, chorus;
  juce::ComboBox presetBox;
  std::vector<Section> sections;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NimbusEditor)
};
