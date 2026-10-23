#ifndef DRUM_PADS_H_7QK2LX
#define DRUM_PADS_H_7QK2LX

// Touch logic for one drum pad (three contacts: left, middle, right).
// No hardware access here, so it can be unit tested.
//
// - Where the finger is picks one drum (zone):
//     left, left+middle        => Left
//     middle alone (or all 3)  => Middle
//     middle+right, right      => Right
//   One finger at a time. A second finger on the far edge plays that edge.
// - A finger lands on two contacts a moment apart, so the zone must stay the
//   same for SETTLE_MS before it plays. This avoids playing two drums.
// - A contact counts as lifted only after it has read "off" for RELEASE_MS,
//   so a flickering contact doesn't retrigger.
// - Sliding into a different zone plays that zone's drum. Once the middle
//   has played, the finger must leave the middle contact to reach an edge
//   (a finger on the middle often brushes a neighbour).
// - Lifting a finger never plays a drum.
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
        if (!on[i]) {
          touched_ms[i] = now_ms;
        }
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

    Zone zone;
    if (on[Left] && on[Right]) {
      // Both edges: a flat finger if they were touched together, otherwise a
      // second finger, which picks its own edge.
      const int32_t newer = (int32_t)(touched_ms[Right] - touched_ms[Left]);
      if (newer < (int32_t)SETTLE_MS && newer > -(int32_t)SETTLE_MS) {
        zone = Middle;
      } else {
        zone = newer > 0 ? Right : Left;
      }
    } else {
      zone = zone_of(on[0], on[1], on[2]);
      if (played == Middle && on[Middle]) {
        // Resting on the middle: brushing a neighbouring contact doesn't
        // count as a slide. Only leaving the middle contact moves to an edge.
        zone = Middle;
      }
    }
    if (zone != candidate) {
      candidate = zone;
      candidate_since_ms = now_ms;
    }

    if (candidate != played && (now_ms - candidate_since_ms) >= SETTLE_MS) {
      const bool first_hit = played == NoZone;
      played = candidate;
      // Only a contact touched since the last hit can play (a slide or a new
      // finger). Lifting a finger changes the zone without playing.
      if (first_hit || touched_since_last_hit()) {
        last_zone = candidate;
        last_hit_ms = now_ms;
        return candidate;
      }
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
  bool touched_since_last_hit() const {
    for (int i = 0; i < 3; i++) {
      if (on[i] && (int32_t)(touched_ms[i] - last_hit_ms) > 0) {
        return true;
      }
    }
    return false;
  }

  bool on[3] = {false, false, false};
  uint32_t touched_ms[3] = {0, 0, 0};
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
