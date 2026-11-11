# NIMBUS — vintage DCO pad oscillator for microKORG2

A logue SDK oscillator for the KORG microKORG2 (firmware 2.0+) that recreates the
sound of the classic early-80s single-DCO polysynths: a bright saw, a slowly
breathing PWM pulse, a sub square, white noise, the 4-position high-pass, and a
bucket-brigade chorus complete with its hiss.

## Parameters

| Page | Param    | Range        | What it does |
|------|----------|--------------|--------------|
| 1 | **PW**       | 0–100 %      | Pulse width, 50 % (square) → 5 % (thin) |
| 1 | **SUB**      | 0–100 %      | Sub square one octave down, phase-locked to the DCO |
| 1 | **CHORUS**   | OFF / I / II / I+II | BBD chorus: I = 0.51 Hz gentle, II = 0.86 Hz deeper, I+II = 9.75 Hz shimmer/vibrato |
| 2 | **SAW**      | 0–100 %      | Saw level |
| 2 | **PULSE**    | 0–100 %      | Pulse level |
| 2 | **PWM DEP**  | 0–100 %      | How far the internal triangle LFO sweeps the pulse width |
| 2 | **PWM RATE** | 0–100 %      | PWM LFO rate, 0.1 Hz → ~19 Hz |
| 2 | **NOISE**    | 0–100 %      | White noise into the mixer |
| 3 | **HPF**      | 0 BOOST / 1 FLAT / 2 / 3 | 0 = bass shelf boost, 1 = off, 2 ≈ 240 Hz, 3 ≈ 720 Hz high-pass |
| 3 | **HISS**     | 0–100 %      | BBD hiss (louder with chorus on, like the real circuit) |
| 3 | **DRIFT**    | 0–100 %      | Per-note tuning offset + slow wander, up to ~±9 cents |
| 3 | **AGE**      | 0–100 %      | Saturation, 60 Hz mains hum and top-end rolloff |
| 3 | **SPREAD**   | 0–100 %      | 0 = one shared chorus LFO (authentic); higher offsets each voice's chorus phase for a wider ensemble |

Mod-matrix destinations: **PW** and **Sub**. Route an LFO or envelope to PW for
extra movement on top of the internal PWM.

### Starting points
- **Classic string pad:** defaults, CHORUS II, filter around 3 kHz, slow attack/release.
- **Shimmer pad:** PW 40, PULSE 90, SAW 80, CHORUS I+II, HPF 2.
- **Worn-out voice card:** DRIFT 70, AGE 85, HISS 80, HPF 0.

## Installing
Load `nimbus.mk2unit` into an oscillator slot with the KORG microKORG2 Librarian app.

## Building
From the logue-sdk repo: `build microkorg2/nimbus` inside KORG's Docker environment,
or with a system ARM cross-compiler:

```
make CROSS_COMPILE=arm-linux-gnueabihf-
```

The Makefile's `--param max-inline-insns-single` was lowered to 2147483647, because
GCC 13 rejects KORG's original value. Don't include `<iostream>`: newer GCC versions
then add a libstdc++ symbol the synth probably doesn't have.

## Files
- `nimbus_dsp.h` — all DSP, platform-independent (also builds on a desktop for previews)
- `nimbus.h` — microKORG2 wrapper: voices, output buffer layout, mod matrix
- `header.c` — unit header and parameter definitions
- `unit.cc` — logue SDK entry points

## Before releasing
- Get a developer ID from KORG (see `developer_ids.md` in the SDK) and set `dev_id` in `header.c`.
- "Juno" is a Roland trademark. Describe it as "inspired by" and keep it out of the product name.
