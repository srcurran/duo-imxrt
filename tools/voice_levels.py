"""Simulates each DUO voice (as set in Synth.h) and measures its loudness.

Signal path per voice: oscillators -> mixer1 gains -> (fold) -> state-variable
lowpass with envelope -> amp envelope -> (chorus + reverb). Loudness is the
A-weighted RMS of the loudest 50 ms in the first second of a held note,
averaged over a few notes and knob settings. Prints each voice relative to
Original and the trim that would match it.
"""
import math
import numpy as np
from scipy import signal

SR = 44100
N = SR  # 1 s per note
OS = 8  # oversampling for naive waveforms
t = np.arange(N) / SR


def naive(kind, freq, amp, pw=0.5, n=N):
    """Teensy-style waveform, band-limited by oversampling + decimation."""
    if amp == 0:
        return np.zeros(n)
    ph = (np.arange(n * OS) * freq / (SR * OS)) % 1.0
    if kind == "sine":
        y = np.sin(2 * np.pi * ph)
    elif kind == "saw":
        y = 2 * ph - 1
    elif kind == "square":
        y = np.where(ph < 0.5, 1.0, -1.0)
    elif kind == "pulse":
        y = np.where(ph < pw, 1.0, -1.0)
    elif kind == "triangle":
        y = 1 - 4 * np.abs(ph - 0.5)
    else:
        raise ValueError(kind)
    return amp * signal.resample_poly(y, 1, OS)[:n]


def linear_env(attack_ms, decay_ms=0, sustain=1.0, n=N):
    e = np.ones(n) * sustain
    a = max(1, int(attack_ms * SR / 1000))
    e[:a] = np.linspace(0, 1, a)
    if decay_ms > 0:
        d = int(decay_ms * SR / 1000)
        seg = np.linspace(1, sustain, d)
        e[a:a + d] = seg[: max(0, min(d, n - a))]
    else:
        e[a:] = 1.0 if sustain == 1.0 else sustain
    return e


def svf_lowpass(x, fc_base, ctl, octaves, q):
    """Chamberlin SVF, 2x oversampled, like Teensy's filter_variable."""
    damp = 1.0 / max(0.7, min(5.0, q))
    lp = bp = 0.0
    prev = 0.0
    out = np.empty(len(x))
    for i in range(len(x)):
        f = fc_base * 2 ** (ctl[i] * octaves)
        f = min(f, SR / 2.5)
        fm = 2 * math.sin(math.pi * f / (SR * 2))
        inp = x[i]
        lp += fm * bp
        hp = (inp + prev) * 0.5 - lp - damp * bp
        prev = inp
        bp += fm * hp
        lp1 = lp
        lp += fm * bp
        hp = inp - lp - damp * bp
        bp += fm * hp
        out[i] = max(-1.0, min(1.0, (lp + lp1) * 0.5))  # int16 saturation
    return out


# ---- LushFx port (chorus + half-rate reverb) ----
def lush(x, p):
    dry, cho, rev_mix, r1, d1, r2, d2, gain = p
    SIZE = 1024
    buf = np.zeros(SIZE)
    pos = 0
    ph1, ph2 = 0.0, 0.5
    c = 7.5e-3 * SR
    combs = [558, 594, 638, 678]
    aps = [278, 220]
    cb = [np.zeros(L) for L in combs]
    cp = [0] * 4
    cf = [0.0] * 4
    ab = [np.zeros(L) for L in aps]
    apos = [0] * 2
    rev_prev = 0.0
    out = np.empty(len(x))

    def tap(ph, depth):
        tri = ph * 2 if ph < 0.5 else 2 - ph * 2
        return c + depth * SR / 1000 * (2 * tri - 1)

    def read(d):
        idx = pos - d + SIZE
        i = int(idx)
        fr = idx - i
        a = buf[i % SIZE]
        b = buf[(i + 1) % SIZE]
        return a + (b - a) * fr

    def chorused(v):
        nonlocal pos, ph1, ph2
        buf[pos] = v
        wet = (read(tap(ph1, d1)) + read(tap(ph2, d2))) * 0.5
        ph1 = (ph1 + r1 / SR) % 1
        ph2 = (ph2 + r2 / SR) % 1
        pos = (pos + 1) % SIZE
        return v * dry + wet * cho

    def reverb(v):
        x0 = v * 0.15
        o = 0.0
        for k in range(4):
            y = cb[k][cp[k]]
            cf[k] = y * 0.65 + cf[k] * 0.35
            cb[k][cp[k]] = x0 + cf[k] * 0.82
            cp[k] = (cp[k] + 1) % combs[k]
            o += y
        for k in range(2):
            dl = ab[k][apos[k]]
            ab[k][apos[k]] = o + dl * 0.5
            o = dl - o
            apos[k] = (apos[k] + 1) % aps[k]
        return o / (0.15 * 4)

    for i in range(0, len(x) - 1, 2):
        v0 = chorused(x[i])
        v1 = chorused(x[i + 1])
        r = reverb((v0 + v1) * 0.5)
        r0 = (rev_prev + r) * 0.5
        rev_prev = r
        out[i] = (v0 + r0 * rev_mix) * gain
        out[i + 1] = (v1 + r * rev_mix) * gain
    return out


ORCHID = (0.7, 0.6, 0.45, 0.5, 2.5, 0.83, 2.5, 0.8)
ORGAN = (0.55, 0.75, 0.25, 6.7, 0.35, 7.3, 0.25, 0.9)
STRINGS = (0.5, 0.85, 0.35, 0.6, 3.0, 5.8, 0.3, 0.85)
PLUCK = (0.9, 0.2, 0.4, 0.4, 1.5, 0.6, 1.5, 0.85)
FM_L = (0.75, 0.5, 0.3, 0.7, 1.5, 1.1, 1.5, 0.85)


def pluck_src(hz, level, brightness, sustain):
    SIZE = 2048
    buf = np.zeros(SIZE)
    c = 0.5 - 0.45 * brightness
    ring = 0.3 * math.exp(3 * sustain)
    fb = math.exp(-6.9078 / (ring * hz))
    delay = SR / hz - c
    rng = np.random.default_rng(1)
    lp = 0.0
    eb = 0.25 + 0.75 * brightness
    pos = 0
    for k in range(int(delay) + 2):
        lp += (rng.uniform(-1, 1) - lp) * eb
        buf[(pos - 1 - k) % SIZE] = lp * level * 26000 / 32768
    out = np.empty(N)
    for i in range(N):
        def rd(d):
            idx = pos - d + SIZE
            j = int(idx)
            fr = idx - j
            return buf[j % SIZE] + (buf[(j + 1) % SIZE] - buf[j % SIZE]) * fr
        x0 = rd(delay)
        x1 = rd(delay + 1)
        buf[pos] = ((1 - c) * x0 + c * x1) * fb
        pos = (pos + 1) % SIZE
        out[i] = x0
    return out


def fold(x):
    tt = (x + 1) * 0.25
    fr = tt - np.floor(tt)
    return 1 - 4 * np.abs(fr - 0.5)


def detune_freq(note_hz, amount):
    if amount > 850:
        return note_hz * (amount + 9000) / 10240
    for lim, semis in ((680, 4), (560, 5), (350, 7), (200, 9)):
        if amount > lim:
            return note_hz * 2 ** (-semis / 12)
    return note_hz / 2 * (20000 - amount) / 20000


def voice(name, note_midi, dt_k, pw_k, filt_k=512, res_k=512):
    note = 440 * 2 ** ((note_midi - 69) / 12)
    f0 = note / 2
    pw = pw_k / 1023
    dt = dt_k / 1023
    vel = 100 / 127
    fbase = filt_k / 2 + 30
    octaves = 4
    res_max = 320
    amp_attack = 2
    f_attack, f_decay, f_sus = 15, 0, 1.0
    fx = None
    m0, m1, m2 = 0.2, 0.4, 0.0  # mixer1 gains: saw, pulse, extra
    extra = np.zeros(N)
    saw = pulse = np.zeros(N)

    if name in ("Original", "Orchid"):
        sa = 0.4
        if dt_k > 850:
            sa = (400 - (dt_k - 850) * 400 / 173) / 1000
        elif dt_k < 200:
            sa = (200 + dt_k) / 1000
        saw = naive("saw", detune_freq(note, dt_k), sa)
        pulse = naive("pulse", f0, 0.5, 0.5 + 0.45 * pw)
        if name == "Orchid":
            amp_attack, f_attack, res_max, fx = 40, 250, 200, ORCHID
    elif name == "Organ":
        pulse = naive("sine", f0, 0.6)
        extra = naive("sine", note, 0.35)
        bars = [3, 4, 6, 8]
        saw = naive("sine", f0 * bars[min(3, dt_k * 4 // 1024)], pw * 0.5)
        m0, m2, amp_attack, res_max, fx = 0.4, 0.4, 4, 200, ORGAN
    elif name == "Strings":
        s = 0.002 + 0.013 * dt
        pulse = naive("saw", f0, 0.5)
        saw = naive("saw", f0 * (1 + s), 0.5)
        extra = naive("saw", f0 * (1 - s), 0.5) + naive("saw", note * (1 + s / 2), pw * 0.5)
        m0, m2, amp_attack, f_attack, res_max, fx = 0.4, 0.4, 150, 400, 200, STRINGS
    elif name == "Acid":
        pulse = naive("saw", f0, (1 - pw) * 0.9)
        saw = naive("square", f0, pw * 0.9)
        m0, octaves, f_attack, f_sus, res_max = 0.4, 5, 2, 0.0, 450
        f_decay = 80 + dt_k * 900 / 1023
    elif name == "Pluck":
        extra = pluck_src(note, vel, pw, dt)
        pulse = np.zeros(N)
        m2, amp_attack, res_max, fx = 0.8, 1, 200, PLUCK
    elif name == "FM":
        ratios = [1, 2, 3, 4, 0.5, 3.5, 1.41, 7]
        r = ratios[min(7, dt_k * 8 // 1024)]
        menv = linear_env(1, 400, 0.25)
        index = 4 * np.pi * pw * FM_DEPTH * menv
        extra = FM_LEVEL * np.sin(2 * np.pi * note * t + index * np.sin(2 * np.pi * note * r * t))
        m2, res_max, fx = 0.8, 200, FM_L
    elif name == "Fold":
        pulse = naive("triangle", f0, 0.8)
        saw = naive("sine", detune_freq(note, dt_k), pw * 0.4)
        m0, m1 = 0.5, 0.5
        fbase, octaves, f_attack = 250, 6, 2
    elif name == "Chord":
        chords = [(4, 7, 12), (3, 7, 12), (2, 7, 12), (5, 7, 12), (4, 7, 11), (3, 7, 10), (4, 7, 10), (3, 7, 14)]
        c3 = chords[min(7, dt_k * 8 // 1024)]
        pulse = naive("saw", note, CHORD_LEVEL)
        saw = naive("saw", note * 2 ** (c3[0] / 12), CHORD_LEVEL)
        extra = naive("saw", note * 2 ** (c3[1] / 12), CHORD_LEVEL) + naive(
            "saw", note * 2 ** (c3[2] / 12), pw * CHORD_LEVEL)
        m0, m2, amp_attack, f_attack, res_max, fx = 0.4, 0.4, 20, 120, 200, ORCHID

    mix = np.clip(saw * m0 + pulse * m1 + extra * m2, -1, 1)
    if name == "Fold":
        amount = filt_k / 1023
        g = 1 + 7 * amount
        mix = fold(mix * 32768 / 20000 * g) * 16000 / 32768
    q = (70 + res_k * (res_max - 70) / 1023) / 100
    ctl = vel * linear_env(f_attack, f_decay, f_sus)
    y = svf_lowpass(mix, fbase, ctl, octaves, q)
    y = y * linear_env(amp_attack)
    if fx:
        y = lush(y, fx)
    return y


# A-weighting
def a_weight():
    f1, f2, f3, f4 = 20.598997, 107.65265, 737.86223, 12194.217
    nums = [(2 * np.pi * f4) ** 2 * (10 ** (1.9997 / 20)), 0, 0, 0, 0]
    dens = np.polymul([1, 4 * np.pi * f4, (2 * np.pi * f4) ** 2], [1, 4 * np.pi * f1, (2 * np.pi * f1) ** 2])
    dens = np.polymul(np.polymul(dens, [1, 2 * np.pi * f3]), [1, 2 * np.pi * f2])
    return signal.bilinear(nums, dens, SR)


AB, AA = a_weight()


def loudness(y):
    w = signal.lfilter(AB, AA, y)
    win = SR // 20
    rms = [np.sqrt(np.mean(w[i:i + win] ** 2)) for i in range(0, len(w) - win, win // 2)]
    return 20 * np.log10(max(rms) + 1e-12)


import sys
FM_LEVEL = float(sys.argv[1]) if len(sys.argv) > 1 else 0.38
CHORD_LEVEL = float(sys.argv[2]) if len(sys.argv) > 2 else 0.22
FM_DEPTH = float(sys.argv[3]) if len(sys.argv) > 3 else 0.6
ONLY = sys.argv[4].split(",") if len(sys.argv) > 4 else None

NOTES = [49, 58, 68]
KNOBS = [(512, 512), (256, 256), (768, 768)]
voices = ["Original", "Orchid", "Organ", "Strings", "Acid", "Pluck", "FM", "Fold", "Chord"]
results = {}
for v in (ONLY or voices):
    vals = [loudness(voice(v, n, d, p)) for n in NOTES for (d, p) in KNOBS]
    results[v] = np.mean(vals)
    print(f"{v:9s} {results[v]:7.1f} dB  (range {min(vals):.1f}..{max(vals):.1f})", flush=True)
ref = results["Original"]
print()
for v in results:
    print(f"{v:9s} {results[v] - ref:+5.1f} dB vs Original   trim x{10 ** ((ref - results[v]) / 20):.2f}")
