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
  (official, sha256 75d2894c...) and `restore/duo_variable-length_685a4e6.bin`.
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
  must be). Remote name `fork`. Branches `variable-length` and `play-led-breathe` are
  pushed and track `fork/...`. CLAUDE.md and `restore/` are committed on `variable-length`
  (so public on the fork).

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
  The cursor step breathes too, half a cycle opposite Play (`breathe_brightness(32768)`):
  note colour if enabled, white if empty. Flashed with the no-wrap page change
  (70d6d0a) 2026-10-21, restarted fine; hands-on check pending.

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
- Always flash builds from `variable-length`: it has Mod 1, the rescue combo and the
  breathing LEDs. `play-led-breathe` is cut from Dato's main: no Mod 1, no rescue combo.
  On 2026-10-21 a play-led-breathe build was flashed by mistake and Mod 1 disappeared.
- play-led-breathe's later step-marker variants (59177c8, 317eb6d: step fades fully out
  in sync with Play) were NOT merged; variable-length keeps the owner's choice from the
  main session: step breathes opposite Play, 25% floor.
