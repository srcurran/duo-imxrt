#ifndef EFFECT_LUSH_H
#define EFFECT_LUSH_H

#include "AudioStream.h"
#include "lush_fx.h"

// Audio-graph wrapper for LushFx (chorus + reverb). While disabled, blocks
// pass through untouched, so the original voice sounds exactly as before.
class AudioEffectLush : public AudioStream {
 public:
  AudioEffectLush() : AudioStream(1, inputQueueArray), enabled(false) {}

  // Call with audio interrupts off (AudioNoInterrupts).
  void enable(const bool on) {
    if (on && !enabled) {
      fx.reset();
    }
    enabled = on;
  }

  // Mix and modulation settings (a voice's flavour). Call with audio
  // interrupts off.
  void set(const LushFx::Params& params) { fx.set(params); }

  virtual void update(void) {
    if (!enabled) {
      audio_block_t* block = receiveReadOnly();
      if (block) {
        transmit(block);
        release(block);
      }
      return;
    }

    audio_block_t* block = receiveWritable();
    if (!block) {
      // No input (silence): keep the reverb tail ringing.
      block = allocate();
      if (!block) return;
      memset(block->data, 0, sizeof(block->data));
    }
    fx.process(block->data, AUDIO_BLOCK_SAMPLES);
    transmit(block);
    release(block);
  }

 private:
  audio_block_t* inputQueueArray[1];
  LushFx::Lush fx;
  bool enabled;
};

#endif /* EFFECT_LUSH_H */
