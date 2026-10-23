"""Reads a dB-meter app's live A-weighted level from a phone screen recording.

  python3 tools/emu/read_meter.py recording.mp4 > meter.csv

Writes "seconds,dBA" rows (seconds from the start of the video), for
tools/emu/tone_test_analysis.py. Reads 4 frames a second with ffmpeg and the
macOS text recognizer (meter_ocr.swift, built on first use), looking for the
"<value> dB(A)" reading (NIOSH Sound Level Meter's "Instantaneous level").
Frames where it can't find one are skipped.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
OCR = os.path.join(HERE, "build", "meter_ocr")
if not os.path.exists(OCR):
    os.makedirs(os.path.dirname(OCR), exist_ok=True)
    subprocess.run(["swiftc", "-O", os.path.join(HERE, "meter_ocr.swift"), "-o", OCR], check=True)

FPS = 4
with tempfile.TemporaryDirectory() as tmp:
    subprocess.run(["ffmpeg", "-v", "error", "-i", sys.argv[1], "-vf", f"fps={FPS},scale=800:-1",
                    os.path.join(tmp, "%05d.png")], check=True)
    frames = sorted(f for f in os.listdir(tmp) if f.endswith(".png"))
    out = []
    for i in range(0, len(frames), 200):
        batch = [os.path.join(tmp, f) for f in frames[i:i + 200]]
        out += subprocess.run([OCR] + batch, capture_output=True, text=True).stdout.splitlines()
for line in out:
    name, _, text = line.partition("\t")
    m = re.search(r"([\d]+\.\d) ?dB\(A\)", text)
    if m:
        print(f"{(int(name[:5]) - 1) / FPS:.2f},{m[1]}")
