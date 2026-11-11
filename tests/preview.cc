// Desktop preview renderer for the NIMBUS DSP core.
// Plays a pad progression through the exact same Engine code that runs on the
// microKORG2, with a simple ADSR + 2-pole lowpass standing in for the synth's
// own filter/amp sections.
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include "nimbus_dsp.h"

using namespace nimbus;

struct Preset { const char * file; int p[kNumParams]; float cutoff; };

static void writeWav(const char * path, const std::vector<float> & s) {
  FILE * f = fopen(path, "wb");
  const uint32_t n = s.size(), sr = 48000, br = sr * 4, ds = n * 2;  // 16-bit stereo
  auto w32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
  auto w16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
  fwrite("RIFF", 1, 4, f); w32(36 + ds * 2); fwrite("WAVEfmt ", 1, 8, f);
  w32(16); w16(1); w16(2); w32(sr); w32(br); w16(4); w16(16);
  fwrite("data", 1, 4, f); w32(ds * 2);
  for (uint32_t i = 0; i < n; i++) {
    int16_t v = (int16_t)lrintf(fmaxf(-1.f, fminf(1.f, s[i])) * 32767.f);
    w16(v); w16(v);
  }
  fclose(f);
}

int main(int argc, char ** argv) {
  const char * outdir = argc > 1 ? argv[1] : ".";
  // PW SUB CHO SAW PUL PWMD PWMR NOISE HPF HISS DRIFT AGE SPREAD
  Preset presets[] = {
    {"1_default_chorusI",   {25, 45, 1, 100, 70, 45, 30, 0, 1, 30, 20, 20, 0},  3200.f},
    {"2_chorusII_wide",     {25, 45, 2, 100, 70, 45, 30, 0, 1, 30, 25, 20, 60}, 3200.f},
    {"3_chorusI+II_shimmer",{40, 20, 3,  80, 90, 60, 45, 0, 2, 30, 20, 20, 0},  4500.f},
    {"4_dry_no_chorus",     {25, 45, 0, 100, 70, 45, 30, 0, 1, 30, 20, 20, 0},  3200.f},
    {"5_worn_tape_age",     {30, 60, 2, 100, 60, 50, 20, 8, 0, 80, 70, 85, 30}, 2400.f},
  };

  // Fmaj7 -> Am7 -> Dm9 -> Bbmaj7 (low voicings for pad warmth)
  const float chords[4][4] = {{53, 57, 60, 64}, {57, 60, 64, 67}, {50, 57, 60, 64}, {46, 53, 57, 62}};
  const float chordLen = 2.6f, release = 1.6f, attack = 0.7f;
  const uint32_t total = (uint32_t)((chordLen * 4 + release + 0.4f) * kFsDefault);
  const uint32_t B = 64;

  for (auto & pr : presets) {
    std::vector<float> delay(kVoices * kDelayLen, 0.f);
    Engine e;
    memset(&e, 0, sizeof(e));
    e.init(delay.data(), 0xC0FFEEu);
    for (int i = 0; i < kNumParams; i++) e.setParam(i, pr.p[i]);

    float note[kVoices] = {60, 60, 60, 60, 60, 60, 60, 60};
    float env[kVoices] = {0}, lp1[kVoices] = {0}, lp2[kVoices] = {0};
    bool gate[kVoices] = {false};
    std::vector<float> outL(total, 0.f);
    float vbuf[B];
    int lastChord = -1;
    const float g = tanf(3.14159265f * pr.cutoff / kFsDefault), k = 1.2f;  // SVF-ish 2-pole
    float ic1[kVoices] = {0}, ic2[kVoices] = {0};
    (void)lp1; (void)lp2;

    for (uint32_t pos = 0; pos + B <= total; pos += B) {
      const float t = pos / kFsDefault;
      const int c = (int)(t / chordLen);
      uint8_t trig = 0;
      if (c != lastChord) {
        for (int v = 0; v < kVoices; v++) gate[v] = false;
        if (c < 4) {
          const int base = (c & 1) * 4;  // alternate voice banks so releases overlap
          for (int j = 0; j < 4; j++) { note[base + j] = chords[c][j]; gate[base + j] = true; trig |= 1u << (base + j); }
        }
        lastChord = c;
      }
      for (int v = 0; v < kVoices; v++) if (trig & (1u << v)) e.noteOn(v);

      e.beginBlock(B);
      for (int v = 0; v < kVoices; v++) {
        e.renderVoice(v, note[v], vbuf, 1, B);
        for (uint32_t i = 0; i < B; i++) {
          // envelope
          if (gate[v]) env[v] += (1.f - env[v]) * (1.f / (attack * kFsDefault) * 4.f);
          else env[v] -= env[v] * (1.f / (release * kFsDefault) * 4.f);
          // TPT state-variable lowpass
          const float x = vbuf[i];
          const float hp = (x - (k + g) * ic1[v] - ic2[v]) / (1.f + g * (k + g));
          const float bp = g * hp + ic1[v]; ic1[v] = g * hp + bp;
          const float lp = g * bp + ic2[v]; ic2[v] = g * bp + lp;
          outL[pos + i] += lp * env[v];
        }
      }
      e.endBlock(B);
    }

    // report + normalise to -3 dBFS
    float peak = 0.f; double rms = 0; bool bad = false;
    for (float s : outL) { if (!std::isfinite(s)) bad = true; peak = fmaxf(peak, fabsf(s)); rms += s * s; }
    rms = sqrt(rms / outL.size());
    printf("%-24s raw peak %.3f  rms %.3f  %s\n", pr.file, peak, rms, bad ? "NaN/Inf!" : "ok");
    const float gain = 0.707f / fmaxf(peak, 1e-6f);
    for (float & s : outL) s *= gain;
    char path[512]; snprintf(path, sizeof(path), "%s/%s.wav", outdir, pr.file);
    writeWav(path, outL);
  }
  return 0;
}
