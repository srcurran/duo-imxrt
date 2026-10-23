#ifndef DRUMS_H_D3BUGV50
#define DRUMS_H_D3BUGV50

#include "../../pins.h"
#include "shared/duo/drum_pads.h"

// Resistive drum pads: three contacts each (left, middle, right).
//   Kick pad: kick / tom / snare
//   Hat pad:  closed hat / clap / open hat
// Touch logic (instant hits, slide = new hit, hold = repeat on the step grid)
// is in shared/duo/drum_pads.h.

namespace Drums {

static const DrumKit::Drum KICK_PAD_DRUMS[3] = {DrumKit::Kick, DrumKit::Tom, DrumKit::Snare};
static const DrumKit::Drum HAT_PAD_DRUMS[3] = {DrumKit::ClosedHat, DrumKit::Clap, DrumKit::OpenHat};

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
    drum_hit(HAT_PAD_DRUMS[hat]);
  }

  const DrumPads::Zone kick = kick_pad.update(contacts[3], contacts[4], contacts[5], now, step_tick);
  if (kick != DrumPads::NoZone) {
    drum_hit(KICK_PAD_DRUMS[kick]);
  }
  if (kick_pad_was_held && !kick_pad.held()) {
    drum_kick_pad_released();
  }
  kick_pad_was_held = kick_pad.held();
}

} // namespace Drums

#endif /* end of include guard: DRUMS_H_D3BUGV50 */
