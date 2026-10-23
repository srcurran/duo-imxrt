#include "drum_pads.h"
#include "unity.h"

using DrumPads::Left;
using DrumPads::Middle;
using DrumPads::NoZone;
using DrumPads::Pad;
using DrumPads::Right;
using DrumPads::SETTLE_MS;
using DrumPads::Zone;

#define ASSERT_ZONE(expected, actual) TEST_ASSERT_EQUAL_INT((int)(expected), (int)(actual))

// Holds the given contacts from `from` to `to` ms (polled every ms) and
// returns the only zone played in that time (fails if more than one hit).
static Zone hold(Pad &pad, bool l, bool m, bool r, uint32_t from, uint32_t to) {
  Zone played = NoZone;
  for (uint32_t t = from; t <= to; t++) {
    const Zone z = pad.update(l, m, r, t, false);
    if (z != NoZone) {
      TEST_ASSERT_EQUAL_INT_MESSAGE((int)NoZone, (int)played, "played twice");
      played = z;
    }
  }
  return played;
}

namespace Tests {

void zone_rules() {
  ASSERT_ZONE(NoZone, DrumPads::zone_of(false, false, false));
  ASSERT_ZONE(Left, DrumPads::zone_of(true, false, false));
  ASSERT_ZONE(Left, DrumPads::zone_of(true, true, false));
  ASSERT_ZONE(Middle, DrumPads::zone_of(false, true, false));
  ASSERT_ZONE(Right, DrumPads::zone_of(false, true, true));
  ASSERT_ZONE(Right, DrumPads::zone_of(false, false, true));
  ASSERT_ZONE(Middle, DrumPads::zone_of(true, true, true));
}

void plays_once_after_settling() {
  Pad pad;
  ASSERT_ZONE(NoZone, pad.update(false, true, false, 0, false));
  ASSERT_ZONE(NoZone, pad.update(false, true, false, SETTLE_MS - 1, false));
  ASSERT_ZONE(Middle, pad.update(false, true, false, SETTLE_MS, false));
  // Holding doesn't play again.
  ASSERT_ZONE(NoZone, hold(pad, false, true, false, SETTLE_MS + 1, 100));
}

void landing_between_left_and_middle_plays_left_only() {
  // Middle touches first, left a moment later: one hit, the left drum.
  Pad pad;
  ASSERT_ZONE(NoZone, pad.update(false, true, false, 0, false));
  ASSERT_ZONE(NoZone, pad.update(false, true, false, 2, false));
  ASSERT_ZONE(Left, hold(pad, true, true, false, 3, 100));
}

void landing_between_middle_and_right_plays_right_only() {
  Pad pad;
  ASSERT_ZONE(NoZone, pad.update(false, true, false, 0, false));
  ASSERT_ZONE(Right, hold(pad, false, true, true, 2, 100));
}

void flicker_does_not_retrigger() {
  Pad pad;
  ASSERT_ZONE(Middle, hold(pad, false, true, false, 0, 20));
  // Contact drops out briefly (shorter than RELEASE_MS) and comes back.
  ASSERT_ZONE(NoZone, hold(pad, false, false, false, 21, 40));
  ASSERT_ZONE(NoZone, hold(pad, false, true, false, 41, 80));
  TEST_ASSERT_TRUE(pad.held());
}

void flicker_on_an_edge_contact_keeps_the_zone() {
  // Resting on middle+right; right drops out briefly. Still Right, no Middle hit.
  Pad pad;
  ASSERT_ZONE(Right, hold(pad, false, true, true, 0, 20));
  ASSERT_ZONE(NoZone, hold(pad, false, true, false, 21, 35));
  ASSERT_ZONE(NoZone, hold(pad, false, true, true, 36, 80));
}

void lift_and_tap_again_plays_again() {
  Pad pad;
  ASSERT_ZONE(Middle, hold(pad, false, true, false, 0, 10));
  ASSERT_ZONE(NoZone, hold(pad, false, false, false, 11, 11 + DrumPads::RELEASE_MS));
  TEST_ASSERT_FALSE(pad.held());
  ASSERT_ZONE(Middle, hold(pad, false, true, false, 60, 80));
}

void sliding_plays_each_new_zone() {
  Pad pad;
  ASSERT_ZONE(Left, hold(pad, true, false, false, 0, 20));
  // Rolling onto the middle while still on the left: still Left.
  ASSERT_ZONE(NoZone, hold(pad, true, true, false, 21, 40));
  // Off the left (after the release time): Middle.
  ASSERT_ZONE(Middle, hold(pad, false, true, false, 41, 100));
  // Onto the right: Right as soon as it touches.
  ASSERT_ZONE(Right, hold(pad, false, true, true, 101, 120));
  ASSERT_ZONE(NoZone, hold(pad, false, false, true, 121, 160));
  TEST_ASSERT_EQUAL_INT(Right, pad.last_zone);
}

void tap_does_not_repeat() {
  Pad pad;
  ASSERT_ZONE(Left, hold(pad, true, false, false, 0, 50));
  // A step arrives while the finger is still down, but too soon.
  ASSERT_ZONE(NoZone, pad.update(true, false, false, 60, true));
  ASSERT_ZONE(NoZone, hold(pad, false, false, false, 61, 150));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 200, true));
}

void hold_repeats_on_steps() {
  Pad pad;
  ASSERT_ZONE(Right, hold(pad, false, false, true, 0, 99));
  ASSERT_ZONE(NoZone, pad.update(false, false, true, 100, true)); // too early
  ASSERT_ZONE(NoZone, hold(pad, false, false, true, 101, 199));   // no step
  ASSERT_ZONE(Right, pad.update(false, false, true, 200, true));
  ASSERT_ZONE(NoZone, hold(pad, false, false, true, 201, 299));
  ASSERT_ZONE(Right, pad.update(false, false, true, 300, true));
  // Lifting stops the repeats.
  ASSERT_ZONE(NoZone, hold(pad, false, false, false, 301, 399));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 400, true));
}

void repeat_follows_slide() {
  Pad pad;
  ASSERT_ZONE(Left, hold(pad, true, false, false, 0, 199));
  ASSERT_ZONE(Left, pad.update(true, false, false, 200, true));
  // Slide across the middle to the right.
  ASSERT_ZONE(Middle, hold(pad, false, true, false, 201, 260));
  ASSERT_ZONE(Right, hold(pad, false, true, true, 261, 299));
  ASSERT_ZONE(Right, pad.update(false, true, true, 300, true));
}

void repeat_keeps_gap_after_slide() {
  Pad pad;
  ASSERT_ZONE(Left, hold(pad, true, false, false, 0, 250));
  // Middle plays once the left contact has been off for the release time.
  ASSERT_ZONE(Middle, hold(pad, false, true, false, 251, 290));
  // A step right after the slide hit is skipped (would be a flam).
  ASSERT_ZONE(NoZone, pad.update(false, true, false, 300, true));
  ASSERT_ZONE(Middle, pad.update(false, true, false, 400, true));
}

void hold_time_restarts_after_lift() {
  Pad pad;
  ASSERT_ZONE(Left, hold(pad, true, false, false, 0, 100));
  ASSERT_ZONE(NoZone, hold(pad, false, false, false, 101, 200));
  ASSERT_ZONE(Left, hold(pad, true, false, false, 300, 399));
  // Only 100 ms into the new hold.
  ASSERT_ZONE(NoZone, pad.update(true, false, false, 400, true));
  ASSERT_ZONE(Left, pad.update(true, false, false, 500, true));
}

} // namespace Tests

void setUp(void) {}
void tearDown(void) {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(Tests::zone_rules);
  RUN_TEST(Tests::plays_once_after_settling);
  RUN_TEST(Tests::landing_between_left_and_middle_plays_left_only);
  RUN_TEST(Tests::landing_between_middle_and_right_plays_right_only);
  RUN_TEST(Tests::flicker_does_not_retrigger);
  RUN_TEST(Tests::flicker_on_an_edge_contact_keeps_the_zone);
  RUN_TEST(Tests::lift_and_tap_again_plays_again);
  RUN_TEST(Tests::sliding_plays_each_new_zone);
  RUN_TEST(Tests::tap_does_not_repeat);
  RUN_TEST(Tests::hold_repeats_on_steps);
  RUN_TEST(Tests::repeat_follows_slide);
  RUN_TEST(Tests::repeat_keeps_gap_after_slide);
  RUN_TEST(Tests::hold_time_restarts_after_lift);
  return UNITY_END();
}
