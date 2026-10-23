#ifndef DRUM_PADS_H_7QK2LX
#define DRUM_PADS_H_7QK2LX

// Touch logic for one drum pad (three contacts: left, middle, right).
// No hardware access here, so it can be unit tested.
//
// - A contact plays its drum the moment it's touched (no waiting).
// - A contact counts as lifted only after it has read "off" for RELEASE_MS,
//   so a flickering contact doesn't retrigger.
// - Sliding onto a new contact plays that contact's drum.
// - Holding the pad repeats the last drum on every sequencer step, once the
//   pad has been held for REPEAT_HOLD_MS. A quick tap stays a single hit.

#include <cstdint>

namespace DrumPads {

enum Zone : int8_t { NoZone = -1, Left = 0, Middle = 1, Right = 2 };

static const uint32_t RELEASE_MS = 25;
static const uint32_t REPEAT_HOLD_MS = 150;
// A repeat never lands closer than this to the previous hit (avoids flams
// when a slide or the first hit falls just before a step).
static const uint32_t REPEAT_MIN_GAP_MS = 30;

struct Pad {
  // Call as often as possible. `step_tick` is true once per sequencer step.
  // Returns the zone to play now, or NoZone.
  Zone update(const bool left, const bool middle, const bool right,
              const uint32_t now_ms, const bool step_tick) {
    const bool raw[3] = {left, middle, right};
    bool touched[3] = {false, false, false};

    for (int i = 0; i < 3; i++) {
      if (raw[i]) {
        if (!on[i]) {
          on[i] = true;
          touched[i] = true;
        }
        last_on_ms[i] = now_ms;
      } else if (on[i] && (now_ms - last_on_ms[i]) >= RELEASE_MS) {
        on[i] = false;
      }
    }

    const bool was_held = holding;
    holding = on[0] || on[1] || on[2];

    Zone hit = NoZone;
    if (touched[Left] && touched[Right]) {
      hit = Middle; // whole finger flat across the pad
    } else if (touched[Left]) {
      hit = Left;
    } else if (touched[Right]) {
      hit = Right;
    } else if (touched[Middle]) {
      hit = Middle;
    }

    if (hit != NoZone) {
      if (!was_held) {
        hold_start_ms = now_ms;
      }
      last_zone = hit;
      last_hit_ms = now_ms;
      return hit;
    }

    if (holding && step_tick && last_zone != NoZone &&
        (now_ms - hold_start_ms) >= REPEAT_HOLD_MS &&
        (now_ms - last_hit_ms) >= REPEAT_MIN_GAP_MS) {
      last_hit_ms = now_ms;
      return last_zone;
    }

    return NoZone;
  }

  bool held() const {
    return holding;
  }

  Zone last_zone = NoZone;

private:
  bool on[3] = {false, false, false};
  uint32_t last_on_ms[3] = {0, 0, 0};
  bool holding = false;
  uint32_t hold_start_ms = 0;
  uint32_t last_hit_ms = 0;
};

} // namespace DrumPads

#endif /* end of include guard: DRUM_PADS_H_7QK2LX */
