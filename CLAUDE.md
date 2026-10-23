# Dato DUO firmware mods

Custom firmware experiments for the Dato DUO synth, built on Dato's open-source
firmware (this repo: github.com/datomusic/duo-imxrt, MIT license).
Full plan doc: https://claude.ai/code/artifact/c8336b8f-97ed-4703-bb62-ebb3c1edeecd

## Ground rules
- Target the newer board (Brains 2, i.MX RT1010, serial 4630+): `brains2/` + `shared/duo/`.
  Older units (Brains 1, K20 chip) use `brains1/src`, which has its own sequencer copy.
  **Owner's DUO: serial 7813** => Brains 2 with resistive drum pads (official
  releases say serial 5000+). Restore image: official 2.4.0 release,
  `duo-imxrt_resistive_2.4.0.bin`.
  Build with `-DBOARD=DUO_BRAINS_2.3` (the default is 2.1). The 2.3 config
  auto-detects the speaker amp's polarity, so it runs on 2.1 and 2.3 boards alike;
  2.4.0 is very likely built this way. A 2.1 build on a 2.3 board may leave the
  speaker muted.
- Never touch startup, USB or MIDI code, including the SysEx "reboot to bootloader"
  handler (`shared/duo/MidiFunctions.h`, `SYSEX_REBOOT_BOOTLOADER`). The updater
  relies on it, and there's no documented button fallback on Brains 2.
- Rescue combo (on branch `variable-length`): hold **Up + Down** while powering on =>
  `BOARD_EnterROMBootloader()`, checked right after `pins_init()` in `main()`, before
  USB/audio. Reads the matrix directly (`rescue_combo_held()` in `apps/duo/buttons.h`).
  The LEDs stay dark in the bootloader (USB 1fc9:0145). Recovery: `echo | uv run
  update_firmware.py <file>` (no MIDI port, so it waits for Enter). Tested on the owner's
  DUO 2026-10-21: combo entered the bootloader and a reflash through it worked. Keep it in
  every build that gets flashed.
- After a flash the DUO once failed to restart (Play LED stuck teal/blue = last colour
  from `enter_dfu()`, Play unresponsive, not on USB). A full power cycle (unplug USB)
  fixed it and the new build ran fine. Not a firmware bug; try that before the rescue combo.
- The firmware's USB commit label is stamped when the build runs, which is before the
  commit, so it reads one commit behind. Compare the .bin files to check what's flashed.
- Firmware images in `restore/` (committed): `restore/duo-imxrt_resistive_2.4.0.bin`
  (official, sha256 75d2894c...), `restore/duo_variable-length_685a4e6.bin` and
  `restore/duo_main_bf912e4.bin` (sha256 c9adeaea...) and
  `restore/duo_drum-pads_e762bdb.bin` (the build now on the DUO, sha256 2aebe678...).
  Before this mod the DUO ran Dato main @ ed8a5eaf, board DUO_BRAINS_2.3.
- Test sequencer logic with the unit tests before building:
  `cd shared/duo/test && ./run_tests.sh`
- Build: Arm GNU Toolchain 12.3.rel1 is installed at `~/Applications/ArmGNUToolchain/12.3.rel1`
  (cmake and uv via Homebrew). From `brains2/apps`:
  `export TOOLCHAIN_DIR=$HOME/Applications/ArmGNUToolchain/12.3.rel1 && ../scripts/build.sh duo -DBOARD=DUO_BRAINS_2.3`
  TOOLCHAIN_DIR is the top folder (the one holding `bin/arm-none-eabi-gcc`), not the
  `arm-none-eabi` subfolder the Readme shows. Output:
  `brains2/apps/duo/build/flexspi_nor_release/duo_firmware.bin` (gitignored).
- Flash: `cd tools/updater && uv sync && uv run update_firmware.py <file>`.
  Keep an official release from GitHub releases on hand to restore.
- Don't push to origin (Dato's repo); its push URL is disabled in this checkout
  (`git remote set-url --push origin DISABLED-do-not-push-to-dato`).
- Personal fork: https://github.com/srcurran/duo-imxrt (public; forks of public repos
  must be). Remote name `fork`. `main` (pushed to `fork/main`, 2026-10-22) holds all the
  mods; `variable-length` and `play-led-breathe` are also pushed. 2026-10-23: main
  (voices, emulator, drum pads, Funk) pushed, with `drum-pads`, the voice branches
  (`claude/duo-voice-orchid-style-972622`, `claude/voice-mode-while-playing`,
  `claude/voice-short-notes`, `claude/voice-gate-swell`, `claude/voice-normalize-octaves`)
  and `claude/ko2-funky-bassline-voice-07075a`. CLAUDE.md and `restore/`
  are committed (so public on the fork). Local `main` is checked out in `~/dev/datoduo-main`.
- Dato's open PRs reviewed 2026-10-22: none taken. #188 (littlefs persistent settings) is
  the base to revisit for pattern memory; it's untested on hardware and touches startup.

## Mod 1: variable sequence length (done, branch `variable-length`)
- **Page mode** (owner's design, 2026-10-21): press and hold **Random** (BTN_SEQ1) while
  **stopped**. While running, Random works exactly as in stock firmware and nothing below
  applies. Random's stock one-off jump when stopped is gone (owner accepted that).
  `page_mode_held` + `in_page_mode()` in `globals.h`; page mode ends if Play starts.
  - Keyboard keys 1-8 => 8, 16, ... 64 steps (max 32 / keys 1-4 until 2026-10-23). Default 8 at power-on. Combo keys don't play
    or record a note; their release is swallowed too (`combo_keys_held`). Keys 5-10 play.
  - **Up/Down** => next/previous page, moving the cursor to the page's first step
    (`Sequencer::set_page`). Stops at the first/last page, no wrap-around (owner's call).
    Not transpose in page mode.
  - **Step button** => moves the cursor to that step (`Sequencer::set_cursor`) instead of
    toggling it. Neither move plays a note. Play continues from the cursor (the first
    note is the step after it, as in stock firmware).
  - LEDs: keyboard is a length meter (keys 1..N lit in their colours, rest dark) and the
    viewed page's key flashes (200 ms on/off), also with 8 steps.
- Page indicator outside page mode: when length > 8, the playing page's key (1-8) is off
  (owner found off more obvious than white). With 8 steps all keys are lit.
- Rests while stopped (owner's request 2026-10-22): pressing the step button under the
  breathing cursor toggles that step; turning it off moves the cursor on one step
  (`Sequencer::toggle_cursor_step`), so a rest is entered without going back a page.
  An off cursor step takes two presses (on, then off). Other step buttons toggle as before.
  Flashed with main (bf912e4) 2026-10-22.
- Earlier designs, replaced: Random + step buttons for length; Random + keys while running.
- The 8 step LEDs follow the playhead's page (1-8, 9-16, ...). Step buttons toggle
  `page * 8 + button` on the shown page (`Sequencer::step_on_cur_page`).
- Lengthening copies the existing pattern into the new steps (owner confirmed: keep copy).
  Side effect: shorten then lengthen overwrites the later steps.
- Sequencer core: `shared/duo/seq.h/.cpp` store `MAX_STEPS = 64` (was 32; raised 2026-10-23, branch `claude/sequence-64-steps`) with a runtime `length`.
  Helpers: `wrapped_step()`, `set_length()`, `cur_page()`, `cur_page_position()`,
  `step_on_cur_page()`. Unit tests cover length, wrapping, copying and paging.
- Builds cleanly for DUO_BRAINS_2.3 with the rescue combo. Earlier versions were flashed
  and tested hands-on 2026-10-21 (last: 0badb49, page key off). The page-mode version
  (18e6b80) was flashed and worked hands-on 2026-10-21. Mod 1 is done.
- Breathing Play LED when stopped (branch `play-led-breathe`, worktree
  `~/dev/datoduo-play-led`): cycle on millis(), now 2 s down / 2 s up (4 s, owner's
  request in the breathing session). Merged into `variable-length` 2026-10-21 (conflict in
  `Leds.h` resolved to use `cur_led`). Flashed 2026-10-21 (4aa03c0). Owner: stayed
  dark too long, so the minimum is now ~25% (`BREATHE_MIN = 64`); owner approved (de10689).
  The cursor step breathes too, in time with Play (`breathe_brightness(0)`, owner's
  choice in the bf912e4 merge; it used to be half a cycle opposite): note colour if
  enabled, white if empty.

## Perform mode (branch `claude/sequence-perform-mode-f19b2e`, flashed 2026-10-23)
- Owner's design, 2026-10-23: hold **Random** while stopped and press **Play** => the
  sequence runs in perform mode. Keys (and the arp) play over it but record nothing.
  Stop, then Play again => normal recording mode.
- Play LED is purple (`LED_PERFORM`) while performing, white otherwise. Step buttons
  still toggle steps; Up/Down still transpose; Random works as stock while running.
- The DUO is monophonic: a live note replaces the sequence note while held, and the
  sequence comes back on release.
- Code: `Sequencer::performing` (`set_performing()`, cleared by `stop()`, also on MIDI
  stop); `record_note()` returns early when set. Combo in `main.cpp` (SEQ_START while
  `in_page_mode()`). Five unit tests (`perform_mode_*`, `stop_ends_perform_mode`).
- Flashed 2026-10-23 (bd19148, .bin sha256 1aee3389...), replacing main bf912e4. Owner
  checked it hands-on the same day: works as desired. Merged into `main`. To go back: reflash `restore/duo_main_bf912e4.bin`.

## Mod 2: drum pads (done, branch `drum-pads`, flashed 2026-10-23)
Code: touch logic in `shared/duo/drum_pads.h` (pure C++, unit tested in
`test/firmware/drum_pads_test.cpp`); reading in `brains2/apps/duo/boards/DUO_BRAINS_2.3/drums.h`
(2.1 is a copy); sounds stay in `duo-firmware/src/DrumSynth.h`.
- Sounds: stock DUO kick and hat (owner's call). Each pad's left/middle/right plays the
  stock tone variation (velocity 0/63-64/127). A six-drum synth (kick/tom/snare,
  closed hat/clap/open hat, commit 74d3f68) was tried and dropped: too boomy.
- Pads are read one contact every ~330 us (each pad fully every ~2 ms; stock: 11 ms).
- Zones: left and left+middle => left; middle alone => middle; middle+right and right =>
  right. The zone must hold 6 ms (`SETTLE_MS`) before it plays, so a finger landing on two
  contacts plays one drum. A contact counts as lifted after 25 ms off (`RELEASE_MS`).
- Sliding into a new zone plays it. Once the middle has played, the finger must leave the
  middle contact to reach an edge (a finger on the middle brushes its neighbours).
- One finger at a time (owner's choice): a second finger on the far edge plays that edge;
  lifting a finger never plays. Thumb on an edge + finger on the middle plays nothing (the
  pad can't tell that from one finger on 1+2).
- Hold to repeat: held for 150 ms, the pad repeats on every sequencer step (clock runs
  when stopped; `Sequencer::ticks_per_step()`). A resting finger repeats too (accepted).
- Hat rings out its short decay on release instead of being cut. Kick still ducks the
  pulse osc while the kick pad is held (stock "sidechain").
- Pressure/impact: the pads sit on mux SYN_MUX2 on GPIO_AD_03 = ADC1 ch 3, so an analog
  level is readable, but it reflects skin contact, not strike speed. Owner skipped it.
- Owner checked it hands-on 2026-10-23 (e762bdb): works as desired. Merged into `main`.
- Later: shape drums with the knobs while a pad is held, record pad hits into the
  sequencer, send drums over MIDI.

## Voices (branch `claude/duo-voice-orchid-style-972622`)
- Owner's design, 2026-10-23: keep the original voice. Hold **Accent** ("boost" pad)
  while **stopped** => voice mode: keyboard keys 1-9 (now 1-10, see Funk) pick the voice; key 1 = Original
  (default at power-on). Like page mode, the keys don't play (key 10 still does) and it
  ends if Play starts. Random's page mode wins if both are held. LEDs: keys 1-9 lit,
  the selected one flashes. The choice is lost at power-off (the DUO saves nothing).
- Voices (table with each voice's Detune / Pulse Width knob roles at the top of the
  voice code in Synth.h). Order since 2026-10-23 (by family, punchy to lush): 1 Original,
  2 Acid, 3 Funk, 4 Fold, 5 Pluck, 6 FM, 7 Organ, 8 Strings, 9 Orchid, 10 Chord (was
  Original, Orchid, Organ, Strings, Acid, Pluck, FM, Fold, Chord, Funk). Trims are looked
  up by name (`voice_trim_db`), so reordering can't scramble levels. All share the filter, envelopes, bitcrusher, delay, glide and
  Accent. Fold: the Filter knob sets the fold amount, the filter acts as a low-pass gate.
- Orchid voice and the idea, inspired by the Telepathic Instruments Orchid and
  Quixotic7/ChoralRootFM1 (GPL-3, so ideas only, no code taken). Owner liked the Orchid
  voice hands-on 2026-10-23 and asked for all the others.
- Code: `voice_apply()` (fixed settings, starts from `voice_reset()` = Original),
  `voice_update_sources()` (knobs, every loop), `voice_note_on/off()` (Pluck strike,
  FM modulator envelope), all in Synth.h. Extra sources osc3, osc4, fm_mod ->
  fm_mod_env -> fm_carrier (Teensy phase modulation), pluck -> voice_mixer -> mixer1
  ch 2. fold_fx sits between mixer1 and filter1, lush_fx (chorus + reverb, per-voice
  `LushFx::Params`) after bitcrusher1. DSP in plain C++, host-tested:
  `shared/duo/lush_fx.h`, `pluck_fx.h` (Karplus-Strong), `fold_fx.h`.
- Original voice is unchanged: the new sources are silent and idle (amplitude 0 / not
  enabled) and fold_fx and lush_fx pass blocks through untouched.
- Envelope times past ~1.4 s round the step to zero and the note never ends
  (`LinearEnvelope::calcRate`), so keep attack/release under that.
- RAM: reverb runs at 22.05 kHz; Pluck's 4 KB loop buffer is in OCRAM (DMAMEM, not
  zeroed at boot, cleared on enable). DTCM (m_data) 24.4 KB of 32 KB, OCRAM 54.5 of
  64 KB (mostly AudioMemory). CPU not measured; estimated well under 10%.
- Unit tests: `lush_fx_test`, `voice_fx_test` (pluck tuning/decay/ring length, fold,
  organ vibrato).
- Levels (2026-10-23): set by the Mac emulator (below) as a per-voice trim table,
  `VOICE_TRIM_DB` in Synth.h, applied at each voice's input to the filter (Fold: at
  the folder's output). Each voice's median loudness (LUFS) across the emulator's knob
  sweep now matches Original's within 1 dB; no audible clipping (Pluck: 3-8 single
  samples per 4 s on bright Accent strikes; -4 dB would remove them but make Pluck
  quieter). Earlier rounds by ear (FM depth halved, Chord/Acid/Pluck/Fold cuts) remain
  as the voices' own level constants underneath. Flashed 2026-10-23
  (`restore/duo_voice-levels.bin`), not yet checked by ear.
- Branch `claude/voice-mode-while-playing` (owner's design 2026-10-23, after trying
  keys-switch-while-playing): **stopped**: Accent + key 1-9 picks the voice.
  **Playing**: keys play as normal (accented, stock); Accent + Up/Down = next/previous
  voice (also works stopped), wrapping round the corner from the last voice to the
  first and back (owner's request 2026-10-23; it used to stop at the ends). While Accent is held
  the keyboard LEDs show the voice keys (selected flashes), also while playing.
  Flashed 2026-10-23 (`restore/duo_voice-updown.bin`), not yet checked hands-on.
- Owner's dB-meter test 2026-10-23 (NIOSH SLM, LAeq per voice, same sequence) vs
  Original: Orchid -9, Organ -8, Strings -3.5, Acid -8, Pluck -10, FM -2, Fold +2.7,
  Chord -3. Owner only found Fold loud by ear, so Fold cut 4.5 dB (FOLD_OUTPUT 6800,
  b3dbfdd); the others left alone. A-weighting under-reads bass (Organ, Acid) and
  short sequenced notes never reach Orchid's slow attack, so trust ears over it.
- Flashed 2026-10-23: 540b6bb (Original + Orchid only, `restore/duo_voices_540b6bb.bin`),
  then 999d0ce (all nine voices, `restore/duo_voices_999d0ce.bin`, sha256 9c0d012c...).
  Not yet checked hands-on. Those flashes replaced the drum-pads build; voices were
  then merged into main with drum pads (2026-10-23).

- Fixes from the owner's video 2026-10-23 (branch `claude/voice-short-notes`):
  (1) Strings (and Orchid) went near-silent on short sequenced notes (~30 ms) with the
  Filter knob low: their slow swells (Strings amp 150 / filter 400 ms, Orchid filter
  250 ms) never got going. Now Strings 60 / 120 ms, Orchid filter 80 ms; within ~2-6 dB
  of Original at 30 ms notes and Filter at 0. The sweep now includes 30 ms notes.
  (2) Organ sounded "very sharp": through the speaker (no output below ~270 Hz) its
  sine fundamental vanished and with the octave plus an even drawbar (6x/8x) the ear
  heard the note an octave up. Organ now always adds a quint (3x) and the knob's
  drawbars are 4x/5x/6x/8x; heard pitch is right at every setting except the lowest
  key with Pulse Width high (there only the drawbar is above 270 Hz: a speaker limit).
  (3) Pluck trim -2 -> -4 dB (it rings regardless of note length; read +2.3 dB with
  short notes in the sweep; owner's DUO reading had it +4.7). No clipping left.

- Swells tied to the gate knob (2026-10-23, branch `claude/voice-gate-swell`, after
  the owner found Strings still dropping out on 5541b04): the gate knob goes down to
  10 ms notes (Sequencer.h maps it to 10-200 ms), and at 10 ms Strings/Orchid/Chord fell
  20-50 dB. `voice_swell(amp_ms, filter_ms)` sets each swelling voice's designed
  attacks (Orchid 40/250, Strings 150/400, Chord 20/120, the originals again);
  `swell_update()` in synth_update caps them at half the gate (amp) and the gate
  (filter). Short gates = plucky versions; held live keys get the cap too. All voices
  now within ~1-3.5 dB of Original at 10-60 ms notes; the sweep includes 10 ms notes
  (11664 renders); emu sets synth.gateLength from gate_ms.

- No silent spots (2026-10-23, branch `claude/voice-normalize-octaves`, from the
  owner's long video of all voices across octaves and filter settings). An emulator
  hunt over voices x octave (-12/0/+12) x filter x resonance x Detune/PW x 10/60 ms notes
  found 77 near-silent cases, all at Filter knob 0: Pluck/FM/Chord play an octave above
  Original and the low filter cut them; Fold at fold amount 0 (a soft triangle) and
  FM/Organ with low PW (near-pure sines) vanished an octave down below the speaker's
  range. Fixes: Pluck/FM/Chord filter base x3 (capped at the stock top, 541 Hz,
  FILTER_BASE_MAX); Fold minimum fold 0.2 (FOLD_MIN_AMOUNT); FM minimum depth 0.06
  (FM_MIN_DEPTH); Organ upper drawbar 0.08-0.3 (was 0-0.5; also less piercing),
  octave 0.45, quint 0.3, and triangles for the 8' and 4' (pure sines sounded whistly
  and piercing; Organ still has the least 1-4 kHz energy of any voice). Pluck trim
  -5.5 dB. Now 0 near-silent cases, no voice >9 dB under Original anywhere, medians
  within 1.1 dB. The level sweep includes octaves (34992 renders, ~55 s); emu prints
  transpose as its last column.

## Funk voice (branch `claude/ko2-funky-bassline-voice-07075a`, 2026-10-23)
- Owner's request: a voice like the funky KO II bass line at the start of
  youtube.com/shorts/DO63a4bfPqM (0:09-0:10). Analysed from the clip: a synth bass
  (odd harmonics strong, some even), notes 55-87 Hz, the filter shut to the
  fundamental within ~100 ms, the level ~-5 dB over 150 ms while held.
- Voice 10 = **Funk**, on keyboard key 10 (VOICE_COUNT is now 10: every key picks a
  voice in voice mode; Up/Down now wraps round). Funk is now key 3 (see the order above. Pulse (0.5) + a little saw (0.25) an
  octave down; amp: 1 ms attack, 200 ms decay to 0.6; filter env snaps open and
  shuts (sustain 0). **Detune** = filter snap 30-400 ms on a curve (noon ~120 ms
  matches the clip), **Pulse Width** = square to thin pulse. The Filter knob is
  half the stock range (30-285 Hz) and octaveControl 4.5, so it closes down to the
  fundamental. Stock resonance range, stock release.
- Level: no trim. Speaker -4 dB vs Original, headphones +1.6 dB: the speaker barely
  plays below ~270 Hz and any boost clips on Accent. `sweep.py` has `TARGET_DB`
  (Funk: -4) for this (later dropped, see below). The tone test still covers voices 1-9.
- Merged into main 2026-10-23 on top of the gate-knob swells and the octave/filter
  fixes (it was written on 34438c6 and left uncommitted; committed as 20ebc13).
  On merge: the -4 dB target was dropped (the full sweep with octaves and short notes
  puts Funk at +0.7 dB, on par), and its filter floor went 30 -> 60 Hz (an octave down
  at Filter 0 it shut to a bare 17-58 Hz fundamental: 2 near-silent cases, now 0).

## Mac emulator (tools/emu, 2026-10-23)
- Runs the real firmware audio code on the Mac: Teensy audio objects, AudioStream.cpp,
  Synth.h, Pitch.h, globals.h, seq.cpp. `tools/emu/shim/host_platform.h` replaces the
  M7 DSP instructions (dspinst.h) with exact C versions and makes interrupts no-ops;
  the code is built with __ARM_ARCH_7EM__ so it takes the DUO's own code paths.
- Build: `tools/emu/build.sh` (clang, ~5 s). Render: `tools/emu/build/emu key=3
  filter=512 resonance=512 detune=512 pw=512 release=512 accent=0 gate_ms=60
  step_ms=125 notes=49,54,58 wav=out.wav` (key=1..9 = voice). Prints LUFS, speaker
  LUFS (measured speaker model), peak, clipped samples. ~1000x real time.
- Sweep: `python3 tools/emu/sweep.py` (5832 renders: 9 voices x filter, resonance,
  detune, pw at 3 values x release 2 x Accent x 16ths/held notes, ~8 s).
  `--check` exits 1 if a voice's median is >1.5 dB off Original or a render clips
  >10 samples. Run it after any sound change.
- Checked: Original's pitches (pulse an octave down, saw at the Detune interval), FM
  harmonics and Chord intervals come out exactly right.
- Note on/off synth code moved from main.cpp into Synth.h (`synth_note_pitch`,
  `synth_note_start`, `synth_note_stop`) so the emulator shares it; main.cpp still
  sends MIDI in the same place.
- Tone test (2026-10-23): `brains2/apps/duo/tone_test.h`, in builds made with
  `DUO_TONE_TEST=1` in the environment (`DUO_TONE_TEST=1 ../scripts/build.sh duo
  -DBOARD=DUO_BRAINS_2.3`); an env var so it can't stick in the CMake cache. Press Play:
  start signal (two 1 kHz beeps), 14 equal-level sine tones 100 Hz-8 kHz (4 s + 2 s
  gap), 8 s gap, voices 1-9 with knobs at noon (6 s + 2 s gap), end beep; ~2:55. Play
  LED orange, step LEDs = progress, voice key lit. Knobs, sequencer, drums ignored;
  USB/MIDI keep running. Image: `restore/duo_tone-test.bin`. `emu test=1 wav=...`
  runs the identical test and prints the timetable. Analysis:
  `tools/emu/read_meter.py recording.mp4 > meter.csv` (macOS text recognition on the
  NIOSH SLM screen), then `uv run --with numpy --with scipy python
  tools/emu/tone_test_analysis.py steps.csv test.wav meter.csv` -> speaker+mic
  response per frequency and each voice measured vs emulated. Checked end to end with
  a fake recording (recovered a 300 Hz roll-off within ~0.5 dB).
- Found with the emulator: (1) the state-variable filter goes unstable above ~8.4 kHz
  at minimum resonance (the test's tone path uses 7 kHz); (2) the reverb multiplied the
  pulse wave's DC offset to 15% of full scale in Orchid; LushFx now has a ~10 Hz DC
  blocker (unit-tested), Orchid's peak dropped 0.45 -> 0.30.
- Speaker measured 2026-10-23 with the tone test (owner's screen recording of NIOSH
  SLM; the recording's own audio agreed within ~1 dB from 300 Hz up, no distortion
  above 300 Hz). Response re 1 kHz: 100 Hz -55, 150 -29, 200 -16, 300 -3, 400-6000 Hz
  roughly flat with dips at 700 (-9), 1500 (-11), 3000 (-17) (likely room/phone
  position), 8 kHz -28. At 100-150 Hz the speaker mostly makes harmonics. The test's
  1 kHz tone read ~103 dBA near the phone: loud; lower VOLUME/TONE_LEVEL if re-run.
  Emulator speaker model (lufs_spk) is now this shape: 6th-order high-pass 270 Hz +
  4th-order low-pass 6.5 kHz, dips left out. With it, every voice's median is within
  ~1 dB of Original (Organ -1.1); no trim changes needed.
- Voices at noon knobs, measured on the DUO vs Original (dBA): Orchid +1.1, Organ
  +2.9, Strings +1.6, Acid -1.7, Pluck +4.7, FM -2.3, Fold +2.0, Chord +3.1. The
  emulator predicts these within ~1.5 dB except a ~2 dB offset on Original's segment
  (Orchid, nearly the same sound, differed by 1.5 dB: measurement noise of one phone
  position) and Pluck (random strike).
- Pluck clipping (Accent, bright, filter knob high): the strike reaches ~95% at the
  source and the filter peaks near its top on that noise burst; not the reverb (emu
  `nolush=1` and `probe=1` showed it). Left as is: 3-8 single samples per 4 s.
  Scaling the strike by ring length was tried and dropped (spread is mostly brightness
  and filter; the +3 dB on short rings made clipping worse).
- Normal build flashed back 2026-10-23 (`restore/duo_voice-levels.bin`, includes the
  Orchid DC blocker).
- Limits: the speaker model leaves out room/position dips, and a single phone position
  is good to ~1.5 dB; ears have the final say.

## Feature backlog (from other small synths)
Scales/root (keys are fixed to `SCALE[]` = black-key pentatonic in `globals.h`),
arp modes, swing, per-step accent/slide, probability/mutate, ratchets,
pattern memory + chaining (the DUO saves nothing at power-off today), punch-in
effects, LFO, motion recording, more oscillator types, Euclidean drums.
Top picks: scales, pattern memory, punch-in effects.

## Which build to flash
- Always flash builds from `main` (owner's decision 2026-10-22): it has Mod 1 (paging,
  rests), the rescue combo and the breathing LEDs (25% floor, cursor step in time with
  Play), perform mode, Mod 2 (drum pads) and the voices (Accent + keys / Up/Down).
  Flashed 2026-10-23: main 143c8f4 (adds Funk, voice 10 on key 10,
  `restore/duo_main_143c8f4.bin`; label verified). Before it: a6392f7 (no silent spots
  across octaves/filter, softer Organ). Before it: cd72c4e (swells
  follow the gate knob,
  `restore/duo_main_cd72c4e.bin`; label verified over USB with
  `tools/updater/firmware_info.py`). Before it: 5541b04 (owner: 3 still sharp, 4 still
  dropped out). Before it: 34438c6 (owner tried it: Organ
  sharp, Strings silent on short notes, both fixed in 5541b04).
- `variable-length` is the older Mod 1 branch (cursor step breathes opposite Play).
  `play-led-breathe` is cut from Dato's main: no Mod 1, no rescue combo. On 2026-10-21 a
  play-led-breathe build was flashed by mistake and Mod 1 disappeared.
- New work: branch off `main` and merge back into it.
