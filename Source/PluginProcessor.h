#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "SynthCore.h"

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
  double getTailLengthSeconds() const override { return 12.0; }

  int getNumPrograms() override;
  int getCurrentProgram() override { return currentProgram; }
  void setCurrentProgram(int index) override;
  const juce::String getProgramName(int index) override;
  void changeProgramName(int, const juce::String &) override {}

  void getStateInformation(juce::MemoryBlock & destData) override;
  void setStateInformation(const void * data, int sizeInBytes) override;

  juce::AudioProcessorValueTreeState apvts;
  static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

  // engine param IDs in nimbus::Engine order
  static const char * const engineIds[nimbus::kNumParams];

 private:
  void handleMidi(const juce::MidiMessage & m);
  void pullParams();

  nimbus::Synth synth;
  std::atomic<float> * engineParams[nimbus::kNumParams] = {};
  std::atomic<float> *cutoff, *reso, *envAmt, *kybd, *attack, *decay, *sustain, *release, *volume;
  int currentProgram = 0;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NimbusProcessor)
};
