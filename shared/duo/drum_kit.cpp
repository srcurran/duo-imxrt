#include "drum_kit.h"
#include <cmath>

namespace DrumKit {

// Kick: today's low DUO kick with a click, a two-stage pitch drop and some
// saturation for punch.
const KickParams KICK_DEFAULT = {
    50.0f, // base_hz
    4.0f,  // fast_drop
    5.0f,  // fast_ms
    1.0f,  // slow_drop
    40.0f, // slow_ms
    95.0f, // decay_ms
    0.35f, // click
    1.8f,  // drive
};

static const float PI_F = 3.14159265f;
static const float SILENT = 0.0001f; // -80 dB

// Roland TR-808 cymbal/hat oscillator frequencies.
static const float METAL_HZ[6] = {205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f};

// sin(2*pi*phase) for phase in [0, 1). Parabolic approximation, error < 0.1%.
static inline float sine(float phase) {
  const float x = phase * 2.0f - 1.0f; // sin(2*pi*phase) = -sin(pi*x)
  const float ax = x < 0 ? -x : x;
  float y = 4.0f * x * (1.0f - ax);
  const float ay = y < 0 ? -y : y;
  y = 0.225f * (y * ay - y) + y;
  return -y;
}

// Smooth tanh-like saturation.
static inline float softclip(float x) {
  if (x > 3.0f) {
    x = 3.0f;
  } else if (x < -3.0f) {
    x = -3.0f;
  }
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

static inline float saturate(float x, float drive) {
  if (drive <= 1.0f) {
    return x;
  }
  return softclip(x * drive) / softclip(drive);
}

static inline void advance(float &phase, float inc) {
  phase += inc;
  if (phase >= 1.0f) {
    phase -= 1.0f;
  }
}

void Svf::set(float sample_rate, float cutoff_hz, float q) {
  const float g = tanf(PI_F * cutoff_hz / sample_rate);
  k = 1.0f / q;
  a1 = 1.0f / (1.0f + g * (g + k));
  a2 = g * a1;
  a3 = g * a2;
}

void Svf::process(float in, float &low, float &band, float &high) {
  const float v3 = in - ic2;
  const float v1 = a1 * ic1 + a2 * v3;
  const float v2 = ic2 + a2 * ic1 + a3 * v3;
  ic1 = 2.0f * v1 - ic1;
  ic2 = 2.0f * v2 - ic2;
  low = v2;
  band = v1;
  high = in - k * v1 - v2;
}

Kit::Kit(float sample_rate) : sr(sample_rate), inv_sr(1.0f / sample_rate) {
  for (int i = 0; i < 6; i++) {
    metal_inc[i] = METAL_HZ[i] * inv_sr;
  }
  snare_hp.set(sr, 1500.0f, 0.7f);
  hat_hp.set(sr, 7000.0f, 0.7f);
  hat_bp.set(sr, 10000.0f, 1.2f);
  open_hp.set(sr, 6500.0f, 0.7f);
  open_bp.set(sr, 9000.0f, 1.2f);
  clap_bp.set(sr, 1200.0f, 1.8f);
  clap_burst_gap = (uint32_t)(0.011f * sr);
  clap_tail_start = 3 * clap_burst_gap;
}

// Per-sample multiplier for an exponential decay with time constant `ms`.
float Kit::coef(float ms) const {
  return expf(-1000.0f / (ms * sr));
}

float Kit::noise() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (float)(int32_t)rng * (1.0f / 2147483648.0f);
}

bool Kit::active(Drum drum) const {
  return voices[drum].on;
}

void Kit::trigger(Drum drum, float level) {
  Voice &v = voices[drum];
  v.on = true;
  v.level = level;
  v.t = 0;
  v.phase1 = 0;
  v.phase2 = 0;
  v.amp = 1.0f;
  v.amp2 = 1.0f;
  v.pitch1 = 1.0f;
  v.pitch2 = 1.0f;

  switch (drum) {
  case Kick:
    v.amp_coef = coef(kick.decay_ms);
    v.amp2_coef = coef(1.5f);
    v.pitch1_coef = coef(kick.fast_ms);
    v.pitch2_coef = coef(kick.slow_ms);
    break;
  case Tom:
    v.amp_coef = coef(140.0f);
    v.amp2_coef = coef(3.0f);
    v.pitch1_coef = coef(35.0f);
    break;
  case Snare:
    v.amp_coef = coef(45.0f);
    v.amp2_coef = coef(70.0f);
    v.pitch1_coef = coef(12.0f);
    break;
  case ClosedHat:
    v.amp_coef = coef(18.0f);
    v.amp2 = 0;
    // Choke the open hat: fade it out quickly instead of cutting (no click).
    voices[OpenHat].amp_coef = coef(6.0f);
    break;
  case OpenHat:
    v.amp_coef = coef(180.0f);
    v.amp2 = 0;
    break;
  case Clap:
    v.amp_coef = coef(2.5f);
    v.amp2 = 0;
    v.amp2_coef = coef(110.0f);
    break;
  default:
    break;
  }
}

float Kit::render_kick(Voice &v) {
  const float hz = kick.base_hz *
                   (1.0f + v.pitch1 * kick.fast_drop + v.pitch2 * kick.slow_drop);
  advance(v.phase1, hz * inv_sr);
  const float body = sine(v.phase1) * v.amp;
  const float click = noise() * v.amp2 * kick.click;
  v.pitch1 *= v.pitch1_coef;
  v.pitch2 *= v.pitch2_coef;
  v.amp *= v.amp_coef;
  v.amp2 *= v.amp2_coef;
  return saturate(body + click, kick.drive) * 0.9f;
}

float Kit::render_tom(Voice &v) {
  const float hz = 100.0f * (1.0f + 0.8f * v.pitch1);
  advance(v.phase1, hz * inv_sr);
  const float body = sine(v.phase1) * v.amp;
  const float click = noise() * v.amp2 * 0.2f;
  v.pitch1 *= v.pitch1_coef;
  v.amp *= v.amp_coef;
  v.amp2 *= v.amp2_coef;
  return saturate(body + click, 1.3f) * 0.75f;
}

float Kit::render_snare(Voice &v) {
  const float bend = 1.0f + 0.4f * v.pitch1;
  advance(v.phase1, 180.0f * bend * inv_sr);
  advance(v.phase2, 330.0f * bend * inv_sr);
  const float body = (0.55f * sine(v.phase1) + 0.3f * sine(v.phase2)) * v.amp;
  float low, band, high;
  snare_hp.process(noise(), low, band, high);
  const float rattle = high * v.amp2 * 0.75f;
  v.pitch1 *= v.pitch1_coef;
  v.amp *= v.amp_coef;
  v.amp2 *= v.amp2_coef;
  return saturate(body + rattle, 1.4f) * 0.85f;
}

float Kit::metal() {
  float sum = 0;
  for (int i = 0; i < 6; i++) {
    advance(metal_phase[i], metal_inc[i]);
    sum += metal_phase[i] < 0.5f ? 1.0f : -1.0f;
  }
  return sum * (1.0f / 6.0f);
}

float Kit::render_clap(Voice &v) {
  // Three quick bursts, then a longer tail.
  if (v.t == clap_burst_gap || v.t == 2 * clap_burst_gap) {
    v.amp = 1.0f;
  } else if (v.t == clap_tail_start) {
    v.amp2 = 0.8f;
  }
  float low, band, high;
  clap_bp.process(noise(), low, band, high);
  const float out = band * (v.amp + v.amp2) * 1.6f;
  v.amp *= v.amp_coef;
  v.amp2 *= v.amp2_coef;
  return out;
}

void Kit::render(float *bus0, float *bus1, int n) {
  Voice &kick_v = voices[Kick];
  Voice &tom_v = voices[Tom];
  Voice &snare_v = voices[Snare];
  Voice &closed_v = voices[ClosedHat];
  Voice &clap_v = voices[Clap];
  Voice &open_v = voices[OpenHat];

  for (int i = 0; i < n; i++) {
    float a = 0;
    if (kick_v.on) {
      a += render_kick(kick_v) * kick_v.level;
    }
    if (tom_v.on) {
      a += render_tom(tom_v) * tom_v.level;
    }
    if (snare_v.on) {
      a += render_snare(snare_v) * snare_v.level;
    }

    float b = 0;
    if (closed_v.on || open_v.on) {
      const float src = 0.6f * metal() + 0.5f * noise();
      float low, band, high, low2, band2, high2;
      if (closed_v.on) {
        hat_hp.process(src, low, band, high);
        hat_bp.process(src, low2, band2, high2);
        b += (high + 0.4f * band2) * closed_v.amp * 0.7f * closed_v.level;
        closed_v.amp *= closed_v.amp_coef;
      }
      if (open_v.on) {
        open_hp.process(src, low, band, high);
        open_bp.process(src, low2, band2, high2);
        b += (high + 0.4f * band2) * open_v.amp * 0.55f * open_v.level;
        open_v.amp *= open_v.amp_coef;
      }
    }
    if (clap_v.on) {
      b += render_clap(clap_v) * clap_v.level;
    }

    bus0[i] = a;
    bus1[i] = b;

    for (int d = 0; d < NUM_DRUMS; d++) {
      voices[d].t++;
    }
  }

  for (int d = 0; d < NUM_DRUMS; d++) {
    Voice &v = voices[d];
    if (v.on) {
      if (v.t > clap_tail_start + 64 && v.amp + v.amp2 < SILENT) {
        v.on = false;
      }
    }
  }
}

} // namespace DrumKit
