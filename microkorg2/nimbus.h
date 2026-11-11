#pragma once
/*
 *  nimbus.h  -  microKORG2 wrapper around the NIMBUS DSP core.
 *
 *  NOTE: do not include <iostream> anywhere in this unit. With newer GCC it
 *  pulls in std::ios_base_library_init (GLIBCXX_3.4.32), which the synth's
 *  runtime is unlikely to provide, and the unit would fail to load.
 */

#include <cstddef>
#include <cstdint>

#include "utils/buffer_ops.h"
#include "utils/int_math.h"
#include "utils/mk2_utils.h"
#include "runtime.h"
#include "unit_osc.h"
#include "macros.h"

#include "nimbus_dsp.h"

class Nimbus {
 public:
  // mod matrix destinations exposed by this oscillator
  enum { kModDestPW = 0, kModDestSub, kNumModDest };

  inline int8_t Init(const unit_runtime_desc_t * desc) {
    if (desc->samplerate != 48000) return k_unit_err_samplerate;
    if (!desc->hooks.sdram_alloc) return k_unit_err_memory;

    float * m = reinterpret_cast<float *>(desc->hooks.sdram_alloc(nimbus::kDelayBytes));
    if (!m) return k_unit_err_memory;

    runtime_desc_ = *desc;
    engine_.init(m);
    for (int i = 0; i < nimbus::kNumParams; i++)
      engine_.setParam(i, unit_header.params[i].init);
    return k_unit_err_none;
  }

  inline void Teardown() { engine_.dbuf = nullptr; }  // sdram freed by the runtime
  inline void Reset() { engine_.reset(); }
  inline void Resume() { engine_.reset(); }
  inline void Suspend() {}

  fast_inline void Process(float * out, size_t frames) {
    const unit_runtime_osc_context_t * ctxt =
        static_cast<const unit_runtime_osc_context_t *>(runtime_desc_.hooks.runtime_context);
    const int voices = ctxt->voiceLimit > nimbus::kVoices ? nimbus::kVoices : ctxt->voiceLimit;

    engine_.beginBlock(frames);
    for (int v = 0; v < voices; v++) {
      if (ctxt->trigger & (1u << v)) engine_.noteOn(v);
      // same output addressing as KORG's own "waves" example
      const uint32_t offset = GetBufferOffset(ctxt, v, frames);
      engine_.renderVoice(v, ctxt->pitch[v], out + offset + (v & 3), ctxt->outputStride, frames);
    }
    engine_.endBlock(frames);
  }

  inline void setParameter(uint8_t index, int32_t value) { engine_.setParam(index, value); }

  inline int32_t getParameterValue(uint8_t index) const {
    return index < nimbus::kNumParams ? engine_.raw[index] : 0;
  }

  inline const char * getParameterStrValue(uint8_t index, int32_t value) const {
    return engine_.strValue(index, value);
  }

  void unit_platform_exclusive(uint8_t messageId, void * data, uint32_t dataSize) {
    (void)dataSize;
    const unit_runtime_osc_context_t * ctxt =
        static_cast<const unit_runtime_osc_context_t *>(runtime_desc_.hooks.runtime_context);
    switch (messageId) {
      case kMk2PlatformExclusiveModData: {
        const float * depth = GetModDepth(data);
        const int32_t * index = GetModIndex(data);
        const int voices = ctxt->voiceLimit > nimbus::kVoices ? nimbus::kVoices : ctxt->voiceLimit;
        for (int v = 0; v < nimbus::kVoices; v++) engine_.pwMod[v] = engine_.subMod[v] = 0.f;
        for (int src = 0; src < kNumMk2ModSrc; src++) {
          if (index[src] != kModDestPW && index[src] != kModDestSub) continue;
          float * dst = (index[src] == kModDestPW) ? engine_.pwMod : engine_.subMod;
          const float * mod = GetModSourceData(data, src, ctxt->voiceLimit, 0);
          for (int v = 0; v < voices; v++) dst[v] += mod[v] * depth[src];
        }
        break;
      }
      case kMk2PlatformExclusiveModDestName: {
        char * name = GetModDestNameData(data);
        switch (name[0]) {
          case kModDestPW:  name[1] = 'P'; name[2] = 'W'; break;
          case kModDestSub: name[1] = 'S'; name[2] = 'u'; name[3] = 'b'; break;
          default: break;
        }
        break;
      }
      default:
        break;
    }
  }

 private:
  nimbus::Engine engine_;
  unit_runtime_desc_t runtime_desc_;
};
