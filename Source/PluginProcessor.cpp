#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace nimbus;
using APVTS = juce::AudioProcessorValueTreeState;

const char * const NimbusProcessor::engineIds[kNumParams] = {
    "pw", "sub", "chorus", "saw", "pulse", "pwmdep", "pwmrate", "noise",
    "hpf", "hiss", "drift", "age", "spread"};

namespace {

std::unique_ptr<juce::AudioParameterFloat> pct(const char * id, const char * name, float def) {
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, juce::NormalisableRange<float>(0.f, 100.f, 0.1f), def,
      juce::AudioParameterFloatAttributes().withLabel("%").withStringFromValueFunction(
          [](float v, int) { return juce::String(juce::roundToInt(v)); }));
}

juce::String secondsText(float v) {
  return v < 1.f ? juce::String(juce::roundToInt(v * 1000.f)) + " ms" : juce::String(v, 2) + " s";
}

std::unique_ptr<juce::AudioParameterFloat> secs(const char * id, const char * name, float lo, float hi, float def,
                                                float centre) {
  juce::NormalisableRange<float> r(lo, hi);
  r.setSkewForCentre(centre);
  return std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, r, def,
      juce::AudioParameterFloatAttributes().withLabel("s").withStringFromValueFunction(
          [](float v, int) { return secondsText(v); }));
}

std::unique_ptr<juce::AudioParameterChoice> choice(const char * id, const char * name, juce::StringArray items,
                                                   int def) {
  return std::make_unique<juce::AudioParameterChoice>(juce::ParameterID{id, 1}, name, items, def);
}

std::unique_ptr<juce::AudioParameterBool> toggle(const char * id, const char * name, bool def) {
  return std::make_unique<juce::AudioParameterBool>(juce::ParameterID{id, 1}, name, def);
}

struct Preset {
  const char * name;
  std::vector<std::pair<const char *, float>> values;  // anything not listed = default
};

const std::vector<Preset> & factory() {
  static const std::vector<Preset> p = {
      {"Classic Strings", {}},
      {"Tape Dream Pad", {{"chorus", 1}, {"cutoff", 2400}, {"attack", 1.0f}, {"release", 2.5f},
                          {"tape_on", 1}, {"tape_heads", 6}, {"tape_time", 45}, {"tape_intens", 50},
                          {"tape_echo", 45}, {"tape_wow", 40}, {"tape_tone", 40}}},
      {"Cloud Strings", {{"release", 3.f}, {"cloud_on", 1}, {"cloud_mode", 1}, {"cloud_act", 60},
                         {"cloud_time", 65}, {"cloud_shape", 90}, {"cloud_rpt", 45}, {"cloud_filter", 65},
                         {"cloud_space", 70}, {"cloud_mix", 50}}},
      {"Mosaic Keys", {{"attack", 0.005f}, {"decay", 0.8f}, {"sustain", 40}, {"release", 0.6f}, {"envamt", 45},
                       {"cloud_on", 1}, {"cloud_mode", 0}, {"cloud_act", 45}, {"cloud_time", 40},
                       {"cloud_shape", 30}, {"cloud_mix", 45}, {"cloud_space", 40}}},
      {"Shimmer Pad", {{"pw", 40}, {"sub", 20}, {"chorus", 3}, {"saw", 80}, {"pulse", 90}, {"pwmdep", 60},
                       {"pwmrate", 45}, {"hpf", 2}, {"cutoff", 4500}, {"reso", 10}, {"envamt", 25},
                       {"attack", 0.8f}, {"release", 2.f}, {"cloud_on", 1}, {"cloud_mode", 2},
                       {"cloud_mix", 30}, {"cloud_space", 60}}},
      {"Warm Brass Pad", {{"saw", 100}, {"pulse", 0}, {"sub", 30}, {"chorus", 1}, {"cutoff", 1200},
                          {"reso", 20}, {"envamt", 60}, {"attack", 0.25f}, {"decay", 1.5f},
                          {"sustain", 60}, {"release", 1.2f}, {"drive_on", 1}, {"drive", 35}}},
      {"Hollow PWM", {{"saw", 0}, {"pulse", 100}, {"pw", 10}, {"pwmdep", 70}, {"pwmrate", 20}, {"sub", 50},
                      {"cutoff", 2200}, {"reso", 25}, {"tape_on", 1}, {"tape_sync", 4}, {"tape_heads", 2},
                      {"tape_intens", 35}, {"tape_echo", 30}}},
      {"Worn Voice Card", {{"pw", 30}, {"sub", 60}, {"pulse", 60}, {"pwmdep", 50}, {"pwmrate", 20},
                           {"noise", 8}, {"hpf", 0}, {"hiss", 80}, {"drift", 70}, {"age", 85}, {"spread", 30},
                           {"cutoff", 2000}, {"drive_on", 1}, {"drive", 40}, {"tape_on", 1}, {"tape_wow", 85},
                           {"tape_tone", 25}, {"tape_intens", 40}}},
      {"Night Drive", {{"pulse", 50}, {"sub", 60}, {"chorus", 1}, {"hpf", 0}, {"cutoff", 900}, {"reso", 35},
                       {"envamt", 45}, {"attack", 1.5f}, {"decay", 3.f}, {"sustain", 70}, {"release", 3.f},
                       {"spread", 50}, {"lfo_rate", 0.3f}, {"lfo_vcf", 15}, {"tape_on", 1},
                       {"tape_sync", 5}, {"tape_heads", 0}, {"tape_intens", 55}, {"tape_echo", 35}}},
      {"Reverse Memory", {{"attack", 0.6f}, {"release", 2.f}, {"cloud_on", 1}, {"cloud_mode", 3},
                          {"cloud_act", 40}, {"cloud_time", 70}, {"cloud_mix", 55}, {"cloud_space", 60},
                          {"tape_on", 1}, {"tape_intens", 30}, {"tape_echo", 25}}},
      {"Vibrato Strings", {{"lfo_rate", 5.5f}, {"lfo_delay", 0.6f}, {"lfo_pitch", 25}, {"chorus", 2}}},
      {"Plain DCO", {{"chorus", 0}, {"hiss", 0}, {"drift", 0}, {"age", 0}}},
  };
  return p;
}

}  // namespace

// ===========================================================================
PresetManager::PresetManager(APVTS & s) : apvts(s) {}

juce::File PresetManager::userFolder() {
#if JUCE_MAC
  auto f = juce::File::getSpecialLocation(juce::File::userHomeDirectory).getChildFile("Library/Audio/Presets/Javi/Nimbus");
#else
  auto f = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Javi/Nimbus/Presets");
#endif
  f.createDirectory();
  return f;
}

int PresetManager::numFactory() const { return (int)factory().size(); }

juce::StringArray PresetManager::factoryNames() const {
  juce::StringArray a;
  for (auto & p : factory()) a.add(p.name);
  return a;
}

juce::StringArray PresetManager::userNames() const {
  juce::StringArray a;
  for (auto & f : userFolder().findChildFiles(juce::File::findFiles, false, "*.nimbuspreset"))
    a.add(f.getFileNameWithoutExtension());
  a.sortNatural();
  return a;
}

juce::String PresetManager::nameAt(int index) const {
  if (index < 0) return {};
  if (index < numFactory()) return factory()[(size_t)index].name;
  return userNames()[index - numFactory()];
}

void PresetManager::resetToDefaults() {
  for (auto * p : apvts.processor.getParameters())
    if (auto * rp = dynamic_cast<juce::RangedAudioParameter *>(p)) rp->setValueNotifyingHost(rp->getDefaultValue());
}

void PresetManager::load(int index) {
  if (index < 0 || index >= total()) return;
  if (index < numFactory()) {
    resetToDefaults();
    for (auto & kv : factory()[(size_t)index].values)
      if (auto * rp = apvts.getParameter(kv.first)) rp->setValueNotifyingHost(rp->convertTo0to1(kv.second));
  } else {
    auto file = userFolder().getChildFile(nameAt(index) + ".nimbuspreset");
    if (auto xml = juce::XmlDocument::parse(file)) {
      auto tree = juce::ValueTree::fromXml(*xml);
      if (tree.hasType(apvts.state.getType())) {
        resetToDefaults();  // presets from older versions get sane values for newer params
        for (int i = 0; i < tree.getNumChildren(); i++) {
          auto c = tree.getChild(i);
          if (auto * rp = apvts.getParameter(c["id"].toString()))
            rp->setValueNotifyingHost(rp->convertTo0to1((float)c["value"]));
        }
      }
    }
  }
  current = index;
  currentName = nameAt(index);
}

bool PresetManager::saveUser(const juce::String & rawName) {
  const auto name = juce::File::createLegalFileName(rawName.trim());
  if (name.isEmpty()) return false;
  auto state = apvts.copyState();
  state.setProperty("presetName", name, nullptr);
  auto xml = state.createXml();
  if (!xml || !xml->writeTo(userFolder().getChildFile(name + ".nimbuspreset"))) return false;
  current = numFactory() + userNames().indexOf(name);
  currentName = name;
  return true;
}

bool PresetManager::deleteUser(int index) {
  if (!isUser(index) || index >= total()) return false;
  const bool ok = userFolder().getChildFile(nameAt(index) + ".nimbuspreset").deleteFile();
  if (ok) { current = -1; currentName = {}; }
  return ok;
}

// ===========================================================================
APVTS::ParameterLayout NimbusProcessor::createLayout() {
  APVTS::ParameterLayout l;
  // DCO / character (shared NIMBUS engine)
  l.add(pct("pw", "PW", 25));
  l.add(pct("sub", "Sub", 45));
  l.add(choice("chorus", "Chorus", {"Off", "I", "II", "I+II"}, 2));
  l.add(pct("saw", "Saw", 100));
  l.add(pct("pulse", "Pulse", 70));
  l.add(pct("pwmdep", "PWM Depth", 45));
  l.add(pct("pwmrate", "PWM Rate", 30));
  l.add(pct("noise", "Noise", 0));
  l.add(choice("hpf", "HPF", {"0", "1", "2", "3"}, 1));
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
  l.add(secs("attack", "Attack", 0.001f, 10.f, 0.6f, 0.5f));
  l.add(secs("decay", "Decay", 0.005f, 10.f, 1.2f, 0.6f));
  l.add(pct("sustain", "Sustain", 85));
  l.add(secs("release", "Release", 0.005f, 12.f, 1.6f, 0.8f));

  // LFO
  juce::NormalisableRange<float> lr(0.1f, 20.f);
  lr.setSkewForCentre(2.f);
  l.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"lfo_rate", 1}, "LFO Rate", lr, 4.f,
      juce::AudioParameterFloatAttributes().withLabel("Hz").withStringFromValueFunction(
          [](float v, int) { return juce::String(v, v < 10.f ? 2 : 1) + " Hz"; })));
  l.add(secs("lfo_delay", "LFO Delay", 0.f, 5.f, 0.f, 1.f));
  l.add(pct("lfo_pitch", "LFO Pitch", 0));
  l.add(pct("lfo_vcf", "LFO VCF", 0));

  l.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"volume", 1}, "Volume", juce::NormalisableRange<float>(-36.f, 6.f, 0.1f), -6.f,
      juce::AudioParameterFloatAttributes().withLabel("dB")));

  // DRIVE
  l.add(toggle("drive_on", "Drive On", false));
  l.add(pct("drive", "Drive", 25));
  l.add(pct("drive_tone", "Drive Tone", 60));
  l.add(pct("drive_mix", "Drive Mix", 100));

  // TAPE ECHO
  l.add(toggle("tape_on", "Tape Echo On", false));
  l.add(pct("tape_time", "Echo Time", 35));
  l.add(choice("tape_sync", "Echo Sync", {"Free", "1/16", "1/8T", "1/8", "1/8D", "1/4"}, 0));
  l.add(choice("tape_heads", "Echo Heads", {"1", "2", "3", "1+2", "2+3", "1+3", "ALL"}, 6));
  l.add(pct("tape_intens", "Echo Intensity", 45));
  l.add(pct("tape_echo", "Echo Level", 40));
  l.add(pct("tape_tone", "Echo Tone", 50));
  l.add(pct("tape_wow", "Wow & Flutter", 30));

  // CLOUD
  l.add(toggle("cloud_on", "Cloud On", false));
  l.add(choice("cloud_mode", "Cloud Mode", {"MOSAIC", "HAZE", "GLIDE", "REVERSE"}, 0));
  l.add(pct("cloud_act", "Cloud Activity", 50));
  l.add(pct("cloud_time", "Cloud Time", 50));
  l.add(pct("cloud_shape", "Cloud Shape", 60));
  l.add(pct("cloud_rpt", "Cloud Repeats", 30));
  l.add(pct("cloud_filter", "Cloud Filter", 70));
  l.add(pct("cloud_space", "Cloud Space", 40));
  l.add(pct("cloud_mix", "Cloud Mix", 40));
  l.add(toggle("cloud_freeze", "Cloud Freeze", false));
  return l;
}

NimbusProcessor::NimbusProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NIMBUS", createLayout()) {
  for (int i = 0; i < kNumParams; i++) engineParams[i] = apvts.getRawParameterValue(engineIds[i]);
}

bool NimbusProcessor::isBusesLayoutSupported(const BusesLayout & layouts) const {
  return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void NimbusProcessor::prepareToPlay(double sampleRate, int) {
  synth.prepare((float)sampleRate);
  rack.prepare((float)sampleRate);
  pullParams();
}

void NimbusProcessor::pullParams() {
  for (int i = 0; i < kNumParams; i++) synth.setEngineParam(i, engineParams[i]->load());
  auto & sp = synth.params;
  sp.cutoff = p("cutoff");
  sp.reso = p("reso") * 0.01f;
  sp.envAmt = p("envamt") * 0.01f;
  sp.kybd = p("kybd") * 0.01f;
  sp.attack = p("attack");
  sp.decay = p("decay");
  sp.sustain = p("sustain") * 0.01f;
  sp.release = p("release");
  sp.volumeDb = p("volume");
  sp.lfoRate = p("lfo_rate");
  sp.lfoDelay = p("lfo_delay");
  sp.lfoPitch = p("lfo_pitch") * 0.01f;
  sp.lfoVcf = p("lfo_vcf") * 0.01f;
  synth.updateEnvelopes();

  auto & d = rack.drive;
  d.on = p("drive_on") > 0.5f;
  d.amount = p("drive") * 0.01f;
  d.tone = p("drive_tone") * 0.01f;
  d.mix = p("drive_mix") * 0.01f;

  auto & t = rack.tape;
  const bool tapeOn = p("tape_on") > 0.5f;
  if (tapeOn && !t.on) t.clear();
  t.on = tapeOn;
  t.time = p("tape_time") * 0.01f;
  t.sync = (int)p("tape_sync");
  t.heads = (int)p("tape_heads");
  t.intensity = p("tape_intens") * 0.01f;
  t.echo = p("tape_echo") * 0.01f;
  t.tone = p("tape_tone") * 0.01f;
  t.wow = p("tape_wow") * 0.01f;

  auto & c = rack.cloud;
  const bool cloudOn = p("cloud_on") > 0.5f;
  if (cloudOn && !c.on) c.clear();
  c.on = cloudOn;
  c.mode = (int)p("cloud_mode");
  c.activity = p("cloud_act") * 0.01f;
  c.time = p("cloud_time") * 0.01f;
  c.shape = p("cloud_shape") * 0.01f;
  c.repeats = p("cloud_rpt") * 0.01f;
  c.filter = p("cloud_filter") * 0.01f;
  c.space = p("cloud_space") * 0.01f;
  c.mix = p("cloud_mix") * 0.01f;
  c.freeze = p("cloud_freeze") > 0.5f;
  rack.update();
}

void NimbusProcessor::handleMidi(const juce::MidiMessage & m) {
  if (m.isNoteOn()) synth.noteOn(m.getNoteNumber());
  else if (m.isNoteOff()) synth.noteOff(m.getNoteNumber());
  else if (m.isPitchWheel()) synth.setPitchBend((float)(m.getPitchWheelValue() - 8192) / 8192.f);
  else if (m.isSustainPedalOn()) synth.setSustain(true);
  else if (m.isSustainPedalOff()) synth.setSustain(false);
  else if (m.isAllNotesOff() || m.isAllSoundOff()) synth.allNotesOff();
}

void NimbusProcessor::processBlock(juce::AudioBuffer<float> & buffer, juce::MidiBuffer & midi) {
  juce::ScopedNoDenormals noDenormals;
  pullParams();
  if (auto * ph = getPlayHead())
    if (auto pos = ph->getPosition())
      if (auto bpm = pos->getBpm()) rack.tape.bpm = (float)*bpm;

  const int n = buffer.getNumSamples();
  if (buffer.getNumChannels() < 2) { buffer.clear(); return; }
  float * L = buffer.getWritePointer(0);
  float * R = buffer.getWritePointer(1);

  int pos = 0;
  for (const auto meta : midi) {
    const int t = juce::jlimit(0, n, meta.samplePosition);
    if (t > pos) { synth.render(L + pos, R + pos, t - pos); pos = t; }
    handleMidi(meta.getMessage());
  }
  if (pos < n) synth.render(L + pos, R + pos, n - pos);

  rack.process(L, R, n);
  for (int i = 0; i < n; i++) { L[i] = fx::sat(L[i]); R[i] = fx::sat(R[i]); }  // safety limiter
}

void NimbusProcessor::getStateInformation(juce::MemoryBlock & destData) {
  auto state = apvts.copyState();
  state.setProperty("presetIndex", presets.current, nullptr);
  state.setProperty("presetName", presets.currentName, nullptr);
  if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void NimbusProcessor::setStateInformation(const void * data, int sizeInBytes) {
  if (auto xml = getXmlFromBinary(data, sizeInBytes))
    if (xml->hasTagName(apvts.state.getType())) {
      auto tree = juce::ValueTree::fromXml(*xml);
      presets.current = tree.getProperty("presetIndex", 0);
      presets.currentName = tree.getProperty("presetName", "").toString();
      apvts.replaceState(tree);
      // push every stored value back into its parameter explicitly, so values the tree
      // considers unchanged (e.g. a toggle sitting between states) are restored too
      for (int i = 0; i < tree.getNumChildren(); i++) {
        auto c = tree.getChild(i);
        if (auto * rp = apvts.getParameter(c["id"].toString()))
          rp->setValueNotifyingHost(rp->convertTo0to1((float)c["value"]));
      }
    }
}

juce::AudioProcessorEditor * NimbusProcessor::createEditor() { return new NimbusEditor(*this); }

juce::AudioProcessor * JUCE_CALLTYPE createPluginFilter() { return new NimbusProcessor(); }
