#define VERSION "1.2.0-beta1"
const uint8_t FIRMWARE_VERSION[] = {1, 2, 0};

// #define DEV_MODE

int MIDI_CHANNEL = 1;

// Musical settings
const uint8_t SCALE[] = {49, 51, 54, 56, 58, 61, 63, 66, 68, 70};
const uint8_t SCALE_OFFSET_FROM_C3[]{1, 3, 6, 8, 10, 13, 15, 18, 20, 22};

#define HIGH_SAMPLE_RATE 44100.0f
#define LOW_SAMPLE_RATE 2489.0f

#define EEPROM_MIDI_CHANNEL 0
#define USB_STARTUP_DELAY_MS 280

// Globals that should not be globals

// Sequencer settings
float osc_saw_frequency = 0.;
float osc_pulse_frequency = 0.;
float osc_pulse_target_frequency = 0.;
float osc_saw_target_frequency = 0.;
uint8_t osc_pulse_midi_note = 0;
int transpose = 0;
bool random_flag = 0;
// Page mode: Random (BTN_SEQ1) pressed and held while the sequencer is
// stopped. Keys 1-4 set the length, Up/Down change page, and step buttons
// move the cursor. While running, Random works as normal and this stays off.
bool page_mode_held = false;
// Keyboard keys (bit per key) pressed as part of a length combo. Their release
// is swallowed too, so they never play or release a note.
uint16_t combo_keys_held = 0;
// Voices: hold Accent (voice mode) and press key 1-9 (stopped) or Up/Down.
// Original is the stock DUO sound and the default at power-on. Each voice is
// described in Synth.h (voice_apply).
enum Voice : uint8_t {
  VOICE_ORIGINAL = 0,
  VOICE_ORCHID,
  VOICE_ORGAN,
  VOICE_STRINGS,
  VOICE_ACID,
  VOICE_PLUCK,
  VOICE_FM,
  VOICE_FOLD,
  VOICE_CHORD,
  VOICE_COUNT  // 9: keys 1-9; key 10 still plays in voice mode
};
uint8_t synth_voice = VOICE_ORIGINAL;
// Voice mode: Accent held. Stopped: keys 1-9 pick a voice. Stopped or
// playing: Up/Down step to the next/previous voice (instead of transposing).
// While playing the keys play as normal (accented).
bool voice_mode_held = false;
bool dfu_flag = 0;
bool in_setup = true;

void note_on(uint8_t midi_note, uint8_t velocity, bool enabled);
void note_off();
void enter_dfu();

bool power_flag = true;

#include "shared/duo/synth_params.h"
synth_parameters synth;

void sequencer_note_on(uint8_t midi_note, uint8_t velocity) {
  note_on(midi_note + transpose, velocity, true);
}
#include "shared/duo/seq.h"

void sequencer_randomize_step_offset(Sequencer::Sequencer& seq) {
  // Jump forward by 1..length-3 steps (1..5 at the default length of 8).
  const long max_jump = seq.get_length() > 3 ? seq.get_length() - 2 : 2;
  const uint8_t offset = seq.get_step_offset() + random(1, max_jump);
  seq.set_step_offset(offset);
}

void sequencer_on_running_advance(Sequencer::Sequencer &seq) {
  if (random_flag) {
    sequencer_randomize_step_offset(seq);
  } else {
    seq.set_step_offset(0);
  }
}

Sequencer::Sequencer
    sequencer(Sequencer::Output::Callbacks{.note_on = sequencer_note_on,
                                           .note_off = note_off},
              sequencer_on_running_advance);

// True while page mode is active (see page_mode_held). Ends as soon as the
// sequencer starts running, even if Random is still held.
bool in_page_mode() {
  return page_mode_held && !sequencer.is_running();
}

// True while voice mode is active (see voice_mode_held).
bool in_voice_mode() {
  return voice_mode_held;
}
