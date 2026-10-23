#ifndef PLUCK_FX_H
#define PLUCK_FX_H

// Plucked string (Karplus-Strong) for the Pluck voice. A noise burst rings in
// a short delay loop whose length sets the pitch. A small lowpass in the loop
// takes the highs out on each pass, so the note mellows as it fades, like a
// harp or koto. Mono, 44.1 kHz, int16. Plain C++, tested on the host;
// synth_pluck.h wraps it for the firmware.
//
// RAM: a 4 KB loop buffer (2048 samples, lowest note ~22 Hz), passed in by
// the caller so the firmware can put it in the slower RAM bank.

#include <math.h>
#include <stdint.h>
#include <string.h>

namespace PluckFx {

class Pluck {
 public:
  static const int SIZE = 2048;  // power of 2
  static constexpr float SAMPLE_RATE = 44100.0f;
  static constexpr float MIN_HZ = SAMPLE_RATE / (SIZE - 4);

  // `buffer` must hold SIZE samples and outlive the Pluck.
  explicit Pluck(int16_t* buffer) : buf(buffer), noise(0x1234567u) {
    set_tone(0.5f, 0.5f);
    frequency(220.0f);
    reset();
  }

  void reset() {
    memset(buf, 0, SIZE * sizeof(int16_t));
    pos = 0;
  }

  // brightness 0..1: 0 = soft and dark, 1 = bright and metallic.
  // sustain 0..1: 0 = short pluck (~0.3 s), 1 = long ring (~6 s).
  void set_tone(const float brightness, const float sustain) {
    const float b = clamp01(brightness);
    // Loop filter y = (1-c)*x[n] + c*x[n-1]: c = 0.5 is the classic
    // Karplus-Strong average (darkest), c near 0 barely filters.
    c = 0.5f - 0.45f * b;
    // Time to fade by 60 dB, 0.3 s to 6 s on an exponential curve.
    ring_seconds = 0.3f * expf(3.0f * clamp01(sustain));
    excite_brightness = 0.25f + 0.75f * b;
    update_feedback();
  }

  // Sets the pitch. Can be called while the string rings (glide).
  void frequency(float hz) {
    if (hz < MIN_HZ) hz = MIN_HZ;
    if (hz > 4000.0f) hz = 4000.0f;
    // The loop filter delays the signal by `c` samples, so read that much
    // sooner to stay in tune.
    delay = SAMPLE_RATE / hz - c;
    loop_hz = hz;
    update_feedback();
  }

  // Plucks the string: fills one period of the loop with a noise burst.
  // `level` 0..1 is the strike strength.
  void pluck(const float hz, const float level) {
    frequency(hz);
    const int len = static_cast<int>(delay) + 2;
    float lp = 0.0f;
    for (int k = 0; k < len; k++) {
      // One-pole lowpass on the noise: a darker strike for low brightness.
      lp += (white() - lp) * excite_brightness;
      buf[(pos - 1 - k) & (SIZE - 1)] = clamp16(lp * level * 26000.0f);
    }
  }

  // Writes `n` samples of the ringing string into `out`.
  void process(int16_t* out, const int n) {
    for (int i = 0; i < n; i++) {
      const float x0 = read(delay);
      const float x1 = read(delay + 1.0f);
      const float y = ((1.0f - c) * x0 + c * x1) * feedback;
      buf[pos] = clamp16(y);
      pos = (pos + 1) & (SIZE - 1);
      out[i] = clamp16(x0);
    }
  }

 private:
  // Loop gain for the ring time: a high note passes through the loop more
  // often per second, so it needs a gain closer to 1 to ring as long.
  // (The loop filter's own losses come on top: bright tones ring longer.)
  void update_feedback() {
    feedback = expf(-6.9078f / (ring_seconds * loop_hz));  // 10^-3 per ring
  }

  static float clamp01(const float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
  static int16_t clamp16(const float x) {
    if (x > 32767.0f) return 32767;
    if (x < -32768.0f) return -32768;
    return static_cast<int16_t>(x);
  }

  // White noise in -1..1 (xorshift32).
  float white() {
    noise ^= noise << 13;
    noise ^= noise >> 17;
    noise ^= noise << 5;
    return static_cast<float>(static_cast<int32_t>(noise)) * (1.0f / 2147483648.0f);
  }

  // Linear-interpolated read, `d` samples behind the write position.
  float read(const float d) const {
    const float idx = static_cast<float>(pos) - d + SIZE;
    const int i = static_cast<int>(idx);
    const float frac = idx - static_cast<float>(i);
    const float a = buf[i & (SIZE - 1)];
    const float b = buf[(i + 1) & (SIZE - 1)];
    return a + (b - a) * frac;
  }

  int16_t* buf;
  int pos;
  float delay;
  float c;
  float feedback;
  float ring_seconds = 1.0f;
  float loop_hz = 220.0f;
  float excite_brightness;
  uint32_t noise;
};

}  // namespace PluckFx

#endif /* PLUCK_FX_H */
