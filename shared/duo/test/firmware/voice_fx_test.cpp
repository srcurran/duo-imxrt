#include "fold_fx.h"
#include "lush_fx.h"
#include "pluck_fx.h"
#include "unity.h"

#include <cstdint>
#include <cstdlib>

// Tests for the voices' sound sources: plucked string, wavefolder, and the
// chorus with the Organ's fast vibrato settings.

static const int BLOCK = 128;  // AUDIO_BLOCK_SAMPLES in the firmware
static const int SAMPLE_RATE = 44100;

static int16_t loop_buffer[PluckFx::Pluck::SIZE];
static PluckFx::Pluck string_model(loop_buffer);

void setUp(void) { string_model.reset(); }
void tearDown(void) {}

static int peak(const int16_t* samples, const int n) {
  int p = 0;
  for (int i = 0; i < n; i++) {
    const int a = std::abs(static_cast<int>(samples[i]));
    if (a > p) p = a;
  }
  return p;
}

// Pitch from the strongest autocorrelation lag, within lags min..max.
static float estimate_hz(const int16_t* x, const int n, const int min_lag, const int max_lag) {
  int best_lag = min_lag;
  double best = -1e30;
  for (int lag = min_lag; lag <= max_lag; lag++) {
    double sum = 0;
    for (int i = 0; i + lag < n; i++) sum += static_cast<double>(x[i]) * x[i + lag];
    if (sum > best) {
      best = sum;
      best_lag = lag;
    }
  }
  return static_cast<float>(SAMPLE_RATE) / best_lag;
}

namespace Tests {

void pluck_is_silent_until_plucked() {
  int16_t block[BLOCK];
  for (int b = 0; b < 50; b++) {
    string_model.process(block, BLOCK);
    TEST_ASSERT_EQUAL(0, peak(block, BLOCK));
  }
}

static void check_pluck_pitch(const float hz, const float brightness) {
  string_model.reset();
  string_model.set_tone(brightness, 1.0f);
  string_model.pluck(hz, 1.0f);
  static int16_t audio[8192];
  string_model.process(audio, 4096);  // let the noise settle into a tone
  string_model.process(audio, 8192);
  const float period = SAMPLE_RATE / hz;
  const float measured = estimate_hz(audio, 8192, static_cast<int>(period * 0.8f),
                                     static_cast<int>(period * 1.2f));
  // Within 1% (about a sixth of a semitone); lag resolution is 1 sample.
  TEST_ASSERT_FLOAT_WITHIN(hz * 0.01f + hz * hz / SAMPLE_RATE, hz, measured);
}

void pluck_plays_in_tune() {
  check_pluck_pitch(110.0f, 0.0f);
  check_pluck_pitch(220.0f, 0.5f);
  check_pluck_pitch(440.0f, 1.0f);
}

void pluck_fades_out() {
  int16_t block[BLOCK];
  string_model.set_tone(0.5f, 0.0f);  // shortest ring
  string_model.pluck(220.0f, 1.0f);
  string_model.process(block, BLOCK);
  TEST_ASSERT_GREATER_THAN(2000, peak(block, BLOCK));
  for (int b = 0; b < 2 * SAMPLE_RATE / BLOCK; b++) string_model.process(block, BLOCK);
  TEST_ASSERT_LESS_THAN(50, peak(block, BLOCK));
}

void pluck_longer_sustain_rings_longer() {
  int16_t block[BLOCK];
  int levels[2];
  for (int s = 0; s < 2; s++) {
    string_model.reset();
    string_model.set_tone(0.5f, s == 0 ? 0.0f : 1.0f);
    string_model.pluck(220.0f, 1.0f);
    for (int b = 0; b < SAMPLE_RATE / 2 / BLOCK; b++) string_model.process(block, BLOCK);
    levels[s] = peak(block, BLOCK);
  }
  TEST_ASSERT_GREATER_THAN(levels[0] * 4, levels[1]);
}

void pluck_survives_extreme_pitches() {
  int16_t block[BLOCK];
  string_model.pluck(1.0f, 1.0f);      // below the lowest note: clamped
  string_model.process(block, BLOCK);
  string_model.pluck(20000.0f, 1.0f);  // above the highest: clamped
  string_model.process(block, BLOCK);
  TEST_ASSERT_TRUE(true);  // no crash or out-of-range access
}

void fold_is_identity_within_full_scale() {
  for (float x = -1.0f; x <= 1.0f; x += 0.125f) {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, x, FoldFx::fold(x));
  }
}

void fold_reflects_beyond_full_scale() {
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, FoldFx::fold(1.5f));
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, FoldFx::fold(2.0f));
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, FoldFx::fold(3.0f));
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, FoldFx::fold(-3.0f));
  TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, FoldFx::fold(-6.0f));
}

void folder_output_stays_in_range() {
  FoldFx::Folder folder;
  folder.set_amount(1.0f);
  int16_t block[BLOCK];
  for (int i = 0; i < BLOCK; i++) block[i] = static_cast<int16_t>((i * 512) - 32768);
  folder.process(block, BLOCK, 20000.0f, 16000.0f);
  TEST_ASSERT_LESS_OR_EQUAL(16000, peak(block, BLOCK));
}

// Organ settings: fast, shallow vibrato. The level of a steady tone wobbles
// (dry and delayed signals partly cancel at some pitches, as in any chorus)
// but never drops out or clips.
void organ_vibrato_keeps_level() {
  static LushFx::Lush fx;
  const LushFx::Params organ = {0.55f, 0.75f, 0.25f, 6.7f, 0.35f, 7.3f, 0.25f, 0.9f};
  fx.set(organ);
  fx.reset();
  int16_t block[BLOCK];
  int phase = 0;
  int min_peak = 1 << 20, max_peak = 0;
  for (int b = 0; b < SAMPLE_RATE / BLOCK; b++) {
    for (int i = 0; i < BLOCK; i++, phase++) {
      // 441 Hz triangle-ish test tone, 10000 peak
      const int t = phase % 100;
      block[i] = static_cast<int16_t>((t < 50 ? t : 100 - t) * 400 - 10000);
    }
    fx.process(block, BLOCK);
    if (b > 40) {
      const int p = peak(block, BLOCK);
      if (p < min_peak) min_peak = p;
      if (p > max_peak) max_peak = p;
    }
  }
  TEST_ASSERT_GREATER_THAN(1000, min_peak);
  TEST_ASSERT_LESS_THAN(32767, max_peak);
}

}  // namespace Tests

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(Tests::pluck_is_silent_until_plucked);
  RUN_TEST(Tests::pluck_plays_in_tune);
  RUN_TEST(Tests::pluck_fades_out);
  RUN_TEST(Tests::pluck_longer_sustain_rings_longer);
  RUN_TEST(Tests::pluck_survives_extreme_pitches);
  RUN_TEST(Tests::fold_is_identity_within_full_scale);
  RUN_TEST(Tests::fold_reflects_beyond_full_scale);
  RUN_TEST(Tests::folder_output_stays_in_range);
  RUN_TEST(Tests::organ_vibrato_keeps_level);
  return UNITY_END();
}
