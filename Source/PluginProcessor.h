#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthCore.h"
#include "FxRack.h"

// Factory presets (built in) + user presets (XML files on disk).
class PresetManager {
 public:
  explicit PresetManager(juce::AudioProcessorValueTreeState & s);

  static juce::File userFolder();
  int numFactory() const;
  juce::StringArray factoryNames() const;
  juce::StringArray userNames() const;  // sorted, without extension
  int total() const { return numFactory() + userNames().size(); }
  juce::String nameAt(int index) const;
  bool isUser(int index) const { return index >= numFactory(); }

  void load(int index);
  bool saveUser(const juce::String & name);  // returns false on failure
  bool deleteUser(int index);

  int current = 0;
  juce::String currentName = "Classic Strings";

 private:
  void resetToDefaults();
  juce::AudioProcessorValueTreeState & apvts;
};

class NimbusProcessor : public juce::AudioProcessor {
 public:
  NimbusProcessor();
  ~NimbusProcessor() override = default;

  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override {}
  bool isBusesLayoutSupported(const BusesLayout & layouts) const override;
  void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;

  juce::AudioProcessorEditor * createEditor() override;
  bool hasEditor() const override { return true; }

  const juce::String getName() const override { return "Nimbus"; }
  bool acceptsMidi() const override { return true; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 20.0; }

  // host-visible programs = factory presets
  int getNumPrograms() override { return presets.numFactory(); }
  int getCurrentProgram() override { return presets.current < presets.numFactory() ? presets.current : 0; }
  void setCurrentProgram(int index) override { presets.load(index); }
  const juce::String getProgramName(int index) override { return presets.nameAt(index); }
  void changeProgramName(int, const juce::String &) override {}

  void getStateInformation(juce::MemoryBlock & destData) override;
  void setStateInformation(const void * data, int sizeInBytes) override;

  juce::AudioProcessorValueTreeState apvts;
  PresetManager presets{apvts};
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
  static const char * const engineIds[nimbus::kNumParams];

 private:
  void handleMidi(const juce::MidiMessage & m);
  void pullParams();
  float p(const char * id) const { return apvts.getRawParameterValue(id)->load(); }

  nimbus::Synth synth;
  nimbus::fx::Rack rack;
  std::atomic<float> * engineParams[nimbus::kNumParams] = {};
  bool wasFrozen = false;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NimbusProcessor)
};
