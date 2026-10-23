#ifndef SEQ_H_0PHDG2MB
#define SEQ_H_0PHDG2MB

#include "note_stack.h"
#include <cstdint>

namespace Sequencer {
static const unsigned INITIAL_VELOCITY = 100;
// Number of physical step buttons/LEDs. Also the default sequence length.
static const uint8_t NUM_STEPS = 8;
static const uint8_t STEPS_PER_PAGE = NUM_STEPS;
// Longest supported sequence (4 pages of 8 steps).
static const uint8_t MAX_STEPS = 32;
static const unsigned PULSES_PER_QUARTER_NOTE = 24;
static const unsigned TICKS_PER_STEP = (PULSES_PER_QUARTER_NOTE / 2);

struct Arpeggiator {
  void init() {
    held_notes.Init();
    index = 0;
  }

  void advance() {
    index++;
  }

  uint8_t recent_note() const {
    return held_notes.most_recent_note().note;
  }
  uint8_t current_note() {
    index = index % held_notes.size();

    return held_notes.sorted_note(index).note;
  }
  uint8_t count() const {
    return held_notes.size();
  }
  inline void hold_note(uint8_t note, uint8_t velocity) {
    if (count() > 0 && note < current_note()) {
      index++;
    }

    held_notes.NoteOn(note, velocity);
  }
  inline void release_note(uint8_t note) {
    held_notes.NoteOff(note);
  }
  inline void release_all_notes() {
    held_notes.Clear();
  };

private:
  uint8_t index = 0;
  NoteStack<10> held_notes;
};

struct Gate {
  void update(const uint32_t delta_millis) {
    elapsed += delta_millis;
  }
  void trigger() {
    elapsed = 0;
  }
  bool open() const {
    return elapsed <= length;
  }

  uint32_t length = 1;

private:
  uint32_t elapsed = 0;
};

struct Output {
  struct Callbacks {
    typedef void (&NoteOn)(uint8_t note, uint8_t velocity);
    typedef void (&NoteOff)(void);
    NoteOn note_on;
    NoteOff note_off;
  };

  Output(Callbacks callbacks) : callbacks(callbacks) {
  }

  void on(const uint8_t note) {
    if (output_active) {
      off();
    }

    output_active = true;
    active_note = note;
    callbacks.note_on(active_note, INITIAL_VELOCITY);
  }

  void off() {
    if (output_active) {
      callbacks.note_off();
      output_active = false;
    }
  }

  bool active() const {
    return output_active;
  }

private:
  const Callbacks callbacks;
  bool output_active = false;
  uint8_t active_note = 0;
};

enum SpeedModifier {
  HalfSpeed = 1,
  NormalSpeed = 2,
  DoubleSpeed = 3,
  QuadSpeed = 4
};

struct Sequencer;
typedef void (&OnRunnningAdvance)(Sequencer &);

struct Sequencer {
  Sequencer(Output::Callbacks callbacks, OnRunnningAdvance on_running_advance);

  // `run()` and `stop()` controls the running state of the Sequencer, however
  // is no internal ticker. In order to advance, `tick_clock()` must be called.
  void run();
  // Stopping also ends perform mode.
  void stop() {
    running = false;
    performing = false;
  }
  // Perform mode: live notes (keys and arp) still sound, but nothing is
  // recorded into the steps. Step toggles still work. Cleared by stop().
  inline void set_performing(const bool on) {
    performing = on;
  }
  inline bool is_performing() const {
    return performing;
  }

  void advance();
  void tick_clock();
  // Clock ticks per step at the current speed (the clock runs even when
  // stopped, so this also gives a step grid while stopped).
  uint8_t ticks_per_step() const;
  void update_gate(uint32_t delta_micros);
  void align_clock();
  inline void hold_note(uint8_t note) {
    hold_note(note, INITIAL_VELOCITY);
  }
  void hold_note(uint8_t note, uint8_t velocity);
  void release_note(uint8_t note);
  inline void release_all_notes() {
    arp.release_all_notes();
    if (!running) {
      output.off();
    }
  };
  inline bool is_running() const {
    return running;
  }
  inline uint8_t cur_step_index() const {
    return wrapped_step(current_step + step_offset);
  }
  // Wraps any step number into the current sequence length.
  inline uint8_t wrapped_step(const unsigned step) const {
    return step % length;
  }
  inline uint8_t get_length() const {
    return length;
  }
  // Sets the sequence length (clamped to 1..MAX_STEPS). When the sequence
  // gets longer, the new steps are filled by repeating the existing pattern.
  void set_length(uint8_t new_length);
  // The page (group of 8 steps) the playhead is currently on.
  inline uint8_t cur_page() const {
    return cur_step_index() / STEPS_PER_PAGE;
  }
  // Number of pages (groups of 8 steps) in the current length.
  inline uint8_t page_count() const {
    return (length + STEPS_PER_PAGE - 1) / STEPS_PER_PAGE;
  }
  // Moves the playhead to `step` (wrapped into the length) without playing
  // anything. Used to place the cursor when stopped.
  void set_cursor(uint8_t step);
  // Moves the playhead to the first step of `page` (wrapped into the page
  // count) without playing anything. Used to pick a page to edit when stopped.
  inline void set_page(const uint8_t page) {
    set_cursor((page % page_count()) * STEPS_PER_PAGE);
  }
  // The playhead's position within its page (0..STEPS_PER_PAGE-1), which is
  // also the step LED/button it lights up.
  inline uint8_t cur_page_position() const {
    return cur_step_index() % STEPS_PER_PAGE;
  }
  // The step that step button `button` (0..STEPS_PER_PAGE-1) controls on the
  // page the playhead is on.
  inline uint8_t step_on_cur_page(const uint8_t button) const {
    return wrapped_step(cur_page() * STEPS_PER_PAGE + button);
  }
  inline uint32_t get_clock() const {
    return clock;
  }
  inline bool gate_active() const {
    return step_gate.open();
  }
  inline bool note_playing() const {
    return output.active();
  }
  inline uint8_t get_step_enabled(const uint8_t step) const {
    return steps[wrapped_step(step)].enabled;
  }
  inline uint8_t get_step_note(const uint8_t step) const {
    return steps[wrapped_step(step)].note;
  }
  inline bool toggle_step(uint8_t step) {
    step = wrapped_step(step);
    const auto enabled = !steps[step].enabled;
    steps[step].enabled = enabled;
    return enabled;
  }
  // Step entry while stopped: toggles the cursor step. If that turns it off,
  // the cursor moves on to the next step, so the step is entered as a rest.
  // Nothing plays. Returns the step's new enabled state.
  bool toggle_cursor_step();
  inline void set_step_note(const uint8_t step, const uint8_t note) {
    steps[wrapped_step(step)].note = note;
  }

  void set_gate_length(const uint32_t micros) {
    step_gate.length = live_gate.length = micros;
  }

  void set_step_offset(uint8_t offset);
  uint8_t get_step_offset() const {
    return step_offset;
  }

  SpeedModifier speed_mod = NormalSpeed;

private:
  void record_note(uint8_t note, uint8_t step);
  void advance_running();
  void play_current_step();
  uint8_t quantized_current_step();
  inline void inc_current_step() {
    current_step = wrapped_step(current_step + 1);
  }

  bool running = false;
  bool performing = false;
  bool step_played_live = false;
  uint8_t length = NUM_STEPS;
  uint8_t current_step = 0;
  uint8_t step_offset = 0;
  uint8_t last_played_step = 0;
  uint32_t clock = 0;
  Arpeggiator arp;
  Gate step_gate;
  Gate live_gate;
  Output output;
  OnRunnningAdvance on_running_advance;

  struct Step {
    uint8_t enabled = false;
    uint8_t note = 0;
  };
  Step steps[MAX_STEPS];
};
} // namespace Sequencer

#endif /* end of include guard: SEQ_H_0PHDG2MB */
