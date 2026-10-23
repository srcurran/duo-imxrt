#ifndef LUSH_FX_H
#define LUSH_FX_H

// Chorus + small reverb for the voices (Orchid, Organ, Strings, ...). Mono,
// 44.1 kHz, int16 samples. Plain C++ with no audio-library dependency, so the
// unit tests can run it on the host. effect_lush.h wraps it for the firmware.
//
// RAM: about 8 KB (chorus 2 KB, reverb 6 KB). The reverb runs at half the
// sample rate (22.05 kHz), which halves its memory and gives a darker tail.

#include <stdint.h>
#include <string.h>

namespace LushFx {

static const float SAMPLE_RATE = 44100.0f;

static inline int16_t clamp16(const float x) {
  if (x > 32767.0f) return 32767;
  if (x < -32768.0f) return -32768;
  return static_cast<int16_t>(x);
}

// Mix and modulation settings. Each voice has its own.
struct Params {
  float dry;          // dry signal level
  float chorus;       // chorus (delayed taps) level
  float reverb;       // reverb level
  float rate1_hz;     // tap 1 LFO rate
  float depth1_ms;    // tap 1 sweep, +/- around 7.5 ms (max 6)
  float rate2_hz;     // tap 2 LFO rate
  float depth2_ms;    // tap 2 sweep
  float output_gain;  // overall level after mixing
};

// Orchid voice: slow, wide Juno-style chorus and a fair amount of reverb.
static const Params ORCHID_PARAMS = {0.7f, 0.6f, 0.45f, 0.5f, 2.5f, 0.83f, 2.5f, 0.8f};

// Chorus: two delay taps around 7.5 ms, each swept by its own triangle LFO.
// Slow, deep sweeps give a Juno-style chorus; fast, shallow ones a vibrato.
class Chorus {
 public:
  static const int SIZE = 1024;  // power of 2, ~23 ms

  Chorus() { set(ORCHID_PARAMS); }

  void reset() {
    memset(buf, 0, sizeof(buf));
    pos = 0;
    phase1 = 0;
    phase2 = 0x80000000u;  // half a cycle apart
  }

  void set(const Params& p) {
    inc1 = lfo_inc(p.rate1_hz);
    inc2 = lfo_inc(p.rate2_hz);
    depth1 = ms_to_samples(clamp_depth(p.depth1_ms));
    depth2 = ms_to_samples(clamp_depth(p.depth2_ms));
  }

  // Takes one dry sample, returns the wet (delayed) sample.
  float process(const float in) {
    buf[pos] = clamp16(in);
    const float wet =
        (read(tap_delay(phase1, depth1)) + read(tap_delay(phase2, depth2))) * 0.5f;
    phase1 += inc1;
    phase2 += inc2;
    pos = (pos + 1) & (SIZE - 1);
    return wet;
  }

 private:
  static constexpr float CENTER_MS = 7.5f;

  static float clamp_depth(const float ms) {
    if (ms < 0.0f) return 0.0f;
    if (ms > 6.0f) return 6.0f;
    return ms;
  }
  static float ms_to_samples(const float ms) { return ms * SAMPLE_RATE / 1000.0f; }
  static uint32_t lfo_inc(const float hz) {
    return static_cast<uint32_t>(4294967296.0f * hz / SAMPLE_RATE);
  }

  // Delay in samples for an LFO phase: 7.5 ms +/- depth.
  static float tap_delay(const uint32_t phase, const float depth) {
    const uint32_t tri = phase < 0x80000000u ? phase : ~phase;  // 0..2^31
    const float t = static_cast<float>(tri) * (1.0f / 2147483648.0f);
    return ms_to_samples(CENTER_MS) + depth * (2.0f * t - 1.0f);
  }

  // Linear-interpolated read, `delay` samples behind the write position.
  float read(const float delay) const {
    const float idx = static_cast<float>(pos) - delay + SIZE;
    const int i = static_cast<int>(idx);
    const float frac = idx - static_cast<float>(i);
    const float a = buf[i & (SIZE - 1)];
    const float b = buf[(i + 1) & (SIZE - 1)];
    return a + (b - a) * frac;
  }

  int16_t buf[SIZE];
  int pos;
  uint32_t phase1;
  uint32_t phase2;
  uint32_t inc1;
  uint32_t inc2;
  float depth1;
  float depth2;
};

// Schroeder/Freeverb-style reverb: 4 damped feedback combs in parallel, then
// 2 allpasses. Lengths are Freeverb's, halved for the 22.05 kHz rate.
class Reverb {
 public:
  void reset() {
    memset(comb_buf, 0, sizeof(comb_buf));
    memset(ap_buf, 0, sizeof(ap_buf));
    for (int c = 0; c < NUM_COMBS; c++) {
      comb_pos[c] = 0;
      comb_filter[c] = 0.0f;
    }
    for (int a = 0; a < NUM_ALLPASS; a++) {
      ap_pos[a] = 0;
    }
  }

  // One sample at 22.05 kHz in, one out.
  float process(const float in) {
    const float x = in * INPUT_GAIN;
    float out = 0.0f;
    int offset = 0;
    for (int c = 0; c < NUM_COMBS; c++) {
      int16_t* line = comb_buf + offset;
      const float y = line[comb_pos[c]];
      comb_filter[c] = y * (1.0f - DAMP) + comb_filter[c] * DAMP;
      line[comb_pos[c]] = clamp16(x + comb_filter[c] * FEEDBACK);
      if (++comb_pos[c] >= comb_len(c)) comb_pos[c] = 0;
      out += y;
      offset += comb_len(c);
    }
    offset = 0;
    for (int a = 0; a < NUM_ALLPASS; a++) {
      int16_t* line = ap_buf + offset;
      const float delayed = line[ap_pos[a]];
      line[ap_pos[a]] = clamp16(out + delayed * 0.5f);
      out = delayed - out;
      if (++ap_pos[a] >= ap_len(a)) ap_pos[a] = 0;
      offset += ap_len(a);
    }
    // Undo the input scaling and average the 4 combs.
    return out * (1.0f / (INPUT_GAIN * NUM_COMBS));
  }

 private:
  static const int NUM_COMBS = 4;
  static const int NUM_ALLPASS = 2;
  static const int COMB_TOTAL = 558 + 594 + 638 + 678;
  static const int AP_TOTAL = 278 + 220;
  static constexpr float INPUT_GAIN = 0.15f;  // headroom for comb build-up
  static constexpr float FEEDBACK = 0.82f;    // tail length
  static constexpr float DAMP = 0.35f;        // high-frequency loss per pass

  static int comb_len(const int c) {
    static const int LEN[NUM_COMBS] = {558, 594, 638, 678};
    return LEN[c];
  }
  static int ap_len(const int a) {
    static const int LEN[NUM_ALLPASS] = {278, 220};
    return LEN[a];
  }

  int16_t comb_buf[COMB_TOTAL];
  int16_t ap_buf[AP_TOTAL];
  int comb_pos[NUM_COMBS];
  float comb_filter[NUM_COMBS];
  int ap_pos[NUM_ALLPASS];
};

// Chorus, then reverb, mixed back with the dry signal.
class Lush {
 public:
  Lush() {
    set(ORCHID_PARAMS);
    reset();
  }

  // Clears all delay lines, so a newly enabled effect doesn't replay old audio.
  void reset() {
    chorus.reset();
    reverb.reset();
    rev_prev = 0.0f;
  }

  void set(const Params& p) {
    params = p;
    chorus.set(p);
  }

  // Processes `n` samples in place. `n` must be even (the reverb works on
  // pairs of samples).
  void process(int16_t* samples, const int n) {
    for (int i = 0; i + 1 < n; i += 2) {
      const float v0 = chorused(samples[i]);
      const float v1 = chorused(samples[i + 1]);
      // Reverb at half rate: average the pair in, interpolate the pair out.
      const float rev = reverb.process((v0 + v1) * 0.5f);
      const float rev0 = (rev_prev + rev) * 0.5f;
      rev_prev = rev;
      samples[i] = clamp16((v0 + rev0 * params.reverb) * params.output_gain);
      samples[i + 1] = clamp16((v1 + rev * params.reverb) * params.output_gain);
    }
  }

 private:
  float chorused(const int16_t in) {
    const float dry = in;
    return dry * params.dry + chorus.process(dry) * params.chorus;
  }

  Params params;
  Chorus chorus;
  Reverb reverb;
  float rev_prev;
};

}  // namespace LushFx

#endif /* LUSH_FX_H */
