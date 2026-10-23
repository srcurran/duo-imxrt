#include <cmath>
#include "drum_kit.h"
#include "unity.h"

using namespace DrumKit;

static const float SR = 44100.0f;
static const int BLOCK = 128;

// Peak level of one bus over `ms` milliseconds.
static float peak(Kit &kit, int bus, float ms) {
  float a[BLOCK], b[BLOCK];
  float p = 0;
  const int blocks = (int)(ms * SR / 1000.0f / BLOCK) + 1;
  for (int i = 0; i < blocks; i++) {
    kit.render(a, b, BLOCK);
    for (int j = 0; j < BLOCK; j++) {
      const float v = fabsf(bus == 0 ? a[j] : b[j]);
      if (v > p) {
        p = v;
      }
    }
  }
  return p;
}

static int bus_of(Drum d) {
  return d <= Snare ? 0 : 1;
}

namespace Tests {

void silent_until_triggered() {
  Kit kit(SR);
  TEST_ASSERT_EQUAL_FLOAT(0.0f, peak(kit, 0, 100));
  TEST_ASSERT_EQUAL_FLOAT(0.0f, peak(kit, 1, 100));
}

void every_drum_sounds_and_stays_in_range() {
  for (int d = 0; d < NUM_DRUMS; d++) {
    Kit kit(SR);
    kit.trigger((Drum)d);
    const float p = peak(kit, bus_of((Drum)d), 100);
    TEST_ASSERT_TRUE_MESSAGE(p > 0.1f, "drum too quiet");
    TEST_ASSERT_TRUE_MESSAGE(p <= 1.0f, "drum clips");
    // Nothing leaks onto the other pad's bus.
    Kit kit2(SR);
    kit2.trigger((Drum)d);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, peak(kit2, 1 - bus_of((Drum)d), 100));
  }
}

void every_drum_dies_away() {
  for (int d = 0; d < NUM_DRUMS; d++) {
    Kit kit(SR);
    kit.trigger((Drum)d);
    peak(kit, 0, 3000);
    TEST_ASSERT_FALSE_MESSAGE(kit.active((Drum)d), "drum still ringing after 3 s");
  }
}

void closed_hat_chokes_open_hat() {
  Kit kit(SR);
  kit.trigger(OpenHat);
  peak(kit, 1, 20);
  kit.trigger(ClosedHat);
  peak(kit, 1, 150);
  TEST_ASSERT_FALSE(kit.active(OpenHat));

  // Without the choke, the open hat is still ringing at that point.
  Kit kit2(SR);
  kit2.trigger(OpenHat);
  peak(kit2, 1, 170);
  TEST_ASSERT_TRUE(kit2.active(OpenHat));
}

void level_scales_output() {
  Kit loud(SR), soft(SR);
  loud.trigger(Tom, 1.0f);
  soft.trigger(Tom, 0.5f);
  const float pl = peak(loud, 0, 50);
  const float ps = peak(soft, 0, 50);
  TEST_ASSERT_FLOAT_WITHIN(0.02f, pl * 0.5f, ps);
}

} // namespace Tests

void setUp(void) {}
void tearDown(void) {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(Tests::silent_until_triggered);
  RUN_TEST(Tests::every_drum_sounds_and_stays_in_range);
  RUN_TEST(Tests::every_drum_dies_away);
  RUN_TEST(Tests::closed_hat_chokes_open_hat);
  RUN_TEST(Tests::level_scales_output);
  return UNITY_END();
}
