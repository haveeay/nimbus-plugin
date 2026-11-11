#pragma once
/*
 *  nimbus_dsp.h
 *
 *  Platform-independent DSP core for NIMBUS, a vintage single-DCO polysynth
 *  pad oscillator. No SDK or libm dependencies, so the same code runs on the
 *  microKORG2 and in the desktop preview renderer.
 *
 *  Signal path per voice:
 *    DCO (polyBLEP saw + PWM pulse + phase-locked sub square + white noise)
 *      -> 4-position HPF (0 bass boost, 1 flat, 2 / 3 high-pass)
 *      -> "age" saturation
 *      -> BBD-style chorus (I / II / I+II) with bandwidth loss
 *      -> BBD hiss + mains hum
 *      -> "age" top-end rolloff
 */

#include <cstdint>

namespace nimbus {

enum {
  kPW = 0, kSub, kChorus,
  kSaw, kPulse, kPwmDep, kPwmRate, kNoise,
  kHpf, kHiss, kDrift, kAge, kSpread,
  kNumParams
};

enum { kChorusOff = 0, kChorusI, kChorusII, kChorusI_II, kNumChorus };
enum { kHpfBoost = 0, kHpfFlat, kHpf2, kHpf3, kNumHpf };

static const int kVoices = 8;
static const int kDelayLen = 256;            // per voice, 5.3 ms @ 48 kHz
static const int kDelayMask = kDelayLen - 1;
static const uint32_t kDelayBytes = kVoices * kDelayLen * sizeof(float);  // 8 KB
static const float kFsDefault = 48000.f;

// ---------------------------------------------------------------------------
// small math helpers (no libm)
// ---------------------------------------------------------------------------
static inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float fracf(float x) { return x - (float)(int32_t)x; }  // x >= 0

// 2^x, ~0.005 cent accuracy over the audio range
static inline float pow2f(float x) {
  const float r = (float)(int32_t)(x + (x >= 0.f ? 0.5f : -0.5f));
  const float f = x - r;  // [-0.5, 0.5]
  const float p = 1.f + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f +
                  f * (0.00961813f + f * 0.00133336f))));
  union { float f; int32_t i; } u;
  u.i = ((int32_t)r + 127) << 23;
  return p * u.f;
}

// tanh-like soft clipper, exact unity slope at 0, flat at |x| >= 3
static inline float softclip(float x) {
  x = clampf(x, -3.f, 3.f);
  const float xx = x * x;
  return x * (27.f + xx) / (27.f + 9.f * xx);
}

// polyBLEP residual for a discontinuity at t = 0 (t in [0,1), dt = phase inc)
static inline float blep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.f; }
  if (t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
  return 0.f;
}

static inline float tri(float ph) {  // ph in [0,1) -> [-1,1]
  const float x = ph - 0.5f;
  return 4.f * (x < 0.f ? -x : x) - 1.f;
}

static inline float sinapx(float ph) {  // ~sin(2*pi*ph), parabolic
  const float t = 2.f * ph - 1.f;
  return -4.f * t * (1.f - (t < 0.f ? -t : t));
}

// ---------------------------------------------------------------------------

struct Engine {
  // raw parameter values (as the UI sees them)
  int32_t raw[kNumParams];
  float rawF[kNumParams];

  // cooked parameters
  float pw, sub, saw, pulse, pwmDep, pwmInc, noise, hiss, drift, age, spread;
  int chorus, hpf;
  float hpfA, drive, driveComp, toneA;

  // sample rate, and whether chorus/hiss/hum are done by the host (desktop plugin)
  float fs;
  bool external;

  // per-voice modulation from the microKORG2 mod matrix
  float pwMod[kVoices], subMod[kVoices];

  // global state
  uint32_t rng;
  float pwmPhase, choPhase, humPhase;
  float pwmLfo0, pwmLfo1;      // block start/end LFO values
  float choPh0, choPh1;        // block start/end chorus phases
  uint32_t widx;
  float * dbuf;

  // per-voice state
  float phase[kVoices];
  uint8_t tog[kVoices];
  float detStatic[kVoices], detWalk[kVoices];
  float hpS[kVoices], wetLp[kVoices], hissHp[kVoices], hissLp[kVoices], toneLp[kVoices];

  inline float rnd() {  // white, [-1, 1)
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return (float)(int32_t)rng * 4.656612873e-10f;
  }

  // one-pole lowpass coefficient for cutoff fc (Hz)
  inline float onepole(float fc) const {
    fc = clampf(fc, 1.f, 0.45f * fs);
    return 1.f - pow2f(-6.2831853f * 1.4426950f * fc / fs);
  }

  void setSampleRate(float sr) {
    fs = sr;
    for (int i = 0; i < kNumParams; i++) setParamF(i, rawF[i]);
  }

  void init(float * delayBuffer, uint32_t seed = 0x1234567u, float sr = kFsDefault, bool ext = false) {
    dbuf = delayBuffer;
    fs = sr;
    external = ext;
    rng = seed ? seed : 1u;
    pwmPhase = choPhase = humPhase = 0.f;
    widx = 0;
    for (int v = 0; v < kVoices; v++) {
      pwMod[v] = subMod[v] = 0.f;
      detStatic[v] = rnd();
      detWalk[v] = 0.f;
    }
    reset();
  }

  void reset() {
    for (int v = 0; v < kVoices; v++) {
      phase[v] = (float)v * 0.137f; tog[v] = 0;  // free-running, unsynced DCOs
      hpS[v] = wetLp[v] = hissHp[v] = hissLp[v] = toneLp[v] = 0.f;
    }
    if (dbuf) for (int i = 0; i < kVoices * kDelayLen; i++) dbuf[i] = 0.f;
  }

  void setParam(int id, int32_t value) { setParamF(id, (float)value); }

  // value in UI units: 0..100 for percentages, index for CHORUS / HPF
  void setParamF(int id, float valueF) {
    if (id < 0 || id >= kNumParams) return;
    rawF[id] = valueF;
    const int32_t value = (int32_t)(valueF + 0.5f);
    raw[id] = value;
    const float p = valueF * 0.01f;
    switch (id) {
      case kPW:      pw = p; break;
      case kSub:     sub = p; break;
      case kChorus:  chorus = value < 0 ? 0 : (value >= kNumChorus ? kNumChorus - 1 : value); break;
      case kSaw:     saw = p; break;
      case kPulse:   pulse = p; break;
      case kPwmDep:  pwmDep = p; break;
      case kPwmRate: pwmInc = 0.1f * pow2f(p * 7.6f) / fs; break;  // 0.1 .. ~19 Hz
      case kNoise:   noise = p; break;
      case kHpf: {
        hpf = value < 0 ? 0 : (value >= kNumHpf ? kNumHpf - 1 : value);
        // one-pole corners: 100 Hz shelf, 8 Hz DC block, 240 Hz, 720 Hz
        static const float fc[kNumHpf] = {100.f, 8.f, 240.f, 720.f};
        hpfA = onepole(fc[hpf]);
        break;
      }
      case kHiss:    hiss = p; break;
      case kDrift:   drift = p; break;
      case kAge:
        age = p;
        drive = 1.f + 2.5f * p;
        driveComp = 1.f / (1.f + 0.9f * p);
        toneA = onepole(18000.f * pow2f(-1.71f * p));  // 18 kHz -> 5.5 kHz
        break;
      case kSpread:  spread = p; break;
      default: break;
    }
  }

  void noteOn(int v) {
    // every note lands on a slightly different tuning, like a tired voice card
    detStatic[v] = rnd();
  }

  void beginBlock(uint32_t frames) {
    pwmLfo0 = tri(pwmPhase);
    pwmPhase = fracf(pwmPhase + pwmInc * frames);
    pwmLfo1 = tri(pwmPhase);

    static const float rate[kNumChorus] = {0.f, 0.513f, 0.863f, 9.75f};
    choPh0 = choPhase;
    choPhase = fracf(choPhase + rate[chorus] / fs * frames);
    choPh1 = choPhase;
    if (choPh1 < choPh0) choPh1 += 1.f;  // keep the ramp monotonic across wrap
  }

  void endBlock(uint32_t frames) {
    widx = (widx + frames) & kDelayMask;
    humPhase = fracf(humPhase + 60.f / fs * frames);
  }

  // Render one voice. Output sample i goes to dst[i * stride].
  void renderVoice(int v, float note, float * dst, uint32_t stride, uint32_t frames) {
    // --- pitch + drift -------------------------------------------------------
    detWalk[v] = clampf(detWalk[v] * 0.998f + rnd() * 0.03f, -1.f, 1.f);
    const float cents = drift * (detStatic[v] * 5.f + detWalk[v] * 4.f);
    float w = 440.f / fs * pow2f((note - 69.f) * (1.f / 12.f) + cents * (1.f / 1200.f));
    w = clampf(w, 1e-6f, 0.45f);
    const float dts = 0.5f * w;

    // --- block ramps ---------------------------------------------------------
    float lfo = pwmLfo0;
    const float lfoInc = (pwmLfo1 - pwmLfo0) / frames;

    const bool cho = !external && chorus != kChorusOff;
    float d = 0.f, dInc = 0.f;
    if (cho) {
      static const float center[kNumChorus] = {0.f, 158.f, 158.f, 150.f};  // ~3.3 ms
      static const float depth[kNumChorus]  = {0.f, 77.f, 91.f, 14.f};
      const float off = spread * (float)v * 0.125f;
      const float d0 = center[chorus] + depth[chorus] * tri(fracf(choPh0 + off));
      const float d1 = center[chorus] + depth[chorus] * tri(fracf(choPh1 + off));
      d = d0; dInc = (d1 - d0) / frames;
    }

    const float lSaw = saw, lPul = pulse;
    const float lSub = clampf(sub + subMod[v], 0.f, 1.f);
    const float lNoise = noise;
    const float pwBase = pw + pwMod[v];
    const float hissGain = external ? 0.f : hiss * (chorus != kChorusOff ? 0.030f : 0.012f);
    const float humGain = external ? 0.f : age * 0.004f;
    const float humInc = 60.f / fs;

    float p = phase[v];
    uint8_t tg = tog[v];
    float hp = hpS[v], wl = wetLp[v], hh = hissHp[v], hl = hissLp[v], tl = toneLp[v];
    float * buf = dbuf + v * kDelayLen;
    float hph = humPhase;

    for (uint32_t i = 0; i < frames; i++) {
      // --- DCO ---------------------------------------------------------------
      const float sawS = 2.f * p - 1.f - blep(p, w);

      const float amt = clampf(pwBase + pwmDep * (0.5f + 0.5f * lfo), 0.f, 1.f);
      const float width = 0.5f - 0.45f * amt;
      float pul = (p < width ? 1.f : -1.f) + blep(p, w) - blep(fracf(p + 1.f - width), w);
      pul -= 2.f * width - 1.f;  // AC-coupled like the real thing

      const float ps = 0.5f * (p + (float)tg);
      const float subS = (ps < 0.5f ? 1.f : -1.f) + blep(ps, dts) - blep(fracf(ps + 0.5f), dts);

      float x = (sawS * lSaw + pul * lPul + subS * lSub + rnd() * lNoise) * 0.4f;

      // --- HPF ---------------------------------------------------------------
      hp += hpfA * (x - hp);
      float y = (hpf == kHpfBoost) ? x + 0.9f * hp : x - hp;

      // --- age: saturation ---------------------------------------------------
      y = softclip(y * drive) * driveComp;

      // --- BBD chorus --------------------------------------------------------
      if (cho) {
        const uint32_t wpos = (widx + i) & kDelayMask;
        buf[wpos] = y;
        const float pos = (float)(widx + i + kDelayLen) - d;
        const int32_t ip = (int32_t)pos;
        const float fr = pos - (float)ip;
        const float s0 = buf[ip & kDelayMask], s1 = buf[(ip + 1) & kDelayMask];
        wl += 0.55f * (s0 + fr * (s1 - s0) - wl);  // BBD bandwidth loss (~7 kHz)
        y = 0.62f * (y + wl);
        d += dInc;
      }

      // --- BBD hiss ----------------------------------------------------------
      const float n = rnd();
      hh += 0.18f * (n - hh);   // high-pass ~1.5 kHz
      hl += 0.70f * ((n - hh) - hl);  // low-pass ~9 kHz
      y += hl * hissGain;

      // --- mains hum (60 Hz + 3rd harmonic) ----------------------------------
      y += humGain * (sinapx(hph) + 0.4f * sinapx(fracf(hph * 3.f)));
      hph += humInc; if (hph >= 1.f) hph -= 1.f;

      // --- age: top-end rolloff ----------------------------------------------
      tl += toneA * (y - tl);

      dst[i * stride] = tl * 0.38f;

      // --- advance -----------------------------------------------------------
      p += w;
      if (p >= 1.f) { p -= 1.f; tg ^= 1; }
      lfo += lfoInc;
    }

    phase[v] = p; tog[v] = tg;
    hpS[v] = hp; wetLp[v] = wl; hissHp[v] = hh; hissLp[v] = hl; toneLp[v] = tl;
  }

  const char * strValue(int id, int32_t value) const {
    static const char * cho[kNumChorus] = {"OFF", "I", "II", "I+II"};
    static const char * hpfs[kNumHpf] = {"0 BOOST", "1 FLAT", "2", "3"};
    if (id == kChorus && value >= 0 && value < kNumChorus) return cho[value];
    if (id == kHpf && value >= 0 && value < kNumHpf) return hpfs[value];
    return nullptr;
  }
};

}  // namespace nimbus
