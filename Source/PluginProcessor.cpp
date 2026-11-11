#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace nimbus;
using APVTS = juce::AudioProcessorValueTreeState;

const char * const NimbusProcessor::engineIds[kNumParams] = {
    "pw", "sub", "chorus", "saw", "pulse", "pwmdep", "pwmrate", "noise",
    "hpf", "hiss", "drift", "age", "spread"};

namespace {

juce::NormalisableRange<float> secondsRange(float lo, float hi) {
  juce::NormalisableRange<float> r(lo, hi);
  r.setSkewForCentre(std::sqrt(lo * hi) * 1.5f);
  return r;
}

std::unique_ptr<juce::AudioParameterFloat> pct(const char * id, const char * name, float def) {
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(0.f, 100.f, 0.1f), def,
      juce::AudioParameterFloatAttributes().withLabel("%").withStringFromValueFunction(
          [](float v, int) { return juce::String(juce::roundToInt(v)); }));
}

std::unique_ptr<juce::AudioParameterFloat> secs(const char * id, const char * name, float lo, float hi, float def) {
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, secondsRange(lo, hi), def,
      juce::AudioParameterFloatAttributes().withLabel("s").withStringFromValueFunction([](float v, int) {
        return v < 1.f ? juce::String(juce::roundToInt(v * 1000.f)) + " ms" : juce::String(v, 2) + " s";
      }));
}

struct Preset {
  const char * name;
  std::vector<std::pair<const char *, float>> values;
};

// Anything not listed falls back to the parameter default.
const std::vector<Preset> & presets() {
  static const std::vector<Preset> p = {
      {"Classic Strings", {}},
      {"Shimmer Pad", {{"pw", 40}, {"sub", 20}, {"chorus", 3}, {"saw", 80}, {"pulse", 90}, {"pwmdep", 60},
                       {"pwmrate", 45}, {"hpf", 2}, {"cutoff", 4500}, {"reso", 10}, {"envamt", 25},
                       {"attack", 0.8f}, {"release", 2.f}}},
      {"Warm Brass Pad", {{"saw", 100}, {"pulse", 0}, {"sub", 30}, {"chorus", 1}, {"cutoff", 1200},
                          {"reso", 20}, {"envamt", 60}, {"attack", 0.25f}, {"decay", 1.5f},
                          {"sustain", 60}, {"release", 1.2f}}},
      {"Hollow PWM", {{"saw", 0}, {"pulse", 100}, {"pw", 10}, {"pwmdep", 70}, {"pwmrate", 20},
                      {"sub", 50}, {"chorus", 2}, {"cutoff", 2200}, {"reso", 25}}},
      {"Worn Voice Card", {{"pw", 30}, {"sub", 60}, {"chorus", 2}, {"pulse", 60}, {"pwmdep", 50},
                           {"pwmrate", 20}, {"noise", 8}, {"hpf", 0}, {"hiss", 80}, {"drift", 70},
                           {"age", 85}, {"spread", 30}, {"cutoff", 2000}}},
      {"Night Drive", {{"pulse", 50}, {"sub", 60}, {"chorus", 1}, {"hpf", 0}, {"cutoff", 900},
                       {"reso", 35}, {"envamt", 45}, {"attack", 1.5f}, {"decay", 3.f}, {"sustain", 70},
                       {"release", 3.f}, {"spread", 50}}},
      {"Plain DCO (no character)", {{"chorus", 0}, {"hiss", 0}, {"drift", 0}, {"age", 0}}},
  };
  return p;
}

}  // namespace

APVTS::ParameterLayout NimbusProcessor::createLayout() {
  APVTS::ParameterLayout l;
  // DCO / character (the shared NIMBUS engine)
  l.add(pct("pw", "PW", 25));
  l.add(pct("sub", "Sub", 45));
  l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"chorus", 1}, "Chorus",
                                                     juce::StringArray{"Off", "I", "II", "I+II"}, 2));
  l.add(pct("saw", "Saw", 100));
  l.add(pct("pulse", "Pulse", 70));
  l.add(pct("pwmdep", "PWM Depth", 45));
  l.add(pct("pwmrate", "PWM Rate", 30));
  l.add(pct("noise", "Noise", 0));
  l.add(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{"hpf", 1}, "HPF",
                                                     juce::StringArray{"0", "1", "2", "3"}, 1));
  l.add(pct("hiss", "Hiss", 30));
  l.add(pct("drift", "Drift", 20));
  l.add(pct("age", "Age", 20));
  l.add(pct("spread", "Spread", 0));

  // VCF
  juce::NormalisableRange<float> fr(20.f, 18000.f);
  fr.setSkewForCentre(1000.f);
  l.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"cutoff", 1}, "Cutoff", fr, 2800.f,
      juce::AudioParameterFloatAttributes().withLabel("Hz").withStringFromValueFunction([](float v, int) {
        return v < 1000.f ? juce::String(juce::roundToInt(v)) + " Hz" : juce::String(v / 1000.f, 1) + " kHz";
      })));
  l.add(pct("reso", "Resonance", 15));
  l.add(pct("envamt", "Env Amount", 35));
  l.add(pct("kybd", "Key Track", 50));

  // ENV
  l.add(secs("attack", "Attack", 0.001f, 10.f, 0.6f));
  l.add(secs("decay", "Decay", 0.005f, 10.f, 1.2f));
  l.add(pct("sustain", "Sustain", 85));
  l.add(secs("release", "Release", 0.005f, 12.f, 1.6f));

  l.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"volume", 1}, "Volume", juce::NormalisableRange<float>(-36.f, 6.f, 0.1f), -6.f,
      juce::AudioParameterFloatAttributes().withLabel("dB")));
  return l;
}

NimbusProcessor::NimbusProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NIMBUS", createLayout()) {
  for (int i = 0; i < kNumParams; i++) engineParams[i] = apvts.getRawParameterValue(engineIds[i]);
  cutoff = apvts.getRawParameterValue("cutoff");
  reso = apvts.getRawParameterValue("reso");
  envAmt = apvts.getRawParameterValue("envamt");
  kybd = apvts.getRawParameterValue("kybd");
  attack = apvts.getRawParameterValue("attack");
  decay = apvts.getRawParameterValue("decay");
  sustain = apvts.getRawParameterValue("sustain");
  release = apvts.getRawParameterValue("release");
  volume = apvts.getRawParameterValue("volume");
}

bool NimbusProcessor::isBusesLayoutSupported(const BusesLayout & layouts) const {
  return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NimbusProcessor::prepareToPlay(double sampleRate, int) {
  synth.prepare((float)sampleRate);
  pullParams();
}

void NimbusProcessor::pullParams() {
  for (int i = 0; i < kNumParams; i++) synth.setEngineParam(i, engineParams[i]->load());
  auto & p = synth.params;
  p.cutoff = cutoff->load();
  p.reso = reso->load() * 0.01f;
  p.envAmt = envAmt->load() * 0.01f;
  p.kybd = kybd->load() * 0.01f;
  p.attack = attack->load();
  p.decay = decay->load();
  p.sustain = sustain->load() * 0.01f;
  p.release = release->load();
  p.volumeDb = volume->load();
  synth.updateEnvelopes();
}

void NimbusProcessor::handleMidi(const juce::MidiMessage & m) {
  if (m.isNoteOn()) synth.noteOn(m.getNoteNumber());
  else if (m.isNoteOff()) synth.noteOff(m.getNoteNumber());
  else if (m.isPitchWheel()) synth.setPitchBend((m.getPitchWheelValue() - 8192) / 8192.f);
  else if (m.isSustainPedalOn()) synth.setSustain(true);
  else if (m.isSustainPedalOff()) synth.setSustain(false);
  else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
}

void NimbusProcessor::processBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer & midi) {
  juce::ScopedNoDenormals noDenormals;
  pullParams();

  const int n = buffer.getNumSamples();
  float * L = buffer.getWritePointer(0);
  float * R = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;
  std::vector<float> scratch;
  if (!R) { scratch.resize((size_t)n); R = scratch.data(); }

  int pos = 0;
  for (const auto meta : midi) {
    const int t = juce::jlimit(0, n, meta.samplePosition);
    if (t > pos) { synth.render(L + pos, R + pos, t - pos); pos = t; }
    handleMidi(meta.getMessage());
  }
  if (pos < n) synth.render(L + pos, R + pos, n - pos);
}

int NimbusProcessor::getNumPrograms() { return (int)presets().size(); }

const juce::String NimbusProcessor::getProgramName(int index) {
  return juce::isPositiveAndBelow(index, (int)presets().size()) ? presets()[(size_t)index].name : "";
}

void NimbusProcessor::setCurrentProgram(int index) {
  if (!juce::isPositiveAndBelow(index, (int)presets().size())) return;
  currentProgram = index;
  for (auto * p : getParameters())
    if (auto * rp = dynamic_cast<juce::RangedAudioParameter *>(p))
      rp->setValueNotifyingHost(rp->getDefaultValue());
  for (auto & kv : presets()[(size_t)index].values)
    if (auto * rp = apvts.getParameter(kv.first))
      rp->setValueNotifyingHost(rp->convertTo0to1(kv.second));
}

void NimbusProcessor::getStateInformation(juce::MemoryBlock & destData) {
  auto state = apvts.copyState();
  state.setProperty("program", currentProgram, nullptr);
  if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void NimbusProcessor::setStateInformation(const void * data, int sizeInBytes) {
  if (auto xml = getXmlFromBinary(data, sizeInBytes))
    if (xml->hasTagName(apvts.state.getType())) {
      auto tree = juce::ValueTree::fromXml(*xml);
      currentProgram = tree.getProperty("program", 0);
      apvts.replaceState(tree);
    }
}

juce::AudioProcessorEditor * NimbusProcessor::createEditor() { return new NimbusEditor(*this); }

juce::AudioProcessor * JUCE_CALLTYPE createPluginFilter() { return new NimbusProcessor(); }
