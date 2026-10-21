#include "sequencer_helpers.h"

/* #define SINGLE_TEST retriggers_held_notes_on_advance */

namespace NoteTracker {
static bool note_active = false;
static int played_notes = 0;
static int last_note = -1;

static void reset() {
  note_active = false;
  played_notes = 0;
}
static void note_on(uint8_t note, uint8_t velocity) {
  TEST_ASSERT_MESSAGE(note_active == false, "note_on called twice.");
  played_notes++;
  last_note = note;
  note_active = true;
}
static void note_off(void) {
  TEST_ASSERT_MESSAGE(note_active == true, "note_off called twice.");
  note_active = false;
}
static Sequencer::Output::Callbacks callbacks{note_on, note_off};

} // namespace NoteTracker

#define ASSERT_NOTE_PLAYING(active)                                            \
  if (active) {                                                                \
    TEST_ASSERT_TRUE_MESSAGE(NoteTracker::note_active,                         \
                             "Expected playing note.");                        \
  } else {                                                                     \
    TEST_ASSERT_FALSE_MESSAGE(NoteTracker::note_active,                        \
                              "Expected no playing notes.");                   \
  }

#define ASSERT_PLAYED_COUNT(count)                                             \
  TEST_ASSERT_EQUAL_MESSAGE(count, NoteTracker::played_notes,                  \
                            "[Played note count]");

void on_running_advance(Sequencer::Sequencer &s) {}

Sequencer::Sequencer make_cleared_sequencer() {
  Sequencer::Sequencer seq(NoteTracker::callbacks, on_running_advance);
  clear_steps(seq);
  return seq;
}

namespace Tests {

void holds_note_if_not_running() {
  auto seq = make_cleared_sequencer();
  seq.set_gate_length(10);

  ASSERT_EQ(seq.is_running(), false);

  seq.hold_note(1);
  seq.update_gate(1);
  ASSERT_EQ(1, NoteTracker::played_notes);
  ASSERT_TRUE(NoteTracker::note_active);
  const uint32_t long_delta = 1000000;
  seq.update_gate(long_delta);

  // Note should still be on after gate length has passed.
  ASSERT_TRUE(NoteTracker::note_active);
  ASSERT_EQ(1, NoteTracker::played_notes);

  seq.release_note(1);
  seq.update_gate(long_delta);
  ASSERT_EQ(false, NoteTracker::note_active);
}

void stops_playing_note_after_gate_duration() {
  auto seq = make_cleared_sequencer();
  const auto gate_len = 10;
  seq.set_gate_length(gate_len);
  seq.run();
  const auto note = 123;
  seq.set_step_note(1, note);
  seq.toggle_step(1);

  ASSERT_PLAYED_COUNT(0);
  ASSERT_NOTE_PLAYING(false)

  tick_to_next_step(seq);
  ASSERT_EQ(1, seq.cur_step_index());
  seq.update_gate(1);

  ASSERT_NOTE_PLAYING(true);
  seq.update_gate(gate_len - 1);
  ASSERT_NOTE_PLAYING(true);

  seq.update_gate(1);
  ASSERT_PLAYED_COUNT(1);
  ASSERT_NOTE_PLAYING(false);
}

void records_early_live_note() {
  auto seq = make_cleared_sequencer();
  const uint8_t gate_len = 5;
  seq.set_gate_length(gate_len);
  seq.run();

  seq.tick_clock();
  ASSERT_EQ(1, seq.cur_step_index());
  const auto note = 1;
  seq.hold_note(note);
  seq.update_gate(1);
  seq.release_note(note);

  ASSERT_ONLY_ENABLED_STEP(seq, 1);
  ASSERT_NOTE_PLAYING(true);

  tick_to_next_step(seq);
  seq.update_gate(gate_len);
  ASSERT_ONLY_ENABLED_STEP(seq, 1);
  ASSERT_NOTE_PLAYING(false);
  ASSERT_PLAYED_COUNT(1);
}

void records_late_live_note() {
  auto seq = make_cleared_sequencer();
  const uint8_t gate_len = 5;
  seq.set_gate_length(gate_len);
  seq.run();

  // Tick to end of current step
  for (unsigned i = 0; i < Sequencer::TICKS_PER_STEP - 1; ++i) {
    seq.tick_clock();
  }
  ASSERT_EQ(1, seq.cur_step_index());

  const auto note = 1;
  seq.hold_note(note);

  ASSERT_ONLY_ENABLED_STEP(seq, 2);
  seq.update_gate(1);

  seq.release_all_notes();
  ASSERT_ONLY_ENABLED_STEP(seq, 2);

  ASSERT_PLAYED_COUNT(0);
  tick_to_next_step(seq);
  ASSERT_EQ(2, seq.cur_step_index());
  ASSERT_NOTE_PLAYING(true);
  seq.update_gate(gate_len);
  ASSERT_NOTE_PLAYING(true);

  // // Ticked over gate duration, should kill active step note.
  seq.update_gate(1);
  ASSERT_NOTE_PLAYING(false);
  ASSERT_PLAYED_COUNT(1);
}

void does_not_record_middle_live_notes() {
  auto seq = make_cleared_sequencer();
  const uint8_t gate_len = 5;
  seq.set_gate_length(gate_len);
  seq.run();

  // Tick to next step to make sure clock is note 0.
  tick_to_next_step(seq);

  // Tick to middle of current step
  for (unsigned i = 0; i < Sequencer::TICKS_PER_STEP / 2; ++i) {
    seq.tick_clock();
  }

  const auto note = 1;
  seq.hold_note(note);

  ASSERT_NOTE_PLAYING(true);
  ASSERT_PLAYED_COUNT(1);

  seq.update_gate(1);
  ASSERT_EQ(0, count_enabled_steps(seq));
}

void always_records_note_when_stopped() {
  auto seq = make_cleared_sequencer();
  const uint8_t gate_len = 5;
  seq.set_gate_length(gate_len);

  for (unsigned i = 0; i < Sequencer::TICKS_PER_STEP; ++i) {
    const auto note = 1;
    seq.hold_note(note);
    seq.release_note(note);
    char msg[64];
    snprintf(msg, 64, "Step not recorded when clock == %u.", i);
    TEST_ASSERT_EQUAL_MESSAGE(1, count_enabled_steps(seq), msg);
    clear_steps(seq);
    seq.tick_clock();
  }

  ASSERT_EQ(0, count_enabled_steps(seq));
}

void records_step_and_advances_when_not_running() {
  auto seq = make_cleared_sequencer();
  ASSERT_FALSE(seq.is_running());
  seq.hold_note(1);
  seq.update_gate(1);
  seq.release_note(1);
  seq.update_gate(1);

  ASSERT_ONLY_ENABLED_STEP(seq, 0);
  ASSERT_EQ(1, seq.cur_step_index());
}

void retriggers_held_notes_on_advance() {
  auto seq = make_cleared_sequencer();
  const auto gate_len = 1;
  seq.set_gate_length(gate_len);
  seq.run();
  seq.tick_clock();

  seq.hold_note(1);
  seq.update_gate(1);

  ASSERT_NOTE_PLAYING(true);
  ASSERT_PLAYED_COUNT(1);

  tick_to_next_step(seq);
  seq.update_gate(1);

  ASSERT_NOTE_PLAYING(true);
  ASSERT_PLAYED_COUNT(2);

  seq.update_gate(gate_len);

  ASSERT_PLAYED_COUNT(2);
  ASSERT_NOTE_PLAYING(false);
}

void respects_step_offset_during_playback() {
  auto seq = make_cleared_sequencer();
  for (int i = 0; i < Sequencer::NUM_STEPS; ++i) {
    seq.set_step_note(i, i);
  }

  set_all_steps_active(seq, true);
  ASSERT_EQ(Sequencer::NUM_STEPS, count_enabled_steps(seq));

  seq.run();
  ASSERT_PLAYED_COUNT(0);

  seq.advance();
  seq.update_gate(1);

  ASSERT_PLAYED_COUNT(1);
  ASSERT_EQ(1, NoteTracker::last_note);
  seq.set_step_offset(2);

  seq.advance();
  seq.update_gate(1);
  ASSERT_PLAYED_COUNT(2);
  ASSERT_EQ(4, NoteTracker::last_note);
  ASSERT_EQ(4, seq.cur_step_index());
}

void arp_does_not_replay_note_when_lower_added() {
  auto seq = make_cleared_sequencer();
  seq.set_gate_length(10);
  seq.run();
  seq.tick_clock();
  seq.hold_note(1);
  seq.hold_note(3);
  ASSERT_PLAYED_COUNT(1);
  ASSERT_EQ(1, NoteTracker::last_note);
  seq.advance();
  seq.hold_note(2);
  ASSERT_PLAYED_COUNT(2);
  ASSERT_EQ(3, NoteTracker::last_note);
  seq.advance();
  ASSERT_PLAYED_COUNT(3);
  ASSERT_EQ(1, NoteTracker::last_note);
}

void plays_randomized_step() {
  auto seq = make_cleared_sequencer();
  const uint8_t offset = 2;
  set_all_steps_active(seq, true);
  for (int step = 0; step < Sequencer::NUM_STEPS; ++step) {
    seq.set_step_note(step, step);
  }

  seq.run();
  seq.advance();
  ASSERT_PLAYED_COUNT(1);
  ASSERT_EQ(1, NoteTracker::last_note);
  seq.set_step_offset(offset);
  ASSERT_PLAYED_COUNT(1);
  seq.advance();
  ASSERT_PLAYED_COUNT(2);
  ASSERT_EQ(2 + offset, NoteTracker::last_note);
}

// --- Variable sequence length ---

void defaults_to_eight_steps() {
  auto seq = make_cleared_sequencer();
  ASSERT_EQ(Sequencer::NUM_STEPS, seq.get_length());

  for (int i = 0; i < Sequencer::NUM_STEPS; ++i) {
    seq.advance();
  }
  ASSERT_EQ(0, seq.cur_step_index());
}

void set_length_clamps_to_valid_range() {
  auto seq = make_cleared_sequencer();
  seq.set_length(0);
  ASSERT_EQ(1, seq.get_length());
  seq.set_length(100);
  ASSERT_EQ(Sequencer::MAX_STEPS, seq.get_length());
}

void wraps_at_new_length() {
  auto seq = make_cleared_sequencer();
  seq.set_length(16);

  for (int i = 0; i < 15; ++i) {
    seq.advance();
  }
  ASSERT_EQ(15, seq.cur_step_index());
  ASSERT_EQ(1, seq.cur_page());
  ASSERT_EQ(7, seq.cur_page_position());

  seq.advance();
  ASSERT_EQ(0, seq.cur_step_index());
  ASSERT_EQ(0, seq.cur_page());
}

void lengthening_copies_pattern_into_new_steps() {
  auto seq = make_cleared_sequencer();
  seq.toggle_step(0);
  seq.set_step_note(0, 40);
  seq.toggle_step(3);
  seq.set_step_note(3, 43);

  seq.set_length(24);
  ASSERT_EQ(6, count_enabled_steps(seq));
  for (int page = 0; page < 3; ++page) {
    ASSERT_TRUE(get_step_enabled(seq, page * 8 + 0));
    ASSERT_EQ(40, get_note(seq, page * 8 + 0));
    ASSERT_TRUE(get_step_enabled(seq, page * 8 + 3));
    ASSERT_EQ(43, get_note(seq, page * 8 + 3));
  }
}

void shortening_wraps_playhead_into_new_length() {
  auto seq = make_cleared_sequencer();
  seq.set_length(16);
  for (int i = 0; i < 10; ++i) {
    seq.advance();
  }
  ASSERT_EQ(10, seq.cur_step_index());

  seq.set_length(8);
  ASSERT_EQ(2, seq.cur_step_index());
  ASSERT_EQ(0, seq.cur_page());
}

void step_buttons_follow_playhead_page() {
  auto seq = make_cleared_sequencer();
  seq.set_length(16);
  ASSERT_EQ(0, seq.step_on_cur_page(0));
  ASSERT_EQ(7, seq.step_on_cur_page(7));

  for (int i = 0; i < 9; ++i) {
    seq.advance();
  }
  ASSERT_EQ(1, seq.cur_page());
  ASSERT_EQ(8, seq.step_on_cur_page(0));
  ASSERT_EQ(15, seq.step_on_cur_page(7));

  // Toggling via the page touches step 9 only, not step 1 on page 0.
  seq.toggle_step(seq.step_on_cur_page(1));
  ASSERT_ONLY_ENABLED_STEP(seq, 9);
}

void plays_steps_on_later_pages_while_running() {
  auto seq = make_cleared_sequencer();
  seq.set_length(32);
  for (int i = 0; i < 32; ++i) {
    seq.set_step_note(i, i);
  }
  set_all_steps_active(seq, true);

  seq.run();
  for (int i = 0; i < 25; ++i) {
    seq.advance();
  }
  ASSERT_EQ(25, NoteTracker::last_note);
  ASSERT_EQ(3, seq.cur_page());

  for (int i = 0; i < 7; ++i) {
    seq.advance();
  }
  ASSERT_EQ(0, NoteTracker::last_note);
  ASSERT_EQ(0, seq.cur_step_index());
}

void step_offset_wraps_at_length() {
  auto seq = make_cleared_sequencer();
  seq.set_length(16);
  seq.set_step_offset(20);
  ASSERT_EQ(4, seq.get_step_offset());
  ASSERT_EQ(4, seq.cur_step_index());
}

} // namespace Tests

void setUp(void) { NoteTracker::reset(); }
void tearDown(void) {}

int main() {
  UNITY_BEGIN();
#ifdef SINGLE_TEST
  RUN_TEST(Tests::SINGLE_TEST);
#else
  RUN_TEST(Tests::holds_note_if_not_running);
  RUN_TEST(Tests::stops_playing_note_after_gate_duration);
  RUN_TEST(Tests::records_early_live_note);
  RUN_TEST(Tests::records_late_live_note);
  RUN_TEST(Tests::does_not_record_middle_live_notes);
  RUN_TEST(Tests::records_step_and_advances_when_not_running);
  RUN_TEST(Tests::retriggers_held_notes_on_advance);
  RUN_TEST(Tests::respects_step_offset_during_playback);
  RUN_TEST(Tests::arp_does_not_replay_note_when_lower_added);
  RUN_TEST(Tests::always_records_note_when_stopped);
  RUN_TEST(Tests::plays_randomized_step);
  RUN_TEST(Tests::defaults_to_eight_steps);
  RUN_TEST(Tests::set_length_clamps_to_valid_range);
  RUN_TEST(Tests::wraps_at_new_length);
  RUN_TEST(Tests::lengthening_copies_pattern_into_new_steps);
  RUN_TEST(Tests::shortening_wraps_playhead_into_new_length);
  RUN_TEST(Tests::step_buttons_follow_playhead_page);
  RUN_TEST(Tests::plays_steps_on_later_pages_while_running);
  RUN_TEST(Tests::step_offset_wraps_at_length);
#endif
  return UNITY_END();
}
