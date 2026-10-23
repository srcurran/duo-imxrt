#ifndef Synth_h
#define Synth_h

#include <analyze_peak.h>
#include <effect_bitcrusher.h>
#include <effect_delay.h>
#include <effect_fade.h>
#include "shared/duo/effect_custom_envelope.h"
#include "shared/duo/effect_lush.h"
#include "shared/duo/effect_fold.h"
#include "shared/duo/synth_pluck.h"
#include <effect_multiply.h>
#include <filter_variable.h>
#include <mixer.h>
#include <synth_waveform.h>
#include <synth_dc.h>
#include <synth_whitenoise.h>
#include <synth_pinknoise.h>
#include <synth_simple_drum.h>

// GUItool: begin automatically generated code
AudioSynthWaveform       osc_pulse;      //xy=78.10000610351562,97
AudioSynthWaveform       osc_saw;        //xy=79.10000610351562,44
// Extra sound sources for the voices (see voice_apply). All silent, and idle,
// in the Original voice. Declared before mixer1 so they add no latency.
AudioSynthWaveform       osc3;           // Organ, Strings, Chord
AudioSynthWaveform       osc4;           // Strings, Chord
AudioSynthWaveform       fm_mod;         // FM: modulator
AudioEffectCustomEnvelope fm_mod_env;    // FM: modulator envelope (the "tine")
AudioSynthWaveformModulated fm_carrier;  // FM: carrier
AudioSynthPluck          pluck;          // Pluck: plucked string
AudioMixer4              voice_mixer;
AudioSynthWaveformDc     dc1;            //xy=88.10000610351562,149
AudioEffectCustomEnvelope envelope2;      //xy=227.10000610351562,149
AudioMixer4              mixer1;         //xy=255.10000610351562,83
// Wavefolder for the Fold voice. Passes audio through untouched otherwise.
AudioEffectFold          fold_fx;
AudioFilterStateVariable filter1;        //xy=403.1000061035156,91
AudioEffectCustomEnvelope envelope1;      //xy=560.1000061035156,81
AudioEffectCustomEnvelope delay_envelope;      //xy=560.1000061035156,81
AudioAnalyzePeak         peak1;          //xy=705.1000061035156,37
AudioEffectDelay         delay1;         //xy=712.0999755859375,174.10000610351562
AudioFilterStateVariable delay_filter;         //xy=712.0999755859375,174.10000610351562
AudioEffectFade          delay_fader;
AudioEffectBitcrusher    bitcrusher1;    //xy=718.1000061035156,81
// Chorus + reverb for the Orchid voice. Passes audio through untouched for
// the original voice. Declared straight after bitcrusher1 so it adds no latency.
AudioEffectLush          lush_fx;
AudioMixer4              mixer_delay;     //xy=728.0999755859375,279.1000061035156
AudioMixer4              mixer_output;         //xy=861.1000061035156,100
AudioEffectFade          pop_suppressor;          //xy=1062,292
AudioAnalyzePeak         peak2;          //xy=987.1000061035156,151
AudioSynthNoiseWhite     hat_noise1;
AudioSynthSimpleDrum     kick_drum1;
AudioEffectCustomEnvelope hat_envelope1;
AudioSynthSimpleDrum     hat_snappy;
AudioFilterStateVariable hat_filter_bp;
AudioFilterStateVariable hat_filter_hp;
AudioMixer4              hat_mixer;
AudioConnection          patchCord1(osc_pulse, 0, mixer1, 1);
AudioConnection          patchCord2(osc_saw, 0, mixer1, 0);
AudioConnection          patchCord3(dc1, envelope2);
AudioConnection          patchCord4(envelope2, 0, filter1, 1);
AudioConnection          patchCord5a(mixer1, 0, fold_fx, 0);
AudioConnection          patchCord5(fold_fx, 0, filter1, 0);
AudioConnection          patchVoice1(osc3, 0, voice_mixer, 0);
AudioConnection          patchVoice2(osc4, 0, voice_mixer, 1);
AudioConnection          patchVoice3(fm_mod, fm_mod_env);
AudioConnection          patchVoice4(fm_mod_env, 0, fm_carrier, 0);
AudioConnection          patchVoice5(fm_carrier, 0, voice_mixer, 2);
AudioConnection          patchVoice6(pluck, 0, voice_mixer, 3);
AudioConnection          patchVoice7(voice_mixer, 0, mixer1, 2);
AudioConnection          patchCord6(filter1, 0, envelope1, 0);
AudioConnection          patchCord7(envelope1, peak1);
AudioConnection          patchCord8(envelope1, bitcrusher1);

// Feedback path
AudioConnection          patchCord9(delay1, 0, delay_filter, 0);
AudioConnection          patchCord10(delay_fader, 0, mixer_delay, 1);
AudioConnection          patchCord11(delay1, 0, mixer_output, 1); 
AudioConnection          patchCord12(mixer_delay, delay1);
AudioConnection          patchCord13(delay_filter, 2, delay_fader, 0);



AudioConnection          patchCord14a(bitcrusher1, 0, lush_fx, 0);
AudioConnection          patchCord14(lush_fx, 0, mixer_output, 0);
AudioConnection          patchCord15(lush_fx, 0, delay_envelope, 0);
AudioConnection          patchCord16(delay_envelope, 0, mixer_delay, 0);
// AudioConnection          patchCord13(mixer_delay, delay1);

AudioConnection          patchCord21(hat_noise1, hat_envelope1);
AudioConnection          patchCord22(hat_envelope1, 0, hat_filter_hp, 0);
AudioConnection          patchCord23(hat_filter_hp, 2, hat_filter_bp, 0);
AudioConnection          patchCord24(hat_filter_bp, 1, hat_mixer, 0);
AudioConnection          patchCord25(hat_snappy, 0, hat_mixer, 1);
AudioConnection          patchCord26(hat_mixer, 0, mixer_delay, 3);
AudioConnection          patchCord27(kick_drum1, 0, mixer_output, 2);
AudioConnection          patchCord28(hat_mixer, 0, mixer_output, 3);
AudioConnection          patchCord29(mixer_output, pop_suppressor);
AudioConnection          patchCord30(mixer_output, peak2);
// GUItool: end automatically generated code

void audio_init();
void audio_volume(int volume);
void synth_update();
void voice_apply();
void voice_note_on(uint8_t velocity);
void voice_note_off();

void audio_init() {
  // assume AudioNoInterrupts
  AudioMemory(192); // 260 bytes per block, 2.9ms per block
  
  // Mixer mixes the oscillators - don't add up to more than 0.8 or the output will clip
  #if defined(__IMXRT1011__)
  mixer1.gain(0, 0.2f); // OSC1
  osc_saw.begin(0.4f, 110, WAVEFORM_BANDLIMIT_SAWTOOTH);
  osc_pulse.pulseWidth(0.5f);
  osc_pulse.begin(.5f, 220, WAVEFORM_BANDLIMIT_PULSE);
  #else
  osc_saw.begin(0.4f, 110, WAVEFORM_SAWTOOTH);
  osc_pulse.pulseWidth(0.5f);
  osc_pulse.begin(.4f, 220, WAVEFORM_PULSE);
  mixer1.gain(0, 0.4f);
  #endif
  mixer1.gain(1, 0.4f); // OSC2
  mixer1.gain(2, 0.0f); // Extra voice sources, off in the Original voice

  // Extra voice sources: all start silent (amplitude 0 = idle).
  for (int i = 0; i < 4; i++) {
    voice_mixer.gain(i, 1.0f);
  }
  osc3.begin(0.0f, 220, WAVEFORM_SINE);
  osc4.begin(0.0f, 220, WAVEFORM_SINE);
  fm_mod.begin(0.0f, 220, WAVEFORM_SINE);
  fm_carrier.begin(0.0f, 220, WAVEFORM_SINE);
  fm_carrier.phaseModulation(720);  // full-scale modulator = +/- 2 turns
  fm_mod_env.attack(1);
  fm_mod_env.decay(400);
  fm_mod_env.sustain(0.25f);
  fm_mod_env.release(300);

  // Filter
  filter1.resonance(0.7f); // range 0.7-5.0
  filter1.frequency(400);
  filter1.octaveControl(4);

  // Amp envelope
  envelope1.attack(2);
  envelope1.decay(0);
  envelope1.sustain(1.0f);
  envelope1.release(400);

  // Filter envelope
  dc1.amplitude(1.0); // Filter env needs an input signal
  envelope2.attack(15);
  envelope2.decay(0);
  envelope2.sustain(1.0f);
  envelope2.release(300);

  // Bitcrusher - set to audio samplerate and max bitrate
  bitcrusher1.bits(16);
  bitcrusher1.sampleRate(44100);

  delay1.delay(0, 350); // Delay time
  delay_filter.frequency(400); // High pass filter in feedback

  delay_envelope.attack(15);
  delay_envelope.sustain(1.0f);
  delay_envelope.release(15);
  mixer_delay.gain(0, 0.5f); // Delay input
  mixer_delay.gain(1, 0.4f); // Delay feedback

  mixer_output.gain(0, MAIN_GAIN);
  mixer_output.gain(1, DELAY_GAIN); // Delay output
  mixer_output.gain(2, KICK_GAIN); // Kick output
  mixer_output.gain(3, HAT_GAIN); // Hat output
  
  /* 
    Sets the reference voltage for the internal DAC, which
    in turn determines the peak to peak voltage of the audio
    signal.

    Clipping and other distortion may occur in SUPER_LOUD_MODE
    so use with caution.

    INTERNAL = 1.1V internal reference
    EXTERNAL = 3.3V supply voltage
    */
  #ifdef SUPER_LOUD_MODE
    //dac1.analogReference(EXTERNAL);
  #else
    //dac1.analogReference(INTERNAL);
  #endif
}

inline void audio_volume(int volume) {
  static const int LOW_VOLUME_THRESHOLD = 4;

  if(volume < LOW_VOLUME_THRESHOLD) {
    mixer_output.gain(0,0);
    mixer_output.gain(1,0);
    mixer_output.gain(2,0);
    mixer_output.gain(3,0);
  } else {
    mixer_output.gain(0, (volume/(1023.f/MAIN_GAIN)));
    mixer_output.gain(1, (volume/(1023.f/DELAY_GAIN)));
    mixer_output.gain(2, ((volume+512)/(2048.f/KICK_GAIN)));
    mixer_output.gain(3, ((volume+512)/(2048.f/HAT_GAIN)));
  }
}

// ---- Voices ----------------------------------------------------------------
// Hold Accent while stopped and press keyboard key 1-9 (see globals.h).
// Every voice runs through the same filter, envelopes, bitcrusher and delay,
// so those knobs and buttons work the same everywhere. Detune and Pulse Width
// are each voice's two "character" knobs:
//
//   key  voice     Detune knob                 Pulse Width knob
//   1    Original  interval (stock)            pulse width (stock)
//   2    Orchid    interval                    pulse width
//   3    Organ     upper drawbar (2 2/3'..1')  upper drawbar level
//   4    Strings   ensemble spread             octave-up level
//   5    Acid      filter decay                saw <-> square
//   6    Pluck     ring length                 brightness
//   7    FM        frequency ratio             FM depth
//   8    Fold      interval                    second wave level
//                  (and the Filter knob sets the fold amount)
//   9    Chord     chord type                  4th note level

static const LushFx::Params ORGAN_LUSH = {0.55f, 0.75f, 0.25f, 6.7f, 0.35f, 7.3f, 0.25f, 0.9f};
static const LushFx::Params STRINGS_LUSH = {0.5f, 0.85f, 0.35f, 0.6f, 3.0f, 5.8f, 0.3f, 0.85f};
static const LushFx::Params PLUCK_LUSH = {0.9f, 0.2f, 0.4f, 0.4f, 1.5f, 0.6f, 1.5f, 0.85f};
static const LushFx::Params FM_LUSH = {0.75f, 0.5f, 0.3f, 0.7f, 1.5f, 1.1f, 1.5f, 0.85f};

// Organ: upper drawbar, as a multiple of the fundamental.
static const float ORGAN_DRAWBARS[] = {3.0f, 4.0f, 6.0f, 8.0f};
// FM: modulator/carrier ratios. 1-4 are harmonic (electric piano, organ-ish,
// hollow), 3.5 and 1.41 are bells and metal, 0.5 a growly bass.
static const float FM_RATIOS[] = {1.0f, 2.0f, 3.0f, 4.0f, 0.5f, 3.5f, 1.41f, 7.0f};
// Chord: frequency ratios of the 2nd, 3rd and 4th notes above the root.
// maj, min, sus2, sus4 (4th note = octave), maj7, min7, dom7, min9.
static const float CHORD_RATIOS[][3] = {
    {1.259921f, 1.498307f, 2.0f},       // major
    {1.189207f, 1.498307f, 2.0f},       // minor
    {1.122462f, 1.498307f, 2.0f},       // sus2
    {1.334840f, 1.498307f, 2.0f},       // sus4
    {1.259921f, 1.498307f, 1.887749f},  // major 7
    {1.189207f, 1.498307f, 1.781797f},  // minor 7
    {1.259921f, 1.498307f, 1.781797f},  // dominant 7
    {1.189207f, 1.498307f, 2.244924f},  // minor add 9
};

// Picks one of `count` entries from a 0-1023 knob.
static inline int knob_index(const int knob, const int count) {
  const int i = knob * count / 1024;
  return i < 0 ? 0 : (i >= count ? count - 1 : i);
}

// Puts everything back to the Original voice: exactly what audio_init() set.
static void voice_reset() {
  osc_saw.begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
  osc_pulse.begin(WAVEFORM_BANDLIMIT_PULSE);
  osc_pulse.amplitude(0.5f);
  mixer1.gain(0, 0.2f);
  mixer1.gain(1, 0.4f);
  mixer1.gain(2, 0.0f);
  osc3.amplitude(0.0f);
  osc4.amplitude(0.0f);
  fm_mod.amplitude(0.0f);
  fm_carrier.amplitude(0.0f);
  pluck.enable(false);
  fold_fx.enable(false);
  lush_fx.enable(false);
  filter1.octaveControl(4);
  envelope1.attack(2);
  envelope2.attack(15);
  envelope2.decay(0);
  envelope2.sustain(1.0f);
  envelope2.release(300);
}

// Applies the settings of the selected voice (synth_voice) that don't follow
// a knob. The knob-driven ones are in voice_update_sources().
void voice_apply() {
  AudioNoInterrupts();
  voice_reset();
  switch (synth_voice) {
    case VOICE_ORCHID:
      envelope1.attack(40);   // soft bloom instead of a click
      envelope2.attack(250);  // the filter opens slowly
      envelope2.release(600);
      lush_fx.set(LushFx::ORCHID_PARAMS);
      lush_fx.enable(true);
      break;
    case VOICE_ORGAN:
      osc_pulse.begin(WAVEFORM_SINE);
      osc_saw.begin(WAVEFORM_SINE);
      osc3.begin(WAVEFORM_SINE);
      mixer1.gain(0, 0.4f);
      mixer1.gain(2, 0.4f);
      envelope1.attack(4);
      lush_fx.set(ORGAN_LUSH);  // fast, shallow: an organ's vibrato-chorus
      lush_fx.enable(true);
      break;
    case VOICE_STRINGS:
      osc_pulse.begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
      osc3.begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
      osc4.begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
      mixer1.gain(0, 0.4f);
      mixer1.gain(2, 0.4f);
      envelope1.attack(150);
      envelope2.attack(400);
      envelope2.release(800);
      lush_fx.set(STRINGS_LUSH);  // slow + fast sweep: a string ensemble
      lush_fx.enable(true);
      break;
    case VOICE_ACID:
      osc_pulse.begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
      osc_saw.begin(WAVEFORM_BANDLIMIT_SQUARE);
      mixer1.gain(0, 0.4f);
      filter1.octaveControl(5);  // a deeper filter sweep
      envelope2.attack(2);
      envelope2.sustain(0.0f);  // the filter snaps open, then decays shut
      envelope2.release(100);
      break;
    case VOICE_PLUCK:
      osc_pulse.amplitude(0.0f);
      mixer1.gain(2, 0.8f);
      envelope1.attack(1);
      pluck.enable(true);
      lush_fx.set(PLUCK_LUSH);
      lush_fx.enable(true);
      break;
    case VOICE_FM:
      osc_pulse.amplitude(0.0f);
      mixer1.gain(2, 0.8f);
      lush_fx.set(FM_LUSH);
      lush_fx.enable(true);
      break;
    case VOICE_FOLD:
      osc_pulse.begin(WAVEFORM_TRIANGLE);
      osc_saw.begin(WAVEFORM_SINE);
      mixer1.gain(0, 0.5f);
      mixer1.gain(1, 0.5f);
      fold_fx.enable(true);
      // The filter acts like a low-pass gate: low at rest, opened by the
      // envelope (which follows the release knob, see synth_update).
      filter1.octaveControl(6);
      envelope2.attack(2);
      break;
    case VOICE_CHORD:
      osc_pulse.begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
      osc3.begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
      osc4.begin(WAVEFORM_BANDLIMIT_SAWTOOTH);
      mixer1.gain(0, 0.4f);
      mixer1.gain(2, 0.4f);
      envelope1.attack(20);
      envelope2.attack(120);
      envelope2.release(600);
      lush_fx.set(LushFx::ORCHID_PARAMS);
      lush_fx.enable(true);
      break;
    default:  // VOICE_ORIGINAL
      break;
  }
  AudioInterrupts();
}

// Note on/off hooks for voices with their own triggers. Called from note_on()
// and note_off() in main.cpp.
void voice_note_on(const uint8_t velocity) {
  if (synth_voice == VOICE_PLUCK) {
    AudioNoInterrupts();
    pluck.pluck(osc_pulse_target_frequency, velocity / 127.0f);
    AudioInterrupts();
  }
  fm_mod_env.noteOn();
}

void voice_note_off() { fm_mod_env.noteOff(); }

// Oscillator pitches and levels for the new voices (not Original/Orchid),
// from the played note and the Detune and Pulse Width knobs. Runs every loop
// with audio interrupts off. `note` is the glided note frequency; most voices
// play their fundamental an octave below it, like the stock pulse oscillator.
static void voice_update_sources(const float note) {
  const float pw = synth.pulseWidth / 1023.0f;
  const float dt = synth.detune / 1023.0f;
  const float f0 = note / 2;
  switch (synth_voice) {
    case VOICE_ORGAN:
      osc_pulse.frequency(f0);  // 8' fundamental
      osc_pulse.amplitude(0.6f);
      osc3.frequency(note);  // 4' octave
      osc3.amplitude(0.35f);
      osc_saw.frequency(f0 * ORGAN_DRAWBARS[knob_index(synth.detune, 4)]);
      osc_saw.amplitude(pw * 0.5f);
      break;
    case VOICE_STRINGS: {
      const float spread = 0.002f + 0.013f * dt;  // up to ~25 cents
      osc_pulse.frequency(f0);
      osc_pulse.amplitude(0.5f);
      osc_saw.frequency(f0 * (1.0f + spread));
      osc_saw.amplitude(0.5f);
      osc3.frequency(f0 * (1.0f - spread));
      osc3.amplitude(0.5f);
      osc4.frequency(note * (1.0f + spread * 0.5f));
      osc4.amplitude(pw * 0.5f);
      break;
    }
    case VOICE_ACID:
      osc_pulse.frequency(f0);
      osc_pulse.amplitude((1.0f - pw) * 0.9f);
      osc_saw.frequency(f0);
      osc_saw.amplitude(pw * 0.9f);
      envelope2.decay(80 + synth.detune * 900 / 1023);  // 80 ms to ~1 s
      break;
    case VOICE_PLUCK:
      osc_saw.amplitude(0.0f);
      pluck.frequency(note);
      pluck.set_tone(pw, dt);
      break;
    case VOICE_FM:
      osc_saw.amplitude(0.0f);
      fm_carrier.frequency(note);
      fm_carrier.amplitude(0.7f);
      fm_mod.frequency(note * FM_RATIOS[knob_index(synth.detune, 8)]);
      fm_mod.amplitude(pw * 0.6f);
      break;
    case VOICE_FOLD:
      osc_pulse.frequency(f0);
      osc_pulse.amplitude(0.8f);
      osc_saw.frequency(osc_saw_frequency);  // the stock detune interval
      osc_saw.amplitude(pw * 0.4f);
      fold_fx.amount(synth.filter / 1023.0f);
      break;
    case VOICE_CHORD: {
      const float* r = CHORD_RATIOS[knob_index(synth.detune, 8)];
      osc_pulse.frequency(note);
      osc_pulse.amplitude(0.5f);
      osc_saw.frequency(note * r[0]);
      osc_saw.amplitude(0.5f);
      osc3.frequency(note * r[1]);
      osc3.amplitude(0.5f);
      osc4.frequency(note * r[2]);
      osc4.amplitude(pw * 0.5f);
      break;
    }
    default:
      break;
  }
}

void synth_update() {
  const uint8_t voice = synth_voice;
  const bool stock = voice == VOICE_ORIGINAL || voice == VOICE_ORCHID;

  float osc_saw_amplitude = 0.4f;

  if(synth.detune > 850) {
    osc_saw_amplitude = map(synth.detune,850,1023,400,0) / 1000.0f;
  } else if(synth.detune < 200) {
    osc_saw_amplitude = map(synth.detune,0,200,200,400) / 1000.0f;
  }

  float bitcrusher_samplerate = HIGH_SAMPLE_RATE;
  if(synth.crush) {
    bitcrusher_samplerate = LOW_SAMPLE_RATE;
  }

  float osc_pulse_pulseWidth = map(synth.pulseWidth,0,1023,500,950)/1000.0f;

  // Resonance range: stock 0.7-3.2. Gentler (0.7-2.0) for the pad-like
  // voices so they don't whistle; wider (0.7-4.5) for Acid's squelch.
  long resonance_max = 320;
  if (voice == VOICE_ACID) {
    resonance_max = 450;
  } else if (voice == VOICE_ORCHID || voice == VOICE_ORGAN ||
             voice == VOICE_STRINGS || voice == VOICE_PLUCK ||
             voice == VOICE_FM || voice == VOICE_CHORD) {
    resonance_max = 200;
  }
  float filter_resonance = map(synth.resonance, 0, 1023, 70, resonance_max) / 100.0f;

  if(synth.accent) {
    filter_resonance = 4.0f;
  }

  // Release: stock 30-540 ms; longer (60 ms to ~1.1 s) for the voices that
  // ring or swell. Keep it under ~1.4 s: past that the envelope's step
  // rounds to zero and the note never fades out.
  const bool long_release = voice == VOICE_ORCHID || voice == VOICE_STRINGS ||
                            voice == VOICE_PLUCK || voice == VOICE_FM ||
                            voice == VOICE_CHORD;
  const int release_ms = long_release
                             ? ((synth.release * synth.release) >> 10) + 60
                             : ((synth.release * synth.release) >> 11) + 30;

  // Audio interrupts have to be off to apply settings
  AudioNoInterrupts();

  if (stock) {
    osc_saw.frequency(osc_saw_frequency);
    osc_saw.amplitude(osc_saw_amplitude);

    osc_pulse.frequency(osc_pulse_frequency/2);
    osc_pulse.pulseWidth(osc_pulse_pulseWidth);
  } else {
    voice_update_sources(osc_pulse_frequency);
  }

  if (voice == VOICE_FOLD) {
    filter1.frequency(250);  // the Filter knob folds instead
    envelope2.release(release_ms);  // the "gate" closes with the note
  } else {
    filter1.frequency((synth.filter/2)+30);
  }
  filter1.resonance(filter_resonance);

  envelope1.release(release_ms);

  bitcrusher1.sampleRate(bitcrusher_samplerate);

  audio_volume(synth.amplitude);

  AudioInterrupts();
}
#endif
