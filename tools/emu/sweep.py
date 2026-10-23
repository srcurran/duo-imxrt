"""Renders every voice across a grid of knob settings with the emulator and
compares each voice's loudness with the Original voice at the same settings.

Usage (from the repo root, after tools/emu/build.sh):
  python3 tools/emu/sweep.py            # full grid, summary table
  python3 tools/emu/sweep.py --quick    # mid knobs only
  python3 tools/emu/sweep.py --check    # exit 1 if a voice's median is more
                                        # than 1.5 dB off Original, or any
                                        # render clips more than 10 samples

For each voice: the median, 10th and 90th percentile of its loudness minus
Original's at identical settings ("speaker" = LUFS through the DUO speaker
model in emu.cpp, measured with the tone test), the loudest setting, and
clipped samples.
Raw results go to tools/emu/build/sweep.csv.
"""
import csv
import itertools
import os
import statistics
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
EMU = os.path.join(HERE, "build", "emu")
VOICES = ["Original", "Orchid", "Organ", "Strings", "Acid", "Pluck", "FM", "Fold", "Chord"]
FIELDS = ["voice", "filter", "resonance", "release", "detune", "pw", "accent", "gate_ms",
          "step_ms", "lufs", "lufs_spk", "peak", "clipped", "transpose"]

if "--quick" in sys.argv:
    GRID = dict(filter=[512], resonance=[512], detune=[512], pw=[512], release=[512],
                accent=[0], style=[(125, 60)])
else:
    GRID = dict(filter=[128, 512, 896], resonance=[0, 512, 1023], detune=[128, 512, 896],
                pw=[128, 512, 896], release=[256, 768], accent=[0, 1],
                transpose=[-12, 0, 12],  # Up/Down octaves
                style=[(125, 10), (125, 30), (125, 60), (1000, 800)])
                # (step_ms, gate_ms): 16ths at the gate knob's minimum (10 ms),
                # very short, normal, and held notes


def render(args):
    out = subprocess.run([EMU] + args, capture_output=True, text=True, check=True).stdout
    return dict(zip(FIELDS, out.strip().split(",")))


def settings():
    keys = list(GRID)
    for combo in itertools.product(*GRID.values()):
        s = dict(zip(keys, combo))
        step, gate = s.pop("style")
        s.update(step_ms=step, gate_ms=gate)
        yield s


jobs = []
for v in range(len(VOICES)):
    for s in settings():
        jobs.append([f"voice={v}"] + [f"{k}={val}" for k, val in s.items()])
with ThreadPoolExecutor(max_workers=os.cpu_count()) as pool:
    rows = list(pool.map(render, jobs))

os.makedirs(os.path.join(HERE, "build"), exist_ok=True)
with open(os.path.join(HERE, "build", "sweep.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, FIELDS)
    w.writeheader()
    w.writerows(rows)

key = lambda r: tuple(r[k] for k in FIELDS[1:9]) + (r["transpose"],)
original = {key(r): r for r in rows if r["voice"] == "0"}


def pct(xs, p):
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(p / 100 * len(xs)))]


print(f"{len(rows)} renders. Loudness vs Original at the same settings (dB):")
print(f"{'key voice':12s} {'speaker':>8s} {'p10':>6s} {'p90':>6s} {'full':>7s}  "
      f"{'loudest setting (speaker LUFS)':34s} {'clipped':>8s}")
for v, name in enumerate(VOICES):
    mine = [r for r in rows if r["voice"] == str(v)]
    d_spk = [float(r["lufs_spk"]) - float(original[key(r)]["lufs_spk"]) for r in mine]
    d_full = [float(r["lufs"]) - float(original[key(r)]["lufs"]) for r in mine]
    loud = max(mine, key=lambda r: float(r["lufs_spk"]))
    clipped = sum(int(r["clipped"]) > 0 for r in mine)
    desc = (f"{float(loud['lufs_spk']):.1f} f{loud['filter']} r{loud['resonance']} "
            f"d{loud['detune']} pw{loud['pw']} a{loud['accent']}")
    print(f"{v + 1}   {name:8s} {statistics.median(d_spk):+8.1f} {pct(d_spk, 10):+6.1f} "
          f"{pct(d_spk, 90):+6.1f} {statistics.median(d_full):+7.1f}  {desc:34s} "
          f"{clipped:>4d}/{len(mine)}")

if "--check" in sys.argv:
    problems = []
    for v, name in enumerate(VOICES):
        mine = [r for r in rows if r["voice"] == str(v)]
        d = statistics.median(float(r["lufs_spk"]) - float(original[key(r)]["lufs_spk"]) for r in mine)
        if abs(d) > 1.5:
            problems.append(f"{name}: median {d:+.1f} dB vs Original")
        worst = max(int(r["clipped"]) for r in mine)
        if worst > 10:
            problems.append(f"{name}: clips {worst} samples in one render")
    print("\nLEVEL CHECK: " + ("OK" if not problems else "FAILED\n  " + "\n  ".join(problems)))
    sys.exit(1 if problems else 0)
