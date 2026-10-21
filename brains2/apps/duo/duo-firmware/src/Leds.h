#ifndef Leds_h
#define Leds_h

#include <FastLED.h>
#include "led_pins.h"

#define COLOR_ORDER GRB

#define LED_TYPE SK6812
#define CORRECTION_SK6812 0xFFF1E0
#define CORRECTION_SK6805 0xFFD3E0
#define LED_WHITE CRGB(230,255,150)

#define leds(A) physical_leds[led_order[A]]

CRGB physical_leds[NUM_LEDS];
#define led_play physical_leds[0]

#define SK6812_BRIGHTNESS 32
#define SK6805_BRIGHTNESS 140

/* The black keys have assigned colors. The white keys are shown in gray */
const CRGB COLORS[] = {
  0x444444,
  0xFF0001,
  0x444444,
  0xFFDD00,
  0x444444,
  0x444444,
  0x11FF00,
  0x444444,
  0x0033DD,
  0x444444,
  0xFF00FF,
  0x444444,
  0x444444,
  0xFF2209,
  0x444444,
  0x99FF00,
  0x444444,
  0x444444,
  0x00EE22,
  0x444444,
  0x0099CC,
  0x444444,
  0xBB33BB,
  0x444444
};

void led_init();
void led_update();
void led_data_received();
void led_deinit();

void led_data_received() {
    FastLED.setBrightness(SK6805_BRIGHTNESS); 
    FastLED.setCorrection(CORRECTION_SK6805);
    detachInterrupt(LED_CLK);
}

void led_init() {
  FastLED.addLeds<LED_TYPE, LED_DATA, COLOR_ORDER>(physical_leds, NUM_LEDS);
  
  FastLED.setBrightness(SK6805_BRIGHTNESS); 
  FastLED.setCorrection(CORRECTION_SK6812);

  // We're going to do a loopback test first to determine brightness
  //attachInterrupt(LED_CLK, led_data_received, CHANGE);
  FastLED.clear();
  physical_leds[NUM_LEDS-1] = CRGB(0xff6805);
  FastLED.show();
  
  FastLED.clear();
  FastLED.show();
  /* The 400ms delay introduced by this startup animation prevents
     an audible pop/click at startup
     */
     
  #ifdef DEV_MODE
     physical_leds[0] = CRGB::Blue;
  #endif

  // The key of the current MIDI channel lights up
  physical_leds[MIDI_CHANNEL+8] = COLORS[SCALE[MIDI_CHANNEL-1]%24];

  FastLED.show();
  delay(100);
  FastLED.show();
  delay(100);
  FastLED.show();
  delay(100);

  for(uint16_t i = 0; i < 10; i++) {
    write_env_led(i*8);
    write_filter_led(i*8);
    write_osc_led(i*8);

    physical_leds[i+9] = COLORS[SCALE[i]%24];
    delay(20);
    FastLED.show();
  }
}

// Fades out leds and sets them to a low power state if possible
void led_deinit() {
  for(int i = 32; i > 1; i = (i*7)>>3) {
    FastLED.setBrightness(i);
    FastLED.show();
    delay(20);
  }

  blank_env_led();
  blank_filter_led();
  blank_osc_led();
  FastLED.clear();
  FastLED.show();
}

// Updates the LED colour and brightness to match the stored sequence
void led_update() {
  for (uint16_t i = 0; i < 10; i++) {
    physical_leds[i + 9] = COLORS[SCALE[i] % 24];
  }

  // With more than 8 steps, keyboard key 1-4 turns white to show which page
  // (steps 1-8, 9-16, ...) the playhead is on.
  if (sequencer.get_length() > Sequencer::STEPS_PER_PAGE) {
    physical_leds[sequencer.cur_page() + 9] = LED_WHITE;
  }

  // The step LEDs show the page (8 steps) the playhead is on.
  for (int l = 0; l < Sequencer::STEPS_PER_PAGE; l++) {
    const uint8_t step = sequencer.step_on_cur_page(l);
    if (sequencer.get_step_enabled(step)) {
      leds(l) = COLORS[sequencer.get_step_note(step) % 24];
    } else {
      leds(l) = CRGB::Black;
    }
  }

  const auto cur_seq_step = sequencer.cur_step_index();
  // The LED under the playhead.
  const auto cur_led = sequencer.cur_page_position();

  if (sequencer.gate_active()) {
    leds(cur_led) = LED_WHITE;
  }

  if (random_held) {
    // While Random is held, LEDs 1-4 show the length as a meter:
    // one lit LED per 8 steps.
    const uint8_t pages = sequencer.get_length() / Sequencer::STEPS_PER_PAGE;
    for (int l = 0; l < Sequencer::STEPS_PER_PAGE; l++) {
      leds(l) = l < pages ? LED_WHITE : CRGB(CRGB::Black);
    }
  }

  if (sequencer.is_running()) {
    led_play = LED_WHITE;
  } else if (random_held) {
    // Keep the play LED off while the length meter is showing.
    led_play = CRGB::Black;
  } else {
    if (sequencer.note_playing()) {
      leds(cur_led) = LED_WHITE;
    } else {
      const unsigned step_ticks = Sequencer::TICKS_PER_STEP;
      const uint32_t seq_clock = sequencer.get_clock() + step_ticks;
      const uint32_t fade_val = (seq_clock % step_ticks) * 16;
      const bool fade_play = (seq_clock % (2 * step_ticks)) < step_ticks;

      // Toggle between fading play button or current step.
      if (fade_play) {
        led_play = LED_WHITE;
        led_play.fadeLightBy(fade_val);
      } else {
        led_play = CRGB::Black;

        if (sequencer.get_step_enabled(cur_seq_step)) {
          leds(cur_led) = blend(LED_WHITE, COLORS[sequencer.get_step_note(cur_seq_step) % 24], fade_val);
        } else {
          leds(cur_led) = LED_WHITE;
          leds(cur_led) = leds(cur_led).fadeLightBy(fade_val);
        }
      }
    }
  }

  write_env_led(peak1.read());
  write_filter_led(synth.filter);
  write_osc_led(synth.pulseWidth);
}

#endif
