#ifndef DRUM_KIT_H_4M8RZA
#define DRUM_KIT_H_4M8RZA

// Six synthesized drums, rendered in plain C++ (no Arduino or audio library),
// so the same code runs on the DUO and in the desktop tests/renderer.
//
// Two output buses, one per pad:
//   bus 0 (kick pad): Kick, Tom, Snare
//   bus 1 (hat pad):  ClosedHat, Clap, OpenHat
// A closed hat chokes a ringing open hat, as on a real hi-hat.

#include <cstdint>

namespace DrumKit {

enum Drum : uint8_t { Kick, Tom, Snare, ClosedHat, Clap, OpenHat, NUM_DRUMS };

// Zero-delay-feedback state variable filter (stable at any cutoff).
struct Svf {
  void set(float sample_rate, float cutoff_hz, float q);
  void reset() {
    ic1 = ic2 = 0.0f;
  }
  // Writes all three outputs.
  void process(float in, float &low, float &band, float &high);

  float a1 = 0, a2 = 0, a3 = 0, k = 1;
  float ic1 = 0, ic2 = 0;
};

struct KickParams {
  float base_hz;     // where the pitch settles
  float fast_drop;   // extra pitch at the start of the fast drop (x base)
  float fast_ms;     // fast drop time constant
  float slow_drop;   // extra pitch at the start of the slow drop (x base)
  float slow_ms;     // slow drop time constant
  float decay_ms;    // amplitude time constant
  float click;       // click transient level
  float drive;       // saturation amount (1 = clean)
};

extern const KickParams KICK_DEFAULT;

struct Kit {
  explicit Kit(float sample_rate = 44100.0f);

  void trigger(Drum drum, float level = 1.0f);
  // Adds n samples to each bus (bus0 and bus1 are overwritten, not mixed).
  void render(float *bus0, float *bus1, int n);
  bool active(Drum drum) const;

  KickParams kick = KICK_DEFAULT;

private:
  struct Voice {
    bool on = false;
    float level = 0;
    float amp = 0, amp_coef = 0;      // main envelope
    float amp2 = 0, amp2_coef = 0;    // second envelope (noise / click)
    float pitch1 = 0, pitch1_coef = 0;
    float pitch2 = 0, pitch2_coef = 0;
    float phase1 = 0, phase2 = 0;
    uint32_t t = 0;                   // samples since trigger
  };

  float coef(float ms) const;
  float noise();
  float render_kick(Voice &v);
  float render_tom(Voice &v);
  float render_snare(Voice &v);
  float render_hat(Voice &v);
  float render_clap(Voice &v);
  float metal();

  float sr;
  float inv_sr;
  uint32_t rng = 0x12345678u;
  Voice voices[NUM_DRUMS];
  float metal_phase[6] = {0, 0, 0, 0, 0, 0};
  float metal_inc[6];
  Svf snare_hp;
  Svf hat_hp, hat_bp;
  Svf open_hp, open_bp;
  Svf clap_bp;
  uint32_t clap_burst_gap;
  uint32_t clap_tail_start;
};

} // namespace DrumKit

#endif /* end of include guard: DRUM_KIT_H_4M8RZA */
