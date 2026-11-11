#pragma once
/*
 *  FxRack.h - NIMBUS effects rack (desktop plugin only)
 *
 *    synth -> DRIVE -> TAPE ECHO -> CLOUD -> out
 *
 *  DRIVE      gentle asymmetric tube-style saturation with tone and mix
 *  TAPE ECHO  three-playback-head tape delay in the spirit of the classic
 *             space echoes: head combinations, intensity up to runaway
 *             self-oscillation, tape saturation and tone in the loop, wow
 *             and flutter, tape-speed glides when TIME moves, tempo sync
 *  CLOUD      granular micro-looper: continuously records the last few
 *             seconds and re-plays them as grains (MOSAIC / HAZE / GLIDE /
 *             REVERSE), with repeats, freeze, filter and a built-in space
 *
 *  Framework-independent so it can be tested headless.
 */

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace nimbus {
namespace fx {

static inline float fclamp(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }
static inline float sat(float x) {  // soft clip, unity slope at 0
  x = fclamp(x, -3.f, 3.f);
  const float xx = x * x;
  return x * (27.f + xx) / (27.f + 9.f * xx);
}
static inline float smoothstep(float t) { t = fclamp(t, 0.f, 1.f); return t * t * (3.f - 2.f * t); }
static inline float onepoleCoef(float fc, float fs) { return 1.f - std::exp(-6.2831853f * fc / fs); }

struct Rng {
  uint32_t s = 0x9E3779B9u;
  inline float uni() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float)(s >> 8) * (1.f / 16777216.f); }  // [0,1)
  inline float bi() { return uni() * 2.f - 1.f; }
};

static inline uint32_t pow2Above(uint32_t n) { uint32_t p = 1; while (p < n) p <<= 1; return p; }

// ===========================================================================
struct Drive {
  bool on = false;
  float amount = 0.25f, tone = 0.6f, mix = 1.f;
  float fs = 48000.f, lpL = 0, lpR = 0, dcL = 0, dcR = 0, dcxL = 0, dcxR = 0, lpCoef = 0.5f;

  void prepare(float sr) { fs = sr; lpL = lpR = dcL = dcR = dcxL = dcxR = 0.f; update(); }
  void update() { lpCoef = onepoleCoef(1500.f * std::pow(12.f, tone), fs); }  // 1.5 .. 18 kHz

  inline float shape(float x, float & lp, float & dcx, float & dcy) {
    const float g = 1.f + amount * 9.f;
    float y = sat(x * g + 0.18f * amount * x * x * g);  // even harmonics, tube-ish
    y *= 1.f / (1.f + amount * 5.5f);                   // keep level roughly constant
    const float out = y - dcx + 0.9995f * dcy;          // DC blocker
    dcx = y; dcy = out;
    lp += lpCoef * (out - lp);
    return lp;
  }

  void process(float * L, float * R, int n) {
    if (!on) return;
    for (int i = 0; i < n; i++) {
      const float wl = shape(L[i], lpL, dcxL, dcL), wr = shape(R[i], lpR, dcxR, dcR);
      L[i] += mix * (wl - L[i]);
      R[i] += mix * (wr - R[i]);
    }
  }
};

// ===========================================================================
struct TapeEcho {
  enum { kHeadModes = 7 };
  bool on = false;
  float time = 0.35f;       // 0..1 free-running repeat rate
  int sync = 0;             // 0 free, 1..5 = 1/16, 1/8T, 1/8, 1/8D, 1/4 for head 1
  int heads = 6;            // 0..6 -> {1},{2},{3},{1+2},{2+3},{1+3},{1+2+3}
  float intensity = 0.45f;  // feedback 0..1 (past ~0.85 it starts to run away)
  float echo = 0.4f;        // echo level
  float tone = 0.5f;        // dark .. bright
  float wow = 0.3f;         // wow & flutter
  float bpm = 120.f;

  float fs = 48000.f;
  std::vector<float> buf;
  uint32_t mask = 0, w = 0;
  float tSmooth = 0.f, lp = 0.f, hp = 0.f, lpCoef = 0.f, hpCoef = 0.f;
  float wowPh = 0.f, flutPh = 0.f, drift = 0.f, driftT = 0.f;
  Rng rng;

  void prepare(float sr) {
    fs = sr;
    buf.assign(pow2Above((uint32_t)(4.2f * sr)), 0.f);
    mask = (uint32_t)buf.size() - 1;
    w = 0; lp = hp = 0.f;
    tSmooth = targetSeconds() * fs;
    update();
  }
  void clear() { std::fill(buf.begin(), buf.end(), 0.f); lp = hp = 0.f; }
  void update() {
    lpCoef = onepoleCoef(1800.f * std::pow(5.f, tone), fs);  // 1.8 .. 9 kHz
    hpCoef = onepoleCoef(70.f + 130.f * (1.f - tone), fs);   // thinner when dark, like worn tape
  }

  float targetSeconds() const {  // head-1 delay
    if (sync > 0 && bpm > 20.f) {
      static const float beats[6] = {0.f, 0.25f, 1.f / 3.f, 0.5f, 0.75f, 1.f};
      return fclamp(beats[sync] * 60.f / bpm, 0.02f, 1.3f);
    }
    return 0.05f * std::pow(7.f, time);  // 50 .. 350 ms, head 3 up to ~1 s
  }

  inline float tap(float d) const {
    const float pos = (float)w + (float)buf.size() - d;
    const uint32_t ip = (uint32_t)pos;
    const float fr = pos - (float)ip;
    const float a = buf[(ip - 1) & mask], b = buf[ip & mask], c = buf[(ip + 1) & mask], e = buf[(ip + 2) & mask];
    // 4-point Hermite
    const float c1 = 0.5f * (c - a), c2 = a - 2.5f * b + 2.f * c - 0.5f * e, c3 = 0.5f * (e - a) + 1.5f * (b - c);
    return ((c3 * fr + c2) * fr + c1) * fr + b;
  }

  void process(float * L, float * R, int n) {
    if (!on) return;
    static const bool use[kHeadModes][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {1, 1, 0},
                                            {0, 1, 1}, {1, 0, 1}, {1, 1, 1}};
    const bool * hu = use[heads < 0 ? 0 : (heads > 6 ? 6 : heads)];
    const int nHeads = (int)hu[0] + (int)hu[1] + (int)hu[2];
    const float headNorm = 1.f / std::sqrt((float)nHeads);
    const float target = targetSeconds() * fs;
    const float slew = 1.f - std::exp(-1.f / (0.25f * fs));  // ~250 ms tape-speed glide
    const float fb = intensity * 1.12f;

    for (int i = 0; i < n; i++) {
      tSmooth += (target - tSmooth) * slew;

      // wow (slow), flutter (fast), and random speed drift
      wowPh += 0.55f / fs; if (wowPh >= 1.f) wowPh -= 1.f;
      flutPh += 6.3f / fs; if (flutPh >= 1.f) flutPh -= 1.f;
      if ((w & 1023u) == 0) driftT = rng.bi();
      drift += (driftT - drift) * (30.f / fs);
      const float mod = wow * (0.0045f * std::sin(6.2831853f * wowPh) + 0.0012f * std::sin(6.2831853f * flutPh) +
                               0.0025f * drift);
      const float d1 = tSmooth * (1.f + mod);

      float h[3] = {0.f, 0.f, 0.f}, sum = 0.f;
      for (int k = 0; k < 3; k++)
        if (hu[k]) { h[k] = tap(d1 * (float)(k + 1)); sum += h[k]; }
      sum *= headNorm;

      // feedback path: tone + tape saturation
      lp += lpCoef * (sum - lp);
      hp += hpCoef * (lp - hp);
      const float fbSig = (lp - hp) * fb;

      const float in = 0.5f * (L[i] + R[i]);
      buf[w & mask] = sat(in + fbSig);
      w++;

      // heads spread across the stereo field: 1 left, 2 right, 3 centre
      const float el = (0.85f * h[0] + 0.25f * h[1] + 0.6f * h[2]) * headNorm;
      const float er = (0.25f * h[0] + 0.85f * h[1] + 0.6f * h[2]) * headNorm;
      L[i] += echo * el;
      R[i] += echo * er;
    }
  }
};

// ===========================================================================
// Small stereo FDN reverb used by CLOUD's SPACE control.
struct Space {
  static const int N = 8;
  std::vector<float> line[N];
  uint32_t len[N] = {}, idx[N] = {};
  float damp[N] = {};
  float fs = 48000.f, g = 0.8f, dampCoef = 0.5f;
  float apBufL[512] = {}, apBufR[512] = {};
  uint32_t apIdx = 0;

  void prepare(float sr) {
    fs = sr;
    static const float ms[N] = {29.7f, 37.1f, 41.1f, 43.7f, 47.9f, 53.3f, 59.9f, 67.1f};
    for (int i = 0; i < N; i++) {
      len[i] = (uint32_t)(ms[i] * 0.001f * sr);
      line[i].assign(len[i], 0.f);
      idx[i] = 0; damp[i] = 0.f;
    }
    dampCoef = onepoleCoef(5500.f, sr);
    std::memset(apBufL, 0, sizeof(apBufL)); std::memset(apBufR, 0, sizeof(apBufR));
  }
  void setDecay(float seconds) {  // RT60 -> per-line gain using the mean line length
    const float meanLen = 0.0475f;
    g = std::pow(10.f, -3.f * meanLen / std::fmax(0.1f, seconds));
  }

  inline void process(float inL, float inR, float & outL, float & outR) {
    float o[N];
    for (int i = 0; i < N; i++) o[i] = line[i][idx[i]];
    // Householder mix
    float s = 0.f;
    for (int i = 0; i < N; i++) s += o[i];
    s *= 2.f / N;
    for (int i = 0; i < N; i++) {
      float v = (o[i] - s) * g + ((i & 1) ? inR : inL) * 0.35f;
      damp[i] += dampCoef * (v - damp[i]);
      line[i][idx[i]] = damp[i];
      if (++idx[i] >= len[i]) idx[i] = 0;
    }
    outL = (o[0] + o[2] + o[4] + o[6]) * 0.5f;
    outR = (o[1] + o[3] + o[5] + o[7]) * 0.5f;
  }
};

// ===========================================================================
struct Cloud {
  enum Mode { kMosaic = 0, kHaze, kGlide, kReverse, kNumModes };
  static const int kMaxGrains = 32;

  bool on = false, freeze = false;
  int mode = kMosaic;
  float activity = 0.5f;  // grain rate / density
  float time = 0.5f;      // grain size and how far back grains reach
  float shape = 0.6f;     // percussive .. smooth grain envelope
  float repeats = 0.3f;   // feedback of the cloud into its own buffer
  float filter = 0.7f;    // lowpass on the cloud
  float space = 0.4f;     // reverb on the cloud
  float mix = 0.4f;

  struct Grain {
    bool active = false;
    float pos = 0.f, rate = 1.f, rateEnd = 1.f, len = 1.f, age = 0.f, gl = 0.7f, gr = 0.7f;
  };

  float fs = 48000.f;
  std::vector<float> bufL, bufR;
  uint32_t mask = 0, w = 0;
  Grain grains[kMaxGrains];
  float untilNext = 0.f, fbL = 0.f, fbR = 0.f, lpL = 0.f, lpR = 0.f, lpCoef = 0.5f;
  Space space_;
  Rng rng;

  void prepare(float sr) {
    fs = sr;
    const uint32_t n = pow2Above((uint32_t)(6.f * sr));
    bufL.assign(n, 0.f); bufR.assign(n, 0.f);
    mask = n - 1; w = 0;
    for (auto & g : grains) g.active = false;
    untilNext = 0.f; fbL = fbR = lpL = lpR = 0.f;
    space_.prepare(sr);
    update();
  }
  void clear() {
    std::fill(bufL.begin(), bufL.end(), 0.f); std::fill(bufR.begin(), bufR.end(), 0.f);
    for (auto & g : grains) g.active = false;
    space_.prepare(fs);
  }
  void update() {
    lpCoef = onepoleCoef(400.f * std::pow(45.f, filter), fs);  // 400 Hz .. 18 kHz
    space_.setDecay(0.6f + space * 5.f);
  }

  inline float read(const std::vector<float> & b, float pos) const {
    const uint32_t ip = (uint32_t)pos;
    const float fr = pos - (float)ip;
    return b[ip & mask] + fr * (b[(ip + 1) & mask] - b[ip & mask]);
  }

  void spawn() {
    Grain * g = nullptr;
    for (auto & gg : grains) if (!gg.active) { g = &gg; break; }
    if (!g) return;

    const float lenSec = 0.04f + time * time * 0.56f;           // 40 ms .. 600 ms
    const float windowSec = 0.25f + time * 3.0f;                 // how far back grains reach
    float rate = 1.f, rateEnd = 1.f, len = lenSec;
    switch (mode) {
      case kMosaic: {
        const float r = rng.uni();
        rate = r < 0.55f ? 1.f : (r < 0.8f ? 2.f : (r < 0.93f ? 0.5f : 1.5f));
        rateEnd = rate;
        break;
      }
      case kHaze:
        len = lenSec * 2.2f;
        rate = 1.f + rng.bi() * 0.006f;  // +/- 10 cents smear
        rateEnd = rate;
        break;
      case kGlide:
        len = lenSec * 1.6f;
        rate = rng.uni() < 0.5f ? 1.f : (rng.uni() < 0.5f ? 0.5f : 2.f);
        rateEnd = rate * (rng.uni() < 0.5f ? 0.75f : 1.335f);  // glide down a 4th or up a 4th
        break;
      case kReverse:
        rate = rng.uni() < 0.75f ? -1.f : -0.5f;
        rateEnd = rate;
        break;
      default: break;
    }
    const float lenS = len * fs;
    const float maxRate = std::fmax(std::fabs(rate), std::fabs(rateEnd));
    // start far enough back that a forward grain never overtakes the write head
    float back = (rate > 0.f ? lenS * maxRate : 0.f) + 64.f + rng.uni() * windowSec * fs;
    if (rate < 0.f) back += lenS * maxRate;
    g->active = true;
    g->pos = (float)w + (float)bufL.size() - back;
    g->rate = rate; g->rateEnd = rateEnd;
    g->len = lenS; g->age = 0.f;
    const float pan = rng.bi() * (mode == kHaze ? 0.9f : 0.6f);
    g->gl = std::sqrt(0.5f * (1.f - pan)); g->gr = std::sqrt(0.5f * (1.f + pan));
  }

  void process(float * L, float * R, int n) {
    if (!on) return;
    const float grainsPerSec = 1.5f + activity * activity * 38.f;
    const float meanLen = (0.04f + time * time * 0.56f) * (mode == kHaze ? 2.2f : (mode == kGlide ? 1.6f : 1.f));
    const float overlap = std::fmax(1.f, grainsPerSec * meanLen);
    const float norm = 1.1f / std::sqrt(overlap);
    const float edge = 0.03f + 0.47f * shape;
    const float fbAmt = repeats * 0.85f;

    for (int i = 0; i < n; i++) {
      // scheduler with jitter
      untilNext -= 1.f;
      if (untilNext <= 0.f) {
        spawn();
        untilNext = fs / grainsPerSec * (0.5f + rng.uni());
      }

      float cl = 0.f, cr = 0.f;
      for (auto & g : grains) {
        if (!g.active) continue;
        const float t = g.age / g.len;
        const float env = smoothstep(t / edge) * smoothstep((1.f - t) / edge);
        const float r = g.rate + (g.rateEnd - g.rate) * t;
        const float sl = read(bufL, g.pos), sr = read(bufR, g.pos);
        const float mono = 0.5f * (sl + sr);
        cl += (0.5f * sl + 0.5f * mono) * env * g.gl * 1.4142f;
        cr += (0.5f * sr + 0.5f * mono) * env * g.gr * 1.4142f;
        g.pos += r;
        if (g.pos < 0.f) g.pos += (float)bufL.size();
        g.age += 1.f;
        if (g.age >= g.len) g.active = false;
      }
      cl *= norm; cr *= norm;

      lpL += lpCoef * (cl - lpL);
      lpR += lpCoef * (cr - lpR);
      float sl, sr;
      space_.process(lpL, lpR, sl, sr);
      const float wl = lpL + space * 0.9f * sl, wr = lpR + space * 0.9f * sr;

      // record (input + repeats) unless frozen
      if (!freeze) {
        bufL[w & mask] = sat(L[i] + fbAmt * lpL);
        bufR[w & mask] = sat(R[i] + fbAmt * lpR);
        w++;
      }
      L[i] = L[i] * (1.f - 0.5f * mix) + wl * mix;
      R[i] = R[i] * (1.f - 0.5f * mix) + wr * mix;
    }
  }
};

// ===========================================================================
struct Rack {
  Drive drive;
  TapeEcho tape;
  Cloud cloud;

  void prepare(float sr) { drive.prepare(sr); tape.prepare(sr); cloud.prepare(sr); }
  void update() { drive.update(); tape.update(); cloud.update(); }
  void process(float * L, float * R, int n) {
    drive.process(L, R, n);
    tape.process(L, R, n);
    cloud.process(L, R, n);
  }
};

}  // namespace fx
}  // namespace nimbus
