#ifndef EFFECT_FOLD_H
#define EFFECT_FOLD_H

#include "AudioStream.h"
#include "fold_fx.h"

// Audio-graph wrapper for FoldFx (wavefolder). While disabled, blocks pass
// through untouched, so the other voices sound exactly as before.
class AudioEffectFold : public AudioStream {
 public:
  AudioEffectFold() : AudioStream(1, inputQueueArray), enabled(false) {}

  void enable(const bool on) { enabled = on; }
  void amount(const float a) { folder.set_amount(a); }

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
    if (!block) return;
    // The Fold voice's oscillators peak around 0.6 of full scale (~20000).
    folder.process(block->data, AUDIO_BLOCK_SAMPLES, 20000.0f, 16000.0f);
    transmit(block);
    release(block);
  }

 private:
  audio_block_t* inputQueueArray[1];
  FoldFx::Folder folder;
  bool enabled;
};

#endif /* EFFECT_FOLD_H */
