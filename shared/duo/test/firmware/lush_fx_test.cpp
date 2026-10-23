#include <cmath>  // before unity.h, whose isnan/isinf macros break it

#include "lush_fx.h"
#include "unity.h"

#include <cstdint>
#include <cstdlib>

// Tests for the Orchid voice's chorus + reverb (LushFx::Lush).

static const int BLOCK = 128;  // AUDIO_BLOCK_SAMPLES in the firmware
static const int SAMPLE_RATE = 44100;

static LushFx::Lush fx;

void setUp(void) { fx.reset(); }
void tearDown(void) {}

static int peak(const int16_t* samples, const int n) {
  int p = 0;
  for (int i = 0; i < n; i++) {
    const int a = std::abs(static_cast<int>(samples[i]));
    if (a > p) p = a;
  }
  return p;
}

namespace Tests {

void silence_stays_silent() {
  int16_t block[BLOCK] = {0};
  for (int b = 0; b < 100; b++) {
    fx.process(block, BLOCK);
    TEST_ASSERT_EQUAL(0, peak(block, BLOCK));
  }
}

// An impulse gives a tail (the reverb rings after the dry sound has gone)...
void impulse_leaves_a_reverb_tail() {
  int16_t block[BLOCK] = {0};
  block[0] = 20000;
  fx.process(block, BLOCK);
  int tail_peak = 0;
  // 50-300 ms after the impulse: dry and chorus taps (<= 10 ms) are long gone.
  for (int b = 1; b < (SAMPLE_RATE * 3 / 10) / BLOCK; b++) {
    for (int i = 0; i < BLOCK; i++) block[i] = 0;
    fx.process(block, BLOCK);
    if (b * BLOCK > SAMPLE_RATE / 20) {
      const int p = peak(block, BLOCK);
      if (p > tail_peak) tail_peak = p;
    }
  }
  TEST_ASSERT_GREATER_THAN(20, tail_peak);
}

// ...that dies away instead of building up (the feedback is stable).
void reverb_tail_decays_to_silence() {
  int16_t block[BLOCK];
  // 1 s of loud square wave, then 5 s of silence.
  for (int b = 0; b < SAMPLE_RATE / BLOCK; b++) {
    for (int i = 0; i < BLOCK; i++) block[i] = (i < BLOCK / 2) ? 20000 : -20000;
    fx.process(block, BLOCK);
  }
  int last_peak = 0;
  for (int b = 0; b < 5 * SAMPLE_RATE / BLOCK; b++) {
    for (int i = 0; i < BLOCK; i++) block[i] = 0;
    fx.process(block, BLOCK);
    last_peak = peak(block, BLOCK);
  }
  TEST_ASSERT_LESS_THAN(4, last_peak);
}

// Full-scale input clips at the int16 limits instead of wrapping round
// (a wrap would flip the sign: a loud crack).
void full_scale_input_never_wraps() {
  // Full-scale 100 Hz sine. A wrap would jump ~65000 between two samples;
  // the real signal moves at most a few thousand.
  int16_t block[BLOCK];
  int prev = 0, max_jump = 0, n = 0;
  for (int b = 0; b < 2 * SAMPLE_RATE / BLOCK; b++) {
    for (int i = 0; i < BLOCK; i++, n++) {
      block[i] = static_cast<int16_t>(32767 * sin(2 * 3.14159265 * 100 * n / SAMPLE_RATE));
    }
    fx.process(block, BLOCK);
    for (int i = 0; i < BLOCK; i++) {
      const int jump = std::abs(block[i] - prev);
      if (jump > max_jump) max_jump = jump;
      prev = block[i];
    }
  }
  TEST_ASSERT_LESS_THAN(20000, max_jump);
}

// reset() clears the delay lines: no old tail after re-enabling.
void reset_clears_the_tail() {
  int16_t block[BLOCK];
  for (int i = 0; i < BLOCK; i++) block[i] = 20000;
  fx.process(block, BLOCK);
  fx.reset();
  for (int b = 0; b < 50; b++) {
    for (int i = 0; i < BLOCK; i++) block[i] = 0;
    fx.process(block, BLOCK);
    TEST_ASSERT_EQUAL(0, peak(block, BLOCK));
  }
}

// A constant offset (DC) is removed, not built up by the reverb.
void dc_offset_is_removed() {
  int16_t block[BLOCK];
  int last_peak = 0;
  for (int b = 0; b < 2 * SAMPLE_RATE / BLOCK; b++) {
    for (int i = 0; i < BLOCK; i++) block[i] = -3000;
    fx.process(block, BLOCK);
    last_peak = peak(block, BLOCK);
  }
  TEST_ASSERT_LESS_THAN(30, last_peak);
}

}  // namespace Tests

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(Tests::silence_stays_silent);
  RUN_TEST(Tests::impulse_leaves_a_reverb_tail);
  RUN_TEST(Tests::reverb_tail_decays_to_silence);
  RUN_TEST(Tests::full_scale_input_never_wraps);
  RUN_TEST(Tests::reset_clears_the_tail);
  RUN_TEST(Tests::dc_offset_is_removed);
  return UNITY_END();
}
