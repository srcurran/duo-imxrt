// Mac emulator of the DUO's synth voice: runs the real firmware audio code
// (Teensy audio objects, Synth.h, Pitch.h, globals.h) on the Mac, plays a
// note pattern with given knob settings, and reports loudness.
//
// Usage: emu key=value ...   (see Params below for keys and defaults;
//        key=1..9 picks the voice by keyboard key, voice=0..8 by number)
// Prints one CSV line: the settings, then
//   lufs      integrated loudness (ITU-R BS.1770 K-weighting, no gating)
//   lufs_spk  the same after a 150 Hz high-pass (a small speaker's bass loss)
//   peak      highest output sample, 0..1 (1 = full scale)
//   clipped   output samples at full scale
// wav=path also writes the output as a mono 16-bit WAV.
//
// The output is what reaches the DAC on the speaker path: mixer_output ->
// pop_suppressor -> speaker preamp (x2), as in main.cpp. The volume knob is
// at maximum, delay and drums are off unless asked for.

#include <Arduino.h>
#include <Audio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

// The audio update loop (AudioStream.cpp); on the DUO a software interrupt.
void software_isr(void);

uint32_t emu_millis = 0;
uint32_t emu_q_flag = 0;
struct emu_dwt_t emu_dwt = {0};

// Speaker-path gains (brains2/core/boards/DUO_BRAINS_2.3/board_audio_output.h).
float MAIN_GAIN = 0.8f;
float DELAY_GAIN = 1.0f;
float KICK_GAIN = 0.8f;
float HAT_GAIN = 1.0f;
const float SPEAKER_GAIN = 2.0f;

// MidiFunctions.h's table is 12-tone equal temperament, notes 0-94.
float midi_note_to_frequency(int x) {
  if (x < 0) x = 0;
  if (x > 94) x = 94;
  return 440.0f * powf(2.0f, (x - 69) / 12.0f);
}

#include "brains2/apps/duo/globals.h"
#include "brains2/apps/duo/duo-firmware/src/Synth.h"
#include "shared/duo/Pitch.h"

// main.cpp's note_on()/note_off(), minus MIDI and LEDs.
static uint8_t note_is_playing = 0;
void note_on(uint8_t midi_note, uint8_t velocity, bool enabled) {
  if (synth.accent) velocity = 127;  // Accent overrides velocity
  note_is_playing = midi_note;
  if (enabled) {
    synth_note_pitch(midi_note, velocity);
    synth_note_start(velocity);
  }
}
void note_off() {
  if (note_is_playing) {
    synth_note_stop();
    note_is_playing = 0;
  }
}

// Collects the output blocks.
class AudioSink : public AudioStream {
 public:
  AudioSink() : AudioStream(1, queue) {}
  std::vector<int16_t> samples;
  virtual void update(void) {
    audio_block_t* b = receiveReadOnly();
    if (b) {
      samples.insert(samples.end(), b->data, b->data + AUDIO_BLOCK_SAMPLES);
      release(b);
    } else {
      samples.insert(samples.end(), AUDIO_BLOCK_SAMPLES, 0);
    }
  }

 private:
  audio_block_t* queue[1];
};

static AudioAmplifier speaker_preamp;
static AudioSink sink;
static AudioConnection out1(pop_suppressor, 0, speaker_preamp, 0);
static AudioConnection out2(speaker_preamp, 0, sink, 0);

struct Params {
  int voice = 0;
  int filter = 512, resonance = 512, release = 512, detune = 512, pw = 512;
  int volume = 1023;
  int accent = 0, glide = 0, crush = 0, delay = 0;
  int velocity = 100;  // the sequencer's INITIAL_VELOCITY
  int step_ms = 125;   // one note every step (16ths at 120 bpm)
  int gate_ms = 60;    // how long each note is held
  int seconds = 4;
  int transpose = 0;
  std::string notes = "49,54,58,63,68,70,63,54";  // keyboard keys 1,3,5,6,9,10,6,3
  std::string wav;
};

static void write_wav(const std::string& path, const std::vector<int16_t>& s) {
  FILE* f = fopen(path.c_str(), "wb");
  if (!f) return;
  const uint32_t data_bytes = s.size() * 2, rate = 44100, byte_rate = rate * 2;
  const uint32_t riff = 36 + data_bytes, fmt_len = 16;
  const uint16_t pcm = 1, ch = 1, align = 2, bits = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmt_len, 4, 1, f); fwrite(&pcm, 2, 1, f); fwrite(&ch, 2, 1, f);
  fwrite(&rate, 4, 1, f); fwrite(&byte_rate, 4, 1, f); fwrite(&align, 2, 1, f);
  fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data_bytes, 4, 1, f);
  fwrite(s.data(), 2, s.size(), f);
  fclose(f);
}

// Biquad (RBJ cookbook), direct form I.
struct Biquad {
  double b0, b1, b2, a1, a2, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  double run(double x) {
    const double y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
    x2 = x1; x1 = x; y2 = y1; y1 = y;
    return y;
  }
};
static Biquad highpass(double fc, double q) {
  const double w = 2 * M_PI * fc / 44100, c = cos(w), al = sin(w) / (2 * q), a0 = 1 + al;
  return Biquad{(1 + c) / 2 / a0, -(1 + c) / a0, (1 + c) / 2 / a0, -2 * c / a0, (1 - al) / a0};
}
static Biquad high_shelf(double fc, double gain_db, double q) {
  const double A = pow(10, gain_db / 40), w = 2 * M_PI * fc / 44100, c = cos(w);
  const double al = sin(w) / (2 * q), sa = 2 * sqrt(A) * al;
  const double a0 = (A + 1) - (A - 1) * c + sa;
  return Biquad{A * ((A + 1) + (A - 1) * c + sa) / a0, -2 * A * ((A - 1) + (A + 1) * c) / a0,
                A * ((A + 1) + (A - 1) * c - sa) / a0, 2 * ((A - 1) - (A + 1) * c) / a0,
                ((A + 1) - (A - 1) * c - sa) / a0};
}

// BS.1770 K-weighted loudness (LUFS) of the samples, optionally after a
// speaker-like high-pass. Skips the first 0.25 s.
static double loudness(const std::vector<int16_t>& s, bool speaker) {
  Biquad shelf = high_shelf(1500, 4.0, 1 / sqrt(2.0)), rlb = highpass(38, 0.5);
  Biquad spk1 = highpass(150, 0.707), spk2 = highpass(150, 0.707);
  double sum = 0;
  size_t n = 0;
  for (size_t i = 0; i < s.size(); i++) {
    double x = s[i] / 32768.0;
    if (speaker) x = spk2.run(spk1.run(x));
    x = rlb.run(shelf.run(x));
    if (i > 11025) { sum += x * x; n++; }
  }
  return n && sum > 0 ? -0.691 + 10 * log10(sum / n) : -120.0;
}

int main(int argc, char** argv) {
  Params p;
  for (int i = 1; i < argc; i++) {
    std::string a = argv[i];
    const size_t eq = a.find('=');
    if (eq == std::string::npos) continue;
    const std::string k = a.substr(0, eq), v = a.substr(eq + 1);
    const int n = atoi(v.c_str());
    if (k == "voice") p.voice = n;
    else if (k == "key") p.voice = n - 1;  // keyboard key 1-9
    else if (k == "filter") p.filter = n;
    else if (k == "resonance") p.resonance = n;
    else if (k == "release") p.release = n;
    else if (k == "detune") p.detune = n;
    else if (k == "pw") p.pw = n;
    else if (k == "volume") p.volume = n;
    else if (k == "accent") p.accent = n;
    else if (k == "glide") p.glide = n;
    else if (k == "crush") p.crush = n;
    else if (k == "delay") p.delay = n;
    else if (k == "velocity") p.velocity = n;
    else if (k == "step_ms") p.step_ms = n;
    else if (k == "gate_ms") p.gate_ms = n;
    else if (k == "seconds") p.seconds = n;
    else if (k == "transpose") p.transpose = n;
    else if (k == "notes") p.notes = v;
    else if (k == "wav") p.wav = v;
    else { fprintf(stderr, "unknown key: %s\n", k.c_str()); return 1; }
  }
  std::vector<int> notes;
  for (const char* c = p.notes.c_str(); *c;) {
    notes.push_back(atoi(c));
    while (*c && *c != ',') c++;
    if (*c == ',') c++;
  }

  // Startup, as main_init(): memory, preamp gain, audio_init().
  AudioMemory(192);
  speaker_preamp.gain(SPEAKER_GAIN);
  audio_init();

  // Knobs and switches, as pots_read().
  synth.filter = p.filter;
  synth.resonance = p.resonance;
  synth.release = p.release;
  synth.detune = p.detune;
  synth.pulseWidth = p.pw;
  synth.amplitude = p.volume;
  synth.accent = p.accent;
  synth.glide = p.glide;
  synth.crush = p.crush;
  synth.delay = p.delay;
  if (p.delay) {  // keys_scan() turning the delay on
    delay_fader.fadeIn(10);
    delay_envelope.noteOn();
  }
  transpose = p.transpose;

  synth_voice = p.voice;
  if (p.voice != VOICE_ORIGINAL) voice_apply();

  // Run block by block: the main loop's pitch_update() and synth_update(),
  // then one pass of the audio update loop (software_isr).
  const double block_ms = 1000.0 * AUDIO_BLOCK_SAMPLES / AUDIO_SAMPLE_RATE_EXACT;
  const int blocks = (int)(p.seconds * 1000 / block_ms);
  int step = -1;
  bool gate = false;
  for (int b = 0; b < blocks; b++) {
    const double t = b * block_ms;
    emu_millis = (uint32_t)t;
    const int s = (int)(t / p.step_ms);
    if (s != step) {
      step = s;
      if (gate) note_off();
      note_on(notes[s % notes.size()] + transpose, p.velocity, true);
      gate = true;
    } else if (gate && t - s * p.step_ms >= p.gate_ms) {
      note_off();
      gate = false;
    }
    pitch_update();
    synth_update();
    software_isr();
  }

  const std::vector<int16_t>& out = sink.samples;
  int peak = 0, clipped = 0;
  for (int16_t v : out) {
    const int a = abs((int)v);
    if (a > peak) peak = a;
    if (v >= 32767 || v <= -32768) clipped++;
  }
  printf("%d,%d,%d,%d,%d,%d,%d,%d,%d,%.2f,%.2f,%.3f,%d\n", p.voice, p.filter, p.resonance,
         p.release, p.detune, p.pw, p.accent, p.gate_ms, p.step_ms, loudness(out, false),
         loudness(out, true), peak / 32768.0, clipped);
  if (!p.wav.empty()) write_wav(p.wav, out);
  return 0;
}
