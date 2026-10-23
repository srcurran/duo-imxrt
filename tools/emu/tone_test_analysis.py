"""Analyses the tone test (brains2/apps/duo/tone_test.h).

1. Emulated levels of every step:
     tools/emu/build/emu test=1 wav=test.wav > steps.csv
     python3 tools/emu/tone_test_analysis.py steps.csv test.wav
2. Against a dB-meter reading of the DUO running the test:
     python3 tools/emu/tone_test_analysis.py steps.csv test.wav meter.csv
   meter.csv: one row per reading, "seconds,dBA" (the meter's live A-weighted
   level, e.g. read from a screen recording with tools/emu/meter_ocr.swift).
   The start signal (two 1 kHz beeps) lines the reading up with the
   timetable; each step's level is the power average of its readings,
   skipping the first second and the last half second.

Prints, per tone, the speaker + mic response (measured dBA minus the tone's
electrical level and minus A-weighting at that frequency, relative to 1 kHz),
and per voice, the measured and emulated loudness relative to Original.

Run with: uv run --with numpy --with scipy python tools/emu/tone_test_analysis.py ...
"""
import csv
import sys

import numpy as np
from scipy import signal
from scipy.io import wavfile

SR = 44100
VOICES = ["Original", "Acid", "Funk", "Fold", "Pluck", "FM", "Organ", "Strings", "Orchid", "Chord"]  # keys 1-10, as globals.h


def a_weighting_db(f):
    f2 = np.asarray(f, dtype=float) ** 2
    ra = (12194**2 * f2**2) / ((f2 + 20.6**2) * np.sqrt((f2 + 107.7**2) * (f2 + 737.9**2)) * (f2 + 12194**2))
    return 20 * np.log10(ra) + 2.0


def a_filter():
    f1, f2, f3, f4 = 20.598997, 107.65265, 737.86223, 12194.217
    nums = [(2 * np.pi * f4) ** 2 * (10 ** (1.9997 / 20)), 0, 0, 0, 0]
    dens = np.polymul([1, 4 * np.pi * f4, (2 * np.pi * f4) ** 2], [1, 4 * np.pi * f1, (2 * np.pi * f1) ** 2])
    dens = np.polymul(np.polymul(dens, [1, 2 * np.pi * f3]), [1, 2 * np.pi * f2])
    return signal.bilinear(nums, dens, SR)


steps = list(csv.DictReader(open(sys.argv[1])))
sr, x = wavfile.read(sys.argv[2])
x = x.astype(float) / 32768
ab, aa = a_filter()
xa = signal.lfilter(ab, aa, x)


def window(s):
    t0 = int(s["start_ms"]) / 1000 + 1.0
    t1 = (int(s["start_ms"]) + int(s["ms"])) / 1000 - 0.5
    return t0, t1


def db(v):
    return 10 * np.log10(np.mean(v**2) + 1e-20)


emu = []
for s in steps:
    t0, t1 = window(s)
    seg, sega = x[int(t0 * SR):int(t1 * SR)], xa[int(t0 * SR):int(t1 * SR)]
    emu.append(dict(s, rms=db(seg), dba=db(sega)))

print("Emulated levels (dB re full scale):")
print(" step  start  what            RMS    A-weighted")
for e in emu:
    what = e["kind"] + (f" {e['hz']} Hz" if e["kind"] == "tone" else
                        f" {VOICES[int(e['voice']) - 1]}" if e["kind"] == "voice" else "")
    print(f" {int(e['step']):3d} {int(e['start_ms'])/1000:6.1f}  {what:14s} {e['rms']:6.1f} {e['dba']:8.1f}")

if len(sys.argv) < 4:
    sys.exit(0)

# ---- Line the meter reading up with the timetable --------------------------
meter = np.array([[float(a), float(b)] for a, b in csv.reader(open(sys.argv[3])) if a and b])
mt, mv = meter[:, 0], meter[:, 1]
# Expected loud/quiet pattern from the emulation, on a 0.1 s grid.
grid = np.arange(0, len(x) / SR, 0.1)
win = int(0.1 * SR)
emu_env = np.array([db(xa[int(t * SR):int(t * SR) + win]) for t in grid])
loud_emu = (emu_env > emu_env.max() - 40).astype(float)
meter_floor = np.percentile(mv, 10)
best = None
for offset in np.arange(-5, mt.max(), 0.1):  # meter time = emulated time + offset
    idx = np.clip(((mt - offset) / 0.1).astype(int), 0, len(grid) - 1)
    valid = (mt - offset >= 0) & (mt - offset < grid[-1])
    if valid.sum() < 20:
        continue
    loud_meter = (mv > meter_floor + 12).astype(float)
    score = np.mean(loud_meter[valid] == loud_emu[idx[valid]])
    if best is None or score > best[0]:
        best = (score, offset)
score, offset = best
print(f"\nMeter lined up: emulated t=0 is meter t={offset:.1f} s ({score*100:.0f}% of readings agree "
      f"on sound vs silence)")


def measured(e):
    t0, t1 = window(e)
    sel = (mt >= t0 + offset) & (mt <= t1 + offset)
    if not sel.any():
        return None
    return 10 * np.log10(np.mean(10 ** (mv[sel] / 10)))


tones = [e for e in emu if e["kind"] == "tone" and 4 <= int(e["step"]) < 32]
ref = next(e for e in tones if e["hz"] == "1000")
ref_m = measured(ref)
print("\nSpeaker + mic response (dB, relative to 1 kHz):")
print("    Hz   measured dBA   response")
for e in tones:
    m = measured(e)
    hz = float(e["hz"])
    resp = (m - ref_m) - (e["rms"] - ref["rms"]) - (a_weighting_db(hz) - a_weighting_db(1000))
    print(f" {int(hz):5d}   {m:8.1f}      {resp:+6.1f}")

voices = [e for e in emu if e["kind"] == "voice"]
v0 = voices[0]
m0 = measured(v0)
print("\nVoices vs Original (dBA): measured on the DUO, emulated (no speaker model)")
for e in voices:
    m = measured(e)
    print(f" {int(e['voice'])} {VOICES[int(e['voice']) - 1]:9s} {m - m0:+6.1f}   {e['dba'] - v0['dba']:+6.1f}")
