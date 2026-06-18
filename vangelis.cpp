/*
 * vangelis.cpp
 * Dirty noisy sine wave oscillator for Korg Prologue
 *
 * Shape      : drive — 0 = clean sine, 1 = heavy saturation
 *              (LFO on Shape animates harmonic content)
 * Shift-Shape: noise mix
 * Param 1    : Jitter   — low-frequency random pitch drift
 * Param 2    : FM Fdbk  — self-modulation depth
 * Param 3    : WavFold  — wavefold pre-gain (0 = off, 100 = 4x into folder)
 * Param 4    : SR Reduc — sample rate reduction (0 = off, 100 = ~1kHz)
 */

#include "userosc.h"

struct Params {
  float   drive;     // from Shape (0..1)
  float   noise_mix; // from Shift-Shape (0..1)
  float   jitter;    // pitch drift depth (0..1)
  float   fm_depth;  // FM self-feedback depth (0..1)
  float   fold;      // wavefold amount (0..1)
  uint8_t sr_period; // sample rate divider: 1 = off, 2–48 = lo-fi

  Params() : drive(0.f), noise_mix(0.f), jitter(0.f), fm_depth(0.f), fold(0.f), sr_period(1) {}
};

struct State {
  float   phi;        // oscillator phase [0..1)
  float   w0;         // base frequency in cycles/sample
  float   prev_sig;   // previous output, for FM feedback
  float   jitter_lp;  // LP-filtered noise for pitch drift
  float   sr_held;    // held sample for SR reduction
  uint8_t sr_counter; // counter for SR reduction
  float   lfo;
  float   lfoz;
  bool    reset_flag;

  State() : phi(0.f), w0(0.f), prev_sig(0.f), jitter_lp(0.f),
            sr_held(0.f), sr_counter(0), lfo(0.f), lfoz(0.f), reset_flag(false) {}

  void reset() {
    phi        = 0.f;
    prev_sig   = 0.f;
    sr_held    = 0.f;
    sr_counter = 0;
    lfo        = lfoz;
    // jitter_lp intentionally not reset — keeps drift smooth across notes
  }
};

static Params s_params;
static State  s_state;

// ---------------------------------------------------------------------------

void OSC_INIT(uint32_t platform, uint32_t api)
{
  (void)platform;
  (void)api;
  s_params = Params();
  s_state  = State();
}

void OSC_CYCLE(const user_osc_param_t * const params,
               int32_t *yn,
               const uint32_t frames)
{
  if (s_state.reset_flag) {
    s_state.reset();
    s_state.reset_flag = false;
  }

  const uint8_t note = (params->pitch) >> 8;
  const uint8_t mod  = (params->pitch) & 0xFF;
  s_state.w0  = osc_w0f_for_note(note, mod);
  s_state.lfo = q31_to_f32(params->shape_lfo);

  const float   w0        = s_state.w0;
  const float   fm_scale  = s_params.fm_depth * 0.5f;
  const float   noise_mix = s_params.noise_mix;
  const uint8_t sr_period = s_params.sr_period;

  float   phi        = s_state.phi;
  float   prev_sig   = s_state.prev_sig;
  float   jitter_lp  = s_state.jitter_lp;
  float   sr_held    = s_state.sr_held;
  uint8_t sr_counter = s_state.sr_counter;
  float   lfoz       = s_state.lfoz;

  const float lfo_inc    = (s_state.lfo - lfoz) / frames;
  // ~8 Hz LP cutoff for jitter: slow enough to hear as pitch instability
  static const float k_jitter_lp = 0.001f;

  q31_t * __restrict y = (q31_t *)yn;
  const q31_t * y_e = y + frames;

  for (; y != y_e; ) {
    // Shape + LFO control drive
    const float drive = clipminmaxf(0.f, s_params.drive + lfoz, 1.f);

    // FM self-feedback: previous output modulates phase
    float sig = osc_sinf(phi + prev_sig * fm_scale);

    // Wavefold: amplify then reflect into [-1, 1]
    if (s_params.fold > 0.f) {
      sig *= 1.f + s_params.fold * 3.f; // up to 4× gain
      if (sig > 1.f)  sig = 2.f - sig;
      if (sig < -1.f) sig = -2.f - sig;
      if (sig > 1.f)  sig = 2.f - sig;  // second pass handles full 4× range
      if (sig < -1.f) sig = -2.f - sig;
    }

    // Drive: boost then soft-clip for harmonic saturation
    sig = osc_softclipf(0.3f, sig * (1.f + drive * 4.f));

    // Noise mix
    sig = (1.f - noise_mix) * sig + noise_mix * osc_white();

    // Sample rate reduction: zero-order hold at reduced rate
    if (sr_period > 1) {
      if (++sr_counter >= sr_period) {
        sr_counter = 0;
        sr_held    = sig;
      }
      sig = sr_held;
    }

    prev_sig = sig;
    *(y++) = f32_to_q31(sig);

    // Jitter: LP-filtered noise drifts the pitch slowly
    jitter_lp += (osc_white() - jitter_lp) * k_jitter_lp;
    phi += w0 * (1.f + jitter_lp * s_params.jitter * 0.3f);
    phi -= (uint32_t)phi;
    lfoz += lfo_inc;
  }

  s_state.phi        = phi;
  s_state.prev_sig   = prev_sig;
  s_state.jitter_lp  = jitter_lp;
  s_state.sr_held    = sr_held;
  s_state.sr_counter = sr_counter;
  s_state.lfoz       = lfoz;
}

void OSC_NOTEON(const user_osc_param_t * const params)
{
  (void)params;
  s_state.reset_flag = true;
}

void OSC_NOTEOFF(const user_osc_param_t * const params)
{
  (void)params;
}

void OSC_PARAM(uint16_t index, uint16_t value)
{
  switch (index) {
  case k_user_osc_param_id1:
    s_params.jitter = clip01f(value * 0.01f);
    break;
  case k_user_osc_param_id2:
    s_params.fm_depth = clip01f(value * 0.01f);
    break;
  case k_user_osc_param_id3:
    s_params.fold = clip01f(value * 0.01f);
    break;
  case k_user_osc_param_id4:
    // 0 → no reduction (period=1), 100 → ~1kHz (period=48)
    s_params.sr_period = 1 + (uint8_t)(value * 47 / 100);
    s_state.sr_counter = 0; // reset counter on change to avoid overshoot
    break;
  case k_user_osc_param_shape:
    s_params.drive = param_val_to_f32(value);
    break;
  case k_user_osc_param_shiftshape:
    s_params.noise_mix = param_val_to_f32(value);
    break;
  default:
    break;
  }
}
