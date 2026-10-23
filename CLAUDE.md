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
  `restore/duo_main_bf912e4.bin` (the build now on the DUO, sha256 c9adeaea...).
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
  mods; `variable-length` and `play-led-breathe` are also pushed. CLAUDE.md and `restore/`
  are committed (so public on the fork). Local `main` is checked out in `~/dev/datoduo-main`.
- Dato's open PRs reviewed 2026-10-22: none taken. #188 (littlefs persistent settings) is
  the base to revisit for pattern memory; it's untested on hardware and touches startup.

## Mod 1: variable sequence length (done, branch `variable-length`)
- **Page mode** (owner's design, 2026-10-21): press and hold **Random** (BTN_SEQ1) while
  **stopped**. While running, Random works exactly as in stock firmware and nothing below
  applies. Random's stock one-off jump when stopped is gone (owner accepted that).
  `page_mode_held` + `in_page_mode()` in `globals.h`; page mode ends if Play starts.
  - Keyboard key 1/2/3/4 => 8/16/24/32 steps. Default 8 at power-on. Combo keys don't play
    or record a note; their release is swallowed too (`combo_keys_held`). Keys 5-10 play.
  - **Up/Down** => next/previous page, moving the cursor to the page's first step
    (`Sequencer::set_page`). Stops at the first/last page, no wrap-around (owner's call).
    Not transpose in page mode.
  - **Step button** => moves the cursor to that step (`Sequencer::set_cursor`) instead of
    toggling it. Neither move plays a note. Play continues from the cursor (the first
    note is the step after it, as in stock firmware).
  - LEDs: keyboard is a length meter (keys 1..N lit in their colours, rest dark) and the
    viewed page's key flashes (200 ms on/off), also with 8 steps.
- Page indicator outside page mode: when length > 8, the playing page's key (1-4) is off
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
- Sequencer core: `shared/duo/seq.h/.cpp` store `MAX_STEPS = 32` with a runtime `length`.
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

## Voices (branch `claude/duo-voice-orchid-style-972622`)
- Owner's design, 2026-10-23: keep the original voice. Hold **Accent** ("boost" pad)
  while **stopped** => voice mode: keyboard keys 1-9 pick the voice; key 1 = Original
  (default at power-on). Like page mode, the keys don't play (key 10 still does) and it
  ends if Play starts. Random's page mode wins if both are held. LEDs: keys 1-9 lit,
  the selected one flashes. The choice is lost at power-off (the DUO saves nothing).
- Voices (table with each voice's Detune / Pulse Width knob roles at the top of the
  voice code in Synth.h): 1 Original, 2 Orchid, 3 Organ, 4 Strings, 5 Acid, 6 Pluck,
  7 FM, 8 Fold, 9 Chord. All share the filter, envelopes, bitcrusher, delay, glide and
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
  voice (also works stopped), stopping at voice 1 / 9, no wrap. While Accent is held
  the keyboard LEDs show the voice keys (selected flashes), also while playing.
  Flashed 2026-10-23 (`restore/duo_voice-updown.bin`), not yet checked hands-on.
- Owner's dB-meter test 2026-10-23 (NIOSH SLM, LAeq per voice, same sequence) vs
  Original: Orchid -9, Organ -8, Strings -3.5, Acid -8, Pluck -10, FM -2, Fold +2.7,
  Chord -3. Owner only found Fold loud by ear, so Fold cut 4.5 dB (FOLD_OUTPUT 6800,
  b3dbfdd); the others left alone. A-weighting under-reads bass (Organ, Acid) and
  short sequenced notes never reach Orchid's slow attack, so trust ears over it.
- Flashed 2026-10-23: 540b6bb (Original + Orchid only, `restore/duo_voices_540b6bb.bin`),
  then 999d0ce (all nine voices, `restore/duo_voices_999d0ce.bin`, sha256 9c0d012c...).
  Not yet checked hands-on. Each flash replaced a drum-pads build from another
  session; drum pads are done and merged into main (d77e027) but not in this branch,
  so the DUO has no drum pads until voices is merged into main and main is flashed.

## Mac emulator (tools/emu, 2026-10-23)
- Runs the real firmware audio code on the Mac: Teensy audio objects, AudioStream.cpp,
  Synth.h, Pitch.h, globals.h, seq.cpp. `tools/emu/shim/host_platform.h` replaces the
  M7 DSP instructions (dspinst.h) with exact C versions and makes interrupts no-ops;
  the code is built with __ARM_ARCH_7EM__ so it takes the DUO's own code paths.
- Build: `tools/emu/build.sh` (clang, ~5 s). Render: `tools/emu/build/emu key=3
  filter=512 resonance=512 detune=512 pw=512 release=512 accent=0 gate_ms=60
  step_ms=125 notes=49,54,58 wav=out.wav` (key=1..9 = voice). Prints LUFS, speaker
  LUFS (150 Hz high-pass), peak, clipped samples. ~1000x real time.
- Sweep: `python3 tools/emu/sweep.py` (5832 renders: 9 voices x filter, resonance,
  detune, pw at 3 values x release 2 x Accent x 16ths/held notes, ~8 s).
  `--check` exits 1 if a voice's median is >1.5 dB off Original or a render clips
  >10 samples. Run it after any sound change.
- Checked: Original's pitches (pulse an octave down, saw at the Detune interval), FM
  harmonics and Chord intervals come out exactly right.
- Note on/off synth code moved from main.cpp into Synth.h (`synth_note_pitch`,
  `synth_note_start`, `synth_note_stop`) so the emulator shares it; main.cpp still
  sends MIDI in the same place.
- Limits: no speaker or room model. Fitting a speaker bass cut-off to the owner's
  dB-meter test didn't help (knob positions in that test unknown; its Original segment
  read ~8 dB high), so levels match the electrical output; ears have the final say.

## Mod 2: drum pads (planned)
Code: `brains2/apps/duo/duo-firmware/src/DrumSynth.h`, `brains2/apps/duo/boards/*/drums.h`.
- Timing: pads are polled only every ~11 ms (`millis() - update_time > 10`). Poll every
  loop, and use asymmetric debounce: fire on first contact, then ignore the pad briefly.
  Retrigger on a new zone touch, and let the hat ring out its decay instead of cutting
  on release.
- Sound: the kick is a plain `AudioSynthSimpleDrum` sine. Add a click transient, a
  two-stage pitch drop and saturation. Add a real snare and an open hat.
- Hold to repeat: a held pad retriggers on the sequencer grid (`TICKS_PER_STEP`; the clock
  runs even when stopped). The first hit is immediate; a tap stays a single hit.
- Later: shape drums with the knobs while a pad is held, drum kits, record pad hits into
  the sequencer, send drums over MIDI.

## Feature backlog (from other small synths)
Scales/root (keys are fixed to `SCALE[]` = black-key pentatonic in `globals.h`),
arp modes, swing, per-step accent/slide, probability/mutate, ratchets,
pattern memory + chaining (the DUO saves nothing at power-off today), punch-in
effects, LFO, motion recording, more oscillator types, Euclidean drums.
Top picks: scales, pattern memory, punch-in effects.

## Which build to flash
- Always flash builds from `main` (owner's decision 2026-10-22): it has Mod 1 (paging,
  rests), the rescue combo and the breathing LEDs (25% floor, cursor step in time with
  Play). Flashed 2026-10-22 (bf912e4); owner checked it hands-on the same day and
  everything works as expected.
- `variable-length` is the older Mod 1 branch (cursor step breathes opposite Play).
  `play-led-breathe` is cut from Dato's main: no Mod 1, no rescue combo. On 2026-10-21 a
  play-led-breathe build was flashed by mistake and Mod 1 disappeared.
- New work: branch off `main` and merge back into it.
