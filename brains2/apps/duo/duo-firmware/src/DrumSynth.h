/*
  DATO DUO firmware - DrumSynth
  (c) 2017, Benjamin Brandenburg <bnjmn7@gmail.com>

  The drums themselves are synthesized in shared/duo/drum_kit.cpp and played
  through `drum_kit` (Synth.h). This file holds the small helpers the pad code
  calls.
*/

void drum_init() {
}

// Plays one drum. While a kick-pad drum plays, the pulse oscillator is ducked
// a little, as in the stock firmware ("sidechain").
void drum_hit(DrumKit::Drum drum, float level = 1.0f) {
  if (drum <= DrumKit::Snare) {
    AudioNoInterrupts();
    osc_pulse.amplitude(0.35f);
    AudioInterrupts();
  }
  drum_kit.trigger(drum, level);
}

// Kick pad lifted: undo the sidechain duck.
void drum_kick_pad_released() {
  AudioNoInterrupts();
  osc_pulse.amplitude(0.4f);
  AudioInterrupts();
}

// Older boards (capacitive pads) still use the stock note on/off calls.
bool kick_playing = 0;
bool hat_playing = 0;

void kick_noteon(uint8_t velocity) {
  (void)velocity;
  drum_hit(DrumKit::Kick);
  kick_playing = 1;
}

void kick_noteoff() {
  drum_kick_pad_released();
  kick_playing = 0;
}

void hat_noteon(uint8_t velocity) {
  drum_hit(velocity > 63 ? DrumKit::OpenHat : DrumKit::ClosedHat);
  hat_playing = 1;
}

void hat_noteoff() {
  hat_playing = 0;
}
