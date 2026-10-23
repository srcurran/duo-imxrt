#ifndef SYNTH_DRUM_KIT_H_9VB3QE
#define SYNTH_DRUM_KIT_H_9VB3QE

// Audio library wrapper around DrumKit::Kit (shared/duo/drum_kit.h).
// Output 0: kick pad (kick, tom, snare). Output 1: hat pad (closed hat,
// clap, open hat).

#include "AudioStream.h"
#include "shared/duo/drum_kit.h"

class AudioSynthDrumKit : public AudioStream {
public:
  AudioSynthDrumKit() : AudioStream(0, NULL), kit(AUDIO_SAMPLE_RATE_EXACT) {
  }

  // Call from the main loop; safe against the audio interrupt.
  void trigger(DrumKit::Drum drum, float level = 1.0f) {
    __disable_irq();
    kit.trigger(drum, level);
    __enable_irq();
  }

  virtual void update(void) {
    audio_block_t *out0 = allocate();
    if (!out0) {
      return;
    }
    audio_block_t *out1 = allocate();
    if (!out1) {
      release(out0);
      return;
    }

    float a[AUDIO_BLOCK_SAMPLES];
    float b[AUDIO_BLOCK_SAMPLES];
    kit.render(a, b, AUDIO_BLOCK_SAMPLES);

    for (int i = 0; i < AUDIO_BLOCK_SAMPLES; i++) {
      out0->data[i] = to_sample(a[i]);
      out1->data[i] = to_sample(b[i]);
    }

    transmit(out0, 0);
    transmit(out1, 1);
    release(out0);
    release(out1);
  }

private:
  static int16_t to_sample(float x) {
    if (x > 1.0f) {
      x = 1.0f;
    } else if (x < -1.0f) {
      x = -1.0f;
    }
    return (int16_t)(x * 32767.0f);
  }

  DrumKit::Kit kit;
};

#endif /* end of include guard: SYNTH_DRUM_KIT_H_9VB3QE */
