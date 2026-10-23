#ifndef SYNTH_PLUCK_H
#define SYNTH_PLUCK_H

#include "AudioStream.h"
#include "pluck_fx.h"

// The string's 4 KB loop buffer lives in the OCRAM bank (DMAMEM), which has
// more room than the fast DTCM bank. Not zeroed at startup: enable() clears it.
DMAMEM static int16_t pluck_loop_buffer[PluckFx::Pluck::SIZE];

// Audio-graph source for PluckFx (plucked string). Silent, and costs nothing,
// unless enabled (only the Pluck voice enables it).
class AudioSynthPluck : public AudioStream {
 public:
  AudioSynthPluck() : AudioStream(0, NULL), fx(pluck_loop_buffer), enabled(false) {}

  // The methods below: call with audio interrupts off (AudioNoInterrupts).
  void enable(const bool on) {
    if (on && !enabled) {
      fx.reset();
    }
    enabled = on;
  }
  void set_tone(const float brightness, const float sustain) {
    fx.set_tone(brightness, sustain);
  }
  void frequency(const float hz) { fx.frequency(hz); }
  void pluck(const float hz, const float level) { fx.pluck(hz, level); }

  virtual void update(void) {
    if (!enabled) return;
    audio_block_t* block = allocate();
    if (!block) return;
    fx.process(block->data, AUDIO_BLOCK_SAMPLES);
    transmit(block);
    release(block);
  }

 private:
  PluckFx::Pluck fx;
  bool enabled;
};

#endif /* SYNTH_PLUCK_H */
