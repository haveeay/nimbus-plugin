/**
 *  @file header.c
 *  @brief NIMBUS - vintage DCO polysynth pad oscillator for microKORG2
 *
 *  Inspired by the classic 80s single-DCO polysynths: saw + PWM pulse +
 *  sub square + noise, a 4-position high-pass, a BBD-style chorus with
 *  its hiss, voice-to-voice drift and analog "age".
 */

#include "unit.h"
#include "runtime.h"

const __unit_header unit_header_t unit_header = {
    .header_size = sizeof(unit_header_t),
    .target = UNIT_TARGET_PLATFORM | k_unit_module_osc,
    .api = UNIT_API_VERSION,
    .dev_id = 0x0U,      // TODO: replace with your Korg-issued developer ID before releasing
    .unit_id = 0x1U,
    .version = 0x00010000U,
    .name = "NIMBUS",
    .num_presets = 0,
    .num_params = 13,
    .params = {
        // min, max, center, default, type, frac, frac mode, reserved, name

        // Page 1 - the big three
        {0, 100, 0,  25, k_unit_param_type_percent, 0, 0, 0, {"PW"}},
        {0, 100, 0,  45, k_unit_param_type_percent, 0, 0, 0, {"SUB"}},
        {0,   3, 0,   1, k_unit_param_type_strings, 0, 0, 0, {"CHORUS"}},

        // Page 2 - DCO
        {0, 100, 0, 100, k_unit_param_type_percent, 0, 0, 0, {"SAW"}},
        {0, 100, 0,  70, k_unit_param_type_percent, 0, 0, 0, {"PULSE"}},
        {0, 100, 0,  45, k_unit_param_type_percent, 0, 0, 0, {"PWM DEP"}},
        {0, 100, 0,  30, k_unit_param_type_percent, 0, 0, 0, {"PWM RATE"}},
        {0, 100, 0,   0, k_unit_param_type_percent, 0, 0, 0, {"NOISE"}},

        // Page 3 - character
        {0,   3, 0,   1, k_unit_param_type_strings, 0, 0, 0, {"HPF"}},
        {0, 100, 0,  30, k_unit_param_type_percent, 0, 0, 0, {"HISS"}},
        {0, 100, 0,  20, k_unit_param_type_percent, 0, 0, 0, {"DRIFT"}},
        {0, 100, 0,  20, k_unit_param_type_percent, 0, 0, 0, {"AGE"}},
        {0, 100, 0,   0, k_unit_param_type_percent, 0, 0, 0, {"SPREAD"}}}};
