#ifndef FOLD_FX_H
#define FOLD_FX_H

// Wavefolder for the Fold voice (west-coast style). The signal is amplified,
// and whenever it would pass full scale it is reflected back, so a soft wave
// grows extra harmonics the harder it's driven. Plain C++, tested on the host;
// effect_fold.h wraps it for the firmware.

#include <stdint.h>

namespace FoldFx {

// Triangle fold of x: equals x within -1..1, reflects beyond (2 -> 0, 3 -> -1).
static inline float fold(const float x) {
  const float t = (x + 1.0f) * 0.25f;  // one fold period = 4 units of x
  const int whole = static_cast<int>(t >= 0.0f ? t : t - 1.0f);  // floor
  const float frac = t - static_cast<float>(whole);
  const float d = frac - 0.5f;
  return 1.0f - 4.0f * (d < 0.0f ? -d : d);
}

class Folder {
 public:
  Folder() : gain(1.0f) {}

  // amount 0..1: 0 = no folding, 1 = drive x8 (many folds).
  void set_amount(float amount) {
    if (amount < 0.0f) amount = 0.0f;
    if (amount > 1.0f) amount = 1.0f;
    gain = 1.0f + 7.0f * amount;
  }

  // Processes `n` samples in place. `input_full_scale` is the input level
  // treated as 1.0 before folding; the output is scaled to `output_level`.
  void process(int16_t* samples, const int n, const float input_full_scale,
               const float output_level) const {
    const float in_scale = gain / input_full_scale;
    for (int i = 0; i < n; i++) {
      const float y = fold(static_cast<float>(samples[i]) * in_scale) * output_level;
      samples[i] = static_cast<int16_t>(y > 32767.0f ? 32767.0f : (y < -32768.0f ? -32768.0f : y));
    }
  }

 private:
  float gain;
};

}  // namespace FoldFx

#endif /* FOLD_FX_H */
