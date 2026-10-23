#ifndef TONE_TEST_H
#define TONE_TEST_H

// Tone test, for calibrating the Mac emulator (tools/emu) against a dB-meter
// reading of the DUO's speaker. Only in builds made with DUO_TONE_TEST=1 set
// (see CMakeLists.txt); the Mac emulator runs the same code (emu test=1).
//
// Press Play and the DUO runs this timetable on its own, ignoring the knobs:
//   start signal  1 kHz 2 s, silence 2 s, 1 kHz 2 s, silence 4 s
//   tones         14 sine tones, 100 Hz - 8 kHz, each 4 s + 2 s silence
//   marker        8 s silence in all
//   voices        keys 1-10 in order, each 6 s of a fixed 16th-note pattern
//                 with every knob at noon, + 2 s silence
//   end signal    1 kHz 2 s
// Every tone has the same electrical level. Play again stops the test.
//
// Needs (from the including file): Synth.h, Pitch.h, globals.h, millis(),
// note_on() and note_off().

namespace ToneTest {

enum Kind : uint8_t { SILENCE, TONE, VOICE };
struct Step {
  uint16_t ms;
  Kind kind;
  uint16_t hz;    // TONE
  uint8_t voice;  // VOICE: 0-9 (keys 1-10)
};

static const Step STEPS[] = {
    // start signal
    {2000, TONE, 1000, 0}, {2000, SILENCE, 0, 0}, {2000, TONE, 1000, 0}, {4000, SILENCE, 0, 0},
    // tones
    {4000, TONE, 100, 0}, {2000, SILENCE, 0, 0}, {4000, TONE, 150, 0}, {2000, SILENCE, 0, 0},
    {4000, TONE, 200, 0}, {2000, SILENCE, 0, 0}, {4000, TONE, 300, 0}, {2000, SILENCE, 0, 0},
    {4000, TONE, 400, 0}, {2000, SILENCE, 0, 0}, {4000, TONE, 500, 0}, {2000, SILENCE, 0, 0},
    {4000, TONE, 700, 0}, {2000, SILENCE, 0, 0}, {4000, TONE, 1000, 0}, {2000, SILENCE, 0, 0},
    {4000, TONE, 1500, 0}, {2000, SILENCE, 0, 0}, {4000, TONE, 2000, 0}, {2000, SILENCE, 0, 0},
    {4000, TONE, 3000, 0}, {2000, SILENCE, 0, 0}, {4000, TONE, 4000, 0}, {2000, SILENCE, 0, 0},
    {4000, TONE, 6000, 0}, {2000, SILENCE, 0, 0}, {4000, TONE, 8000, 0}, {2000, SILENCE, 0, 0},
    // marker: 2 + 6 = 8 s silence
    {6000, SILENCE, 0, 0},
    // voices, keys 1-10
    {6000, VOICE, 0, 0}, {2000, SILENCE, 0, 0}, {6000, VOICE, 0, 1}, {2000, SILENCE, 0, 0},
    {6000, VOICE, 0, 2}, {2000, SILENCE, 0, 0}, {6000, VOICE, 0, 3}, {2000, SILENCE, 0, 0},
    {6000, VOICE, 0, 4}, {2000, SILENCE, 0, 0}, {6000, VOICE, 0, 5}, {2000, SILENCE, 0, 0},
    {6000, VOICE, 0, 6}, {2000, SILENCE, 0, 0}, {6000, VOICE, 0, 7}, {2000, SILENCE, 0, 0},
    {6000, VOICE, 0, 8}, {2000, SILENCE, 0, 0}, {6000, VOICE, 0, 9}, {2000, SILENCE, 0, 0},
    // end signal
    {2000, TONE, 1000, 0},
};
static const int NUM_STEPS = sizeof(STEPS) / sizeof(STEPS[0]);

// Fixed settings: volume knob ~80%, tone level well below full scale.
static const int VOLUME = 800;
static const float TONE_LEVEL = 0.25f;
// Voice pattern: keyboard keys 1,3,5,6,9,10,6,3 as 16ths at 120 bpm.
static const uint8_t NOTES[] = {49, 54, 58, 63, 68, 70, 63, 54};
static const uint32_t NOTE_STEP_MS = 125;
static const uint32_t NOTE_GATE_MS = 60;

static bool running = false;
static int step = 0;
static uint32_t step_start = 0;
static int note_index = -1;
static bool gate = false;

inline bool is_running() { return running; }
inline int current_step() { return step; }

// Knobs at noon, switches off, fixed volume.
static void fix_knobs() {
  synth.filter = synth.resonance = synth.release = 512;
  synth.detune = synth.pulseWidth = 512;
  synth.amplitude = VOLUME;
  synth.gateLength = (NOTE_GATE_MS - 10) * 1023 / 190;  // gate knob = the notes' length
  synth.glide = synth.crush = synth.accent = synth.delay = false;
}

static void stop_note() {
  if (gate) {
    note_off();
    gate = false;
  }
}

// A plain sine through the voice path with the filter well open: osc3 into
// mixer1, no envelope sweep, no chorus/reverb, no delay.
static void start_tone(const float hz) {
  synth_voice = VOICE_ORIGINAL;
  voice_apply();
  AudioNoInterrupts();
  osc_pulse.amplitude(0.0f);
  osc_saw.amplitude(0.0f);
  osc3.begin(WAVEFORM_SINE);
  osc3.frequency(hz);
  osc3.amplitude(TONE_LEVEL);
  mixer1.gain(2, 1.0f);
  dc1.amplitude(0.0f);  // no filter envelope: cutoff stays at the base
  // 7 kHz: this filter goes unstable (oscillates, clips) above ~8.4 kHz at
  // the lowest resonance. It dims the 6 and 8 kHz tones a little; the
  // emulator measures exactly how much, and the analysis allows for it.
  filter1.frequency(7000);
  filter1.resonance(0.707f);
  bitcrusher1.sampleRate(HIGH_SAMPLE_RATE);
  envelope1.release(30);
  audio_volume(VOLUME);
  mixer_output.gain(1, 0.0f);  // delay out
  AudioInterrupts();
  envelope1.noteOn();
}

static void enter_step() {
  const Step& s = STEPS[step];
  stop_note();
  note_index = -1;
  if (s.kind == TONE) {
    start_tone(s.hz);
  } else if (s.kind == VOICE) {
    envelope1.noteOff();
    fix_knobs();
    synth_voice = s.voice;
    voice_apply();
  } else {
    envelope1.noteOff();
  }
}

inline void start() {
  running = true;
  step = 0;
  step_start = millis();
  gate = false;
  enter_step();
}

// Back to normal playing: Original voice, knobs read again by pots_read().
inline void stop() {
  stop_note();
  envelope1.noteOff();
  running = false;
  synth_voice = VOICE_ORIGINAL;
  voice_apply();
}

// Call every main-loop pass while running (instead of the normal synth and
// sequencer updates).
inline void update() {
  if (!running) return;
  const uint32_t now = millis();
  while (now - step_start >= STEPS[step].ms) {
    step_start += STEPS[step].ms;
    if (++step >= NUM_STEPS) {
      stop();
      return;
    }
    enter_step();
  }
  if (STEPS[step].kind == VOICE) {
    const uint32_t t = now - step_start;
    const int n = t / NOTE_STEP_MS;
    if (n != note_index) {
      stop_note();
      note_index = n;
      note_on(NOTES[n % sizeof(NOTES)], 100, true);
      gate = true;
    } else if (gate && t - n * NOTE_STEP_MS >= NOTE_GATE_MS) {
      stop_note();
    }
    fix_knobs();
    pitch_update();
    synth_update();
    AudioNoInterrupts();
    mixer_output.gain(1, 0.0f);  // delay out
    AudioInterrupts();
  }
}

}  // namespace ToneTest

#endif /* TONE_TEST_H */
