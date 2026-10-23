#ifndef DRUM_PADS_H_7QK2LX
#define DRUM_PADS_H_7QK2LX

// Touch logic for one drum pad (three contacts: left, middle, right).
// No hardware access here, so it can be unit tested.
//
// - Where the finger is picks one drum (zone):
//     left, left+middle        => Left
//     middle alone (or all 3)  => Middle
//     middle+right, right      => Right
// - A finger lands on two contacts a moment apart, so the zone must stay the
//   same for SETTLE_MS before it plays. This avoids playing two drums.
// - A contact counts as lifted only after it has read "off" for RELEASE_MS,
//   so a flickering contact doesn't retrigger.
// - Sliding into a different zone plays that zone's drum.
// - Holding the pad repeats the last drum on every sequencer step, once the
//   pad has been held for REPEAT_HOLD_MS. A quick tap stays a single hit.

#include <cstdint>

namespace DrumPads {

enum Zone : int8_t { NoZone = -1, Left = 0, Middle = 1, Right = 2 };

static const uint32_t SETTLE_MS = 6;
static const uint32_t RELEASE_MS = 25;
static const uint32_t REPEAT_HOLD_MS = 150;
// A repeat never lands closer than this to the previous hit (avoids flams
// when a slide or the first hit falls just before a step).
static const uint32_t REPEAT_MIN_GAP_MS = 30;

// Zone for a set of touched contacts.
inline Zone zone_of(const bool left, const bool middle, const bool right) {
  if (left && !right) {
    return Left;
  }
  if (right && !left) {
    return Right;
  }
  if (left || middle || right) {
    return Middle; // middle alone, or a finger flat across the whole pad
  }
  return NoZone;
}

struct Pad {
  // Call as often as possible. `step_tick` is true once per sequencer step.
  // Returns the zone to play now, or NoZone.
  Zone update(const bool left, const bool middle, const bool right,
              const uint32_t now_ms, const bool step_tick) {
    const bool raw[3] = {left, middle, right};
    for (int i = 0; i < 3; i++) {
      if (raw[i]) {
        on[i] = true;
        last_on_ms[i] = now_ms;
      } else if (on[i] && (now_ms - last_on_ms[i]) >= RELEASE_MS) {
        on[i] = false;
      }
    }

    const bool was_held = holding;
    holding = on[0] || on[1] || on[2];

    if (!holding) {
      candidate = NoZone;
      played = NoZone;
      return NoZone;
    }
    if (!was_held) {
      hold_start_ms = now_ms;
    }

    const Zone zone = zone_of(on[0], on[1], on[2]);
    if (zone != candidate) {
      candidate = zone;
      candidate_since_ms = now_ms;
    }

    if (candidate != played && (now_ms - candidate_since_ms) >= SETTLE_MS) {
      played = candidate;
      last_zone = candidate;
      last_hit_ms = now_ms;
      return candidate;
    }

    if (step_tick && played != NoZone &&
        (now_ms - hold_start_ms) >= REPEAT_HOLD_MS &&
        (now_ms - last_hit_ms) >= REPEAT_MIN_GAP_MS) {
      last_hit_ms = now_ms;
      return played;
    }

    return NoZone;
  }

  bool held() const {
    return holding;
  }

  // Last zone played (kept after the finger lifts).
  Zone last_zone = NoZone;

private:
  bool on[3] = {false, false, false};
  uint32_t last_on_ms[3] = {0, 0, 0};
  bool holding = false;
  Zone candidate = NoZone;
  uint32_t candidate_since_ms = 0;
  Zone played = NoZone; // zone played during the current hold
  uint32_t hold_start_ms = 0;
  uint32_t last_hit_ms = 0;
};

} // namespace DrumPads

#endif /* end of include guard: DRUM_PADS_H_7QK2LX */
