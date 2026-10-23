// Renders the drum kit to .wav files so the sounds can be judged on a computer
// before flashing. Usage: render_drums <output folder>

#include "drum_kit.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace DrumKit;

static const float SR = 44100.0f;
static const int BLOCK = 128;

static void put32(FILE *f, uint32_t v) {
  fwrite(&v, 4, 1, f);
}
static void put16(FILE *f, uint16_t v) {
  fwrite(&v, 2, 1, f);
}

static void write_wav(const std::string &path, const std::vector<float> &s) {
  FILE *f = fopen(path.c_str(), "wb");
  if (!f) {
    fprintf(stderr, "can't write %s\n", path.c_str());
    return;
  }
  const uint32_t bytes = (uint32_t)s.size() * 2;
  fwrite("RIFF", 1, 4, f);
  put32(f, 36 + bytes);
  fwrite("WAVEfmt ", 1, 8, f);
  put32(f, 16);
  put16(f, 1); // PCM
  put16(f, 1); // mono
  put32(f, (uint32_t)SR);
  put32(f, (uint32_t)SR * 2);
  put16(f, 2);
  put16(f, 16);
  fwrite("data", 1, 4, f);
  put32(f, bytes);
  for (float x : s) {
    if (x > 1.0f) x = 1.0f;
    if (x < -1.0f) x = -1.0f;
    put16(f, (uint16_t)(int16_t)lrintf(x * 32767.0f));
  }
  fclose(f);
  printf("wrote %s\n", path.c_str());
}

struct Hit {
  float at_ms;
  Drum drum;
};

// Renders a list of hits (both buses summed, as on the DUO's speaker).
static std::vector<float> render(Kit &kit, const std::vector<Hit> &hits, float total_ms) {
  std::vector<float> out;
  const int total = (int)(total_ms * SR / 1000.0f);
  size_t next = 0;
  float a[BLOCK], b[BLOCK];
  for (int pos = 0; pos < total;) {
    // Trigger hits that fall in this sample (render one sample at a time
    // around hits for exact timing; blocks otherwise).
    int n = BLOCK;
    if (next < hits.size()) {
      const int hit_pos = (int)(hits[next].at_ms * SR / 1000.0f);
      if (hit_pos <= pos) {
        kit.trigger(hits[next].drum);
        next++;
        continue;
      }
      if (hit_pos - pos < n) {
        n = hit_pos - pos;
      }
    }
    kit.render(a, b, n);
    for (int i = 0; i < n; i++) {
      out.push_back(a[i] + b[i]);
    }
    pos += n;
  }
  return out;
}

// Approximation of the stock DUO kick (AudioSynthSimpleDrum, middle zone:
// 55 Hz, 137 ms, full pitch mod), for comparison.
static std::vector<float> stock_kick() {
  std::vector<float> out;
  const float len = 0.137f * SR;
  float phase = 0;
  for (int i = 0; i < (int)(0.4f * SR); i++) {
    const float lin = 1.0f - i / len;
    if (lin <= 0) {
      out.push_back(0);
      continue;
    }
    const float env = lin * lin;
    phase += 55.0f * (1.0f + 3.0f * env) / SR;
    out.push_back(sinf(2.0f * 3.14159265f * phase) * env);
  }
  return out;
}

int main(int argc, char **argv) {
  const std::string dir = argc > 1 ? argv[1] : ".";

  write_wav(dir + "/kick_0_today.wav", stock_kick());

  struct Variant {
    const char *name;
    KickParams p;
  };
  const Variant variants[] = {
      {"kick_A_subtle", {52.0f, 3.0f, 6.0f, 0.8f, 40.0f, 90.0f, 0.2f, 1.3f}},
      {"kick_B_punch", KICK_DEFAULT},
      {"kick_C_hard", {47.0f, 6.0f, 4.0f, 1.4f, 45.0f, 110.0f, 0.5f, 2.6f}},
  };
  for (const Variant &v : variants) {
    Kit kit(SR);
    kit.kick = v.p;
    write_wav(dir + "/" + v.name + ".wav", render(kit, {{0, Kick}}, 600));
  }

  const struct {
    const char *name;
    Drum drum;
    float ms;
  } singles[] = {
      {"tom", Tom, 800},       {"snare", Snare, 500},
      {"hat_closed", ClosedHat, 300}, {"clap", Clap, 600},
      {"hat_open", OpenHat, 1200},
  };
  for (const auto &s : singles) {
    Kit kit(SR);
    write_wav(dir + "/" + s.name + ".wav", render(kit, {{0, s.drum}}, s.ms));
  }

  // Finger sliding left to right across each pad, then back.
  {
    Kit kit(SR);
    write_wav(dir + "/slide_kick_pad.wav",
              render(kit, {{0, Kick}, {70, Tom}, {140, Snare}, {600, Snare},
                           {670, Tom}, {740, Kick}},
                     1400));
    Kit kit2(SR);
    write_wav(dir + "/slide_hat_pad.wav",
              render(kit2, {{0, ClosedHat}, {70, Clap}, {140, OpenHat},
                            {600, OpenHat}, {670, Clap}, {740, ClosedHat}},
                     1400));
  }

  // A two-bar groove at 120 BPM (16th = 125 ms) with each kick variant.
  for (const Variant &v : variants) {
    Kit kit(SR);
    kit.kick = v.p;
    std::vector<Hit> hits;
    for (int bar = 0; bar < 2; bar++) {
      const float b = bar * 2000.0f;
      for (int s = 0; s < 16; s++) {
        const float t = b + s * 125.0f;
        if (s == 0 || s == 6 || s == 8 || (bar == 1 && s == 14)) hits.push_back({t, Kick});
        if (s == 4 || s == 12) hits.push_back({t, Snare});
        if (s % 2 == 0 && s != 14) hits.push_back({t, ClosedHat});
        if (s == 14) hits.push_back({t, OpenHat});
        if (bar == 1 && s == 12) hits.push_back({t, Clap});
        if (bar == 1 && s == 15) hits.push_back({t, Tom});
      }
    }
    // Hits must be in time order.
    for (size_t i = 1; i < hits.size(); i++) {
      for (size_t j = i; j > 0 && hits[j].at_ms < hits[j - 1].at_ms; j--) {
        std::swap(hits[j], hits[j - 1]);
      }
    }
    write_wav(dir + "/groove_" + std::string(v.name + 5) + ".wav", render(kit, hits, 4300));
  }
  return 0;
}
