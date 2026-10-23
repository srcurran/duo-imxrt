#include "drum_pads.h"
#include "unity.h"

using DrumPads::Left;
using DrumPads::Middle;
using DrumPads::NoZone;
using DrumPads::Pad;
using DrumPads::Right;

#define ASSERT_ZONE(expected, actual) TEST_ASSERT_EQUAL_INT((int)(expected), (int)(actual))

namespace Tests {

void plays_on_first_contact() {
  Pad pad;
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 0, false));
  ASSERT_ZONE(Left, pad.update(true, false, false, 1, false));
  // Holding doesn't play again.
  ASSERT_ZONE(NoZone, pad.update(true, false, false, 2, false));
  ASSERT_ZONE(NoZone, pad.update(true, false, false, 50, false));
}

void each_contact_has_its_zone() {
  Pad a, b, c;
  ASSERT_ZONE(Left, a.update(true, false, false, 0, false));
  ASSERT_ZONE(Middle, b.update(false, true, false, 0, false));
  ASSERT_ZONE(Right, c.update(false, false, true, 0, false));
}

void two_contacts_at_once_pick_the_edge() {
  Pad a, b, c;
  ASSERT_ZONE(Left, a.update(true, true, false, 0, false));
  ASSERT_ZONE(Right, b.update(false, true, true, 0, false));
  // Whole finger flat across the pad.
  ASSERT_ZONE(Middle, c.update(true, true, true, 0, false));
}

void flicker_does_not_retrigger() {
  Pad pad;
  ASSERT_ZONE(Middle, pad.update(false, true, false, 0, false));
  // Contact drops out briefly (shorter than RELEASE_MS) and comes back.
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 5, false));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 15, false));
  ASSERT_ZONE(NoZone, pad.update(false, true, false, 20, false));
  TEST_ASSERT_TRUE(pad.held());
}

void lift_and_tap_again_plays_again() {
  Pad pad;
  ASSERT_ZONE(Middle, pad.update(false, true, false, 0, false));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 10, false));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 10 + DrumPads::RELEASE_MS, false));
  TEST_ASSERT_FALSE(pad.held());
  ASSERT_ZONE(Middle, pad.update(false, true, false, 60, false));
}

void sliding_plays_each_new_zone() {
  Pad pad;
  ASSERT_ZONE(Left, pad.update(true, false, false, 0, false));
  // Finger rolls onto the middle while still on the left.
  ASSERT_ZONE(Middle, pad.update(true, true, false, 8, false));
  // Leaves the left, stays on the middle: nothing new.
  ASSERT_ZONE(NoZone, pad.update(false, true, false, 16, false));
  ASSERT_ZONE(Right, pad.update(false, true, true, 24, false));
  ASSERT_ZONE(NoZone, pad.update(false, false, true, 32, false));
  TEST_ASSERT_EQUAL_INT(Right, pad.last_zone);
}

void sliding_back_after_release_plays_again() {
  Pad pad;
  ASSERT_ZONE(Left, pad.update(true, false, false, 0, false));
  ASSERT_ZONE(Middle, pad.update(false, true, false, 10, false));
  // Left has been off long enough to count as lifted.
  ASSERT_ZONE(NoZone, pad.update(false, true, false, 10 + DrumPads::RELEASE_MS, false));
  ASSERT_ZONE(Left, pad.update(true, true, false, 50, false));
}

void tap_does_not_repeat() {
  Pad pad;
  ASSERT_ZONE(Left, pad.update(true, false, false, 0, false));
  // A step arrives while the finger is still down, but too soon.
  ASSERT_ZONE(NoZone, pad.update(true, false, false, 60, true));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 80, false));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 200, true));
}

void hold_repeats_on_steps() {
  Pad pad;
  ASSERT_ZONE(Right, pad.update(false, false, true, 0, false));
  ASSERT_ZONE(NoZone, pad.update(false, false, true, 100, true)); // too early
  ASSERT_ZONE(NoZone, pad.update(false, false, true, 150, false)); // no step
  ASSERT_ZONE(Right, pad.update(false, false, true, 200, true));
  ASSERT_ZONE(NoZone, pad.update(false, false, true, 210, false));
  ASSERT_ZONE(Right, pad.update(false, false, true, 300, true));
  // Lifting stops the repeats.
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 310, false));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 400, true));
}

void repeat_follows_slide() {
  Pad pad;
  ASSERT_ZONE(Left, pad.update(true, false, false, 0, false));
  ASSERT_ZONE(Left, pad.update(true, false, false, 200, true));
  ASSERT_ZONE(Middle, pad.update(true, true, false, 250, false));
  ASSERT_ZONE(Middle, pad.update(false, true, false, 300, true));
}

void repeat_keeps_gap_after_slide() {
  Pad pad;
  ASSERT_ZONE(Left, pad.update(true, false, false, 0, false));
  ASSERT_ZONE(Middle, pad.update(true, true, false, 290, false));
  // A step right after the slide hit is skipped (would be a flam).
  ASSERT_ZONE(NoZone, pad.update(true, true, false, 300, true));
  ASSERT_ZONE(Middle, pad.update(true, true, false, 400, true));
}

void hold_time_restarts_after_lift() {
  Pad pad;
  ASSERT_ZONE(Left, pad.update(true, false, false, 0, false));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 100, false));
  ASSERT_ZONE(NoZone, pad.update(false, false, false, 200, false));
  ASSERT_ZONE(Left, pad.update(true, false, false, 300, false));
  // Only 100 ms into the new hold.
  ASSERT_ZONE(NoZone, pad.update(true, false, false, 400, true));
  ASSERT_ZONE(Left, pad.update(true, false, false, 500, true));
}

} // namespace Tests

void setUp(void) {}
void tearDown(void) {}

int main() {
  UNITY_BEGIN();
  RUN_TEST(Tests::plays_on_first_contact);
  RUN_TEST(Tests::each_contact_has_its_zone);
  RUN_TEST(Tests::two_contacts_at_once_pick_the_edge);
  RUN_TEST(Tests::flicker_does_not_retrigger);
  RUN_TEST(Tests::lift_and_tap_again_plays_again);
  RUN_TEST(Tests::sliding_plays_each_new_zone);
  RUN_TEST(Tests::sliding_back_after_release_plays_again);
  RUN_TEST(Tests::tap_does_not_repeat);
  RUN_TEST(Tests::hold_repeats_on_steps);
  RUN_TEST(Tests::repeat_follows_slide);
  RUN_TEST(Tests::repeat_keeps_gap_after_slide);
  RUN_TEST(Tests::hold_time_restarts_after_lift);
  return UNITY_END();
}
