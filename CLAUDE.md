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

## Feature backlog (from other small synths)
Scales/root (keys are fixed to `SCALE[]` = black-key pentatonic in `globals.h`),
arp modes, swing, per-step accent/slide, probability/mutate, ratchets,
pattern memory + chaining (the DUO saves nothing at power-off today), punch-in
effects, LFO, motion recording, more oscillator types, Euclidean drums.
Top picks: scales, pattern memory, punch-in effects.

## Which build to flash
- Always flash builds from `main` (owner's decision 2026-10-22): it has Mod 1 (paging,
  rests), the rescue combo and the breathing LEDs (25% floor, cursor step in time with
  Play), perform mode and Mod 2 (drum pads). Last flashed 2026-10-23 (drum-pads
  e762bdb, merged into main the same day); owner checked it hands-on.
- `variable-length` is the older Mod 1 branch (cursor step breathes opposite Play).
  `play-led-breathe` is cut from Dato's main: no Mod 1, no rescue combo. On 2026-10-21 a
  play-led-breathe build was flashed by mistake and Mod 1 disappeared.
- New work: branch off `main` and merge back into it.
