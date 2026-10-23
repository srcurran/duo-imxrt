#ifndef DRUMS_H_D3BUGV50
#define DRUMS_H_D3BUGV50

#include "../../pins.h"
#include "shared/duo/drum_pads.h"

// Resistive drum pads: three contacts each (left, middle, right).
// Stock DUO kick and hat sounds; each zone plays the stock tone variation
// (left: low/long kick, plain hat; right: high/short kick, snappy hat).
// Touch logic (zones, slide = new hit, hold = repeat on the step grid)
// is in shared/duo/drum_pads.h.

namespace Drums {

// Stock velocities for the left / middle / right contacts.
static const uint8_t KICK_ZONE_VELOCITY[3] = {0, 63, 127};
static const uint8_t HAT_ZONE_VELOCITY[3] = {0, 64, 127};

// Contacts are read one at a time, spread out, so no single loop pass stalls.
// Six contacts at this spacing: each pad is fully read every ~2 ms.
static const uint32_t CONTACT_READ_INTERVAL_US = 330;

static const Pin CONTACT_PINS[6] = {HAT_PAD_L_PIN,  HAT_PAD_M_PIN,  HAT_PAD_R_PIN,
                                    KICK_PAD_L_PIN, KICK_PAD_M_PIN, KICK_PAD_R_PIN};

DrumPads::Pad hat_pad;
DrumPads::Pad kick_pad;
bool contacts[6] = {false, false, false, false, false, false};
uint8_t next_contact = 0;
uint32_t last_read_us = 0;
uint32_t last_clock = 0;
bool step_pending = false;
bool kick_pad_was_held = false;

void init() {
  drum_init();
}

void update() {
  // Note sequencer steps as they pass (the clock runs even when stopped).
  const uint32_t clock = sequencer.get_clock();
  if (clock != last_clock) {
    last_clock = clock;
    if (clock % sequencer.ticks_per_step() == 0) {
      step_pending = true;
    }
  }

  const uint32_t now_us = micros();
  if (now_us - last_read_us < CONTACT_READ_INTERVAL_US) {
    return;
  }
  last_read_us = now_us;

  contacts[next_contact] = pinRead(CONTACT_PINS[next_contact]);
  next_contact++;
  if (next_contact < 6) {
    return;
  }
  next_contact = 0;

  // All six contacts read: update both pads.
  const uint32_t now = millis();
  const bool step_tick = step_pending;
  step_pending = false;

  const DrumPads::Zone hat = hat_pad.update(contacts[0], contacts[1], contacts[2], now, step_tick);
  if (hat != DrumPads::NoZone) {
    hat_noteon(HAT_ZONE_VELOCITY[hat]); // the hat rings out its short decay
  }

  const DrumPads::Zone kick = kick_pad.update(contacts[3], contacts[4], contacts[5], now, step_tick);
  if (kick != DrumPads::NoZone) {
    kick_noteon(KICK_ZONE_VELOCITY[kick]);
  }
  if (kick_pad_was_held && !kick_pad.held()) {
    kick_noteoff(); // undo the sidechain duck on the pulse osc
  }
  kick_pad_was_held = kick_pad.held();
}

} // namespace Drums

#endif /* end of include guard: DRUMS_H_D3BUGV50 */
