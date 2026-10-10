#!/usr/bin/env python3
"""Builds PYRA's sample instruments from free libraries.

Sources (both allow free redistribution with credit):
  * Salamander Grand Piano by Alexander Holm - CC-BY 3.0
  * VS Chamber Orchestra: Community Edition (VSCO 2 CE) by Versilian Studios / Sam Gossner,
    Ivy Audio / Simon Dalzell - "use for any purpose, credit appreciated, do not sell the samples directly"

For each instrument: picks one round-robin per note and 1-3 velocity layers, resamples to 44.1 kHz,
trims silence, checks/corrects tuning, levels the notes, and either fades out decaying sounds or bakes
a seamless crossfade loop into sustained ones. Writes 16-bit WAVs plus instrument.json; the WAVs are
then encoded to Ogg Vorbis by PyraRenderTest --convert.

    python3 Tools/make_samples.py <vsco_dir> <piano_wav_dir> <out_stage_dir>
"""
import json, math, os, re, sys, glob, warnings
import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly

warnings.filterwarnings("ignore")
SR = 44100
NOTE = {"C": 0, "C#": 1, "D": 2, "D#": 3, "E": 4, "F": 5, "F#": 6, "G": 7, "G#": 8, "A": 9, "A#": 10, "B": 11}
DYN = {"pp": 1, "p": 2, "mp": 3, "soft": 2, "mf": 4, "medium": 4, "f": 5, "loud": 6, "ff": 6}


def midi(name):
    m = re.match(r"([A-G]#?)(-?\d)$", name)
    return 12 * (int(m.group(2)) + 1) + NOTE[m.group(1)]


# id, display name, category, folder (relative), sustained?, max seconds, max velocity layers, attack, release
INSTRUMENTS = [
    ("grand_piano", "Grand Piano", "Keys", None, False, 9.0, 3, 0.0, 0.5),
    ("upright_piano", "Upright Piano", "Keys", "Keys/Upright Nr1", False, 7.0, 3, 0.0, 0.4),
    ("violins", "Violins", "Strings", "Strings/Violin Section/susVib", True, 6.0, 2, 0.08, 0.5),
    ("violas", "Violas", "Strings", "Strings/Viola Section/susvib", True, 6.0, 2, 0.08, 0.5),
    ("cellos", "Cellos", "Strings", "Strings/Cello Section/susvib", True, 6.0, 2, 0.08, 0.5),
    ("contrabass", "Contrabass", "Strings", "Strings/Solo Contrabass/SusVib", True, 6.0, 2, 0.06, 0.4),
    ("solo_violin", "Solo Violin", "Strings", "Strings/Solo Violin/Arco Vib", True, 6.0, 2, 0.05, 0.35),
    ("pizzicato", "Pizzicato Strings", "Strings", "Strings/Violin Section/Pizz", False, 1.6, 2, 0.0, 0.2),
    ("harp", "Harp", "Strings", "Strings/Harp", False, 5.0, 1, 0.0, 0.6),
    ("trumpet", "Trumpet", "Brass & Winds", "Brass/Trumpet/sus", True, 5.0, 2, 0.03, 0.25),
    ("french_horn", "French Horn", "Brass & Winds", "Brass/F Horn/sus", True, 5.0, 2, 0.05, 0.3),
    ("trombone", "Trombone", "Brass & Winds", "Brass/Tenor Trombone/sus", True, 5.0, 2, 0.04, 0.3),
    ("tuba", "Tuba", "Brass & Winds", "Brass/Tuba/sus", True, 5.0, 2, 0.05, 0.3),
    ("flute", "Flute", "Brass & Winds", "Woodwinds/Flute/susvib", True, 5.0, 1, 0.04, 0.25),
    ("clarinet", "Clarinet", "Brass & Winds", "Woodwinds/Clarinet/susLong", True, 5.0, 2, 0.03, 0.2),
    ("oboe", "Oboe", "Brass & Winds", "Woodwinds/Oboe/Vib", True, 5.0, 2, 0.03, 0.2),
    ("bassoon", "Bassoon", "Brass & Winds", "Woodwinds/Bassoon/sus", True, 5.0, 2, 0.03, 0.2),
    ("marimba", "Marimba", "Keys", "Percussion/Marimba", False, 2.5, 1, 0.0, 0.3),
    ("glockenspiel", "Glockenspiel", "Keys", "Percussion/Glock", False, 4.0, 1, 0.0, 0.5),
]


def parse(fname):
    """-> (midi note, velocity rank, round-robin index) from a sample file name, or None."""
    base = os.path.splitext(os.path.basename(fname))[0]
    m = re.match(r"^([A-G]#?\d)v(\d+)$", base)                     # Salamander: A0v10
    if m: return midi(m.group(1)), int(m.group(2)), 1
    toks = base.split("_")
    note = None
    for t in toks:
        if re.match(r"^[A-G]#?\d$", t): note = midi(t)
    if note is None: return None
    vel = 1; rr = 1
    for t in toks:
        tl = t.lower()
        mv = re.match(r"^v(\d)$", tl)
        if mv: vel = int(mv.group(1))
        elif tl in DYN: vel = DYN[tl]
        mr = re.match(r"^rr(\d)$", tl)
        if mr: rr = int(mr.group(1))
        elif re.match(r"^\d{1,2}$", t): rr = int(t)
    return note, vel, rr


def load(path):
    sr, x = wavfile.read(path)
    if x.dtype == np.int16: x = x.astype(np.float32) / 32768.0
    elif x.dtype == np.int32: x = x.astype(np.float32) / 2147483648.0
    else: x = x.astype(np.float32)
    if x.ndim == 1: x = np.stack([x, x], 1)
    x = x[:, :2]
    if sr != SR:
        g = math.gcd(SR, sr)
        x = resample_poly(x, SR // g, sr // g, axis=0).astype(np.float32)
    return x


def envelope(x, win=0.01):
    w = max(1, int(SR * win))
    m = np.abs(x).max(1)
    return np.sqrt(np.convolve(m ** 2, np.ones(w) / w, "same"))


def estimate_cents(x, expected):
    """Tuning error (cents) against the expected note, octave-folded (YIN pitch detection), or None if unclear."""
    mono = x.mean(1)
    start = int(0.3 * SR)
    seg = mono[start:start + int(0.25 * SR)]
    if len(seg) < int(0.1 * SR):
        seg = mono[:int(0.25 * SR)]
    W = len(seg) // 2
    taumax, taumin = min(int(SR / 30), W - 1), int(SR / 4000)
    if W < 256 or taumax <= taumin + 2: return None
    d = np.array([np.sum((seg[:W] - seg[t:t + W]) ** 2) for t in range(taumax)])
    cm = d.copy()
    cm[1:] = d[1:] * np.arange(1, taumax) / np.maximum(np.cumsum(d[1:]), 1e-12)
    cm[0] = 1
    tau = None
    for t in range(max(taumin, 1), taumax - 1):
        if cm[t] < 0.15 and cm[t] <= cm[t + 1]:
            a, b, c = cm[t - 1], cm[t], cm[t + 1]
            den = a - 2 * b + c
            tau = t + (0.5 * (a - c) / den if abs(den) > 1e-12 else 0.0)
            break
    if tau is None: return None
    note = 69 + 12 * math.log2(SR / tau / 440)
    diff = (note - expected + 6) % 12 - 6
    return 100 * diff

def bake_loop(x, sustained_from, end):
    """Finds a good loop in [sustained_from, end) and crossfades it. Returns (data, loopStart, loopEnd)."""
    mono = x.mean(1)
    best = None
    win = 1024
    for L in (int(SR * 0.9), int(SR * 0.6), int(SR * 0.4), int(SR * 0.25), int(SR * 0.15)):
        if end - sustained_from < L + win * 2: continue
        e_cands = range(end - win, max(sustained_from + L + win, end - int(0.6 * SR)), -97)
        for e in e_cands:
            s = e - L
            a = mono[s - win:s]; b = mono[e - win:e]
            na, nb = np.linalg.norm(a), np.linalg.norm(b)
            if na < 1e-6 or nb < 1e-6: continue
            c = float(np.dot(a, b) / (na * nb))
            if best is None or c > best[0]: best = (c, s, e)
        if best and best[0] > 0.97: break
    if best is None:
        return None
    _, s, e = best
    X = min(int(0.12 * SR), (e - s) // 3)
    y = x[:e].copy()
    t = np.linspace(0, 1, X, dtype=np.float32)[:, None]
    fade_out = np.cos(t * np.pi / 2); fade_in = np.sin(t * np.pi / 2)
    # make the end of the loop flow into its start
    y[e - X:e] = x[e - X:e] * fade_out + x[s - X:s] * fade_in
    return y, s, e


def process(inst, files, out_dir, report):
    iid, name, cat, folder, sustained, max_s, max_layers, att, rel = inst
    by = {}
    for f in files:
        p = parse(f)
        if p is None: continue
        note, vel, rr = p
        by.setdefault((note, vel), []).append((rr, f))
    count = {}
    for (_, v) in by: count[v] = count.get(v, 0) + 1
    vels = sorted(v for v in count if count[v] >= 0.5 * max(count.values()))   # layers that cover most notes
    if len(vels) > max_layers:
        idx = np.round(np.linspace(0, len(vels) - 1, max_layers)).astype(int)
        vels = [vels[i] for i in idx]
    notes = sorted({n for (n, v) in by if v in vels})

    zones, layer_rms = [], {v: [] for v in vels}
    os.makedirs(out_dir, exist_ok=True)
    for note in notes:
        for v in vels:
            if (note, v) not in by:
                continue
            f = sorted(by[(note, v)])[0][1]
            x = load(f)
            env = envelope(x)
            pk = env.max()
            if pk < 1e-4: continue
            on = np.argmax(env > pk * 0.003)
            x = x[max(0, on - int(0.002 * SR)):]
            env = envelope(x)
            cents = estimate_cents(x, note)
            tune = 0.0
            if cents is not None and abs(cents) <= 115: tune = -round(cents, 1)   # also fixes notes labelled a semitone off
            elif cents is not None: report.append(f"  ? {iid} {note} v{v}: pitch off by {cents:.0f} cents (not corrected)")

            if sustained:
                tail = len(x) - int(0.25 * SR)            # skip the recorded release
                loud = np.where(env > env.max() * 0.15)[0]   # and anything after the note has faded
                if len(loud): tail = min(tail, loud[-1] - int(0.05 * SR))
                end = min(tail, int(max_s * SR))
                looped = bake_loop(x, min(int(0.45 * SR), int(len(x) * 0.3)), end)
                if looped is None:
                    report.append(f"  ! {iid} {note} v{v}: too short to loop")
                    y, ls, le = x[:end], 0, 0
                else:
                    y, ls, le = looped
            else:
                below = np.where(envelope(x, 0.05) > env.max() * 0.001)[0]
                end = min(int(max_s * SR), (below[-1] if len(below) else len(x)) + 1)
                y = x[:end].copy()
                fl = min(int(0.4 * SR), max(1, len(y) // 6))
                y[-fl:] *= np.linspace(1, 0, fl, dtype=np.float32)[:, None] ** 2
                ls = le = 0

            rms = float(np.sqrt(np.mean(y[: min(len(y), int(0.6 * SR))] ** 2)) + 1e-9)
            layer_rms[v].append(rms)
            fn = f"{note}_{v}.wav"
            zones.append({"file": fn.replace(".wav", ".ogg"), "root": note, "vel": v, "tune": tune,
                          "loopStart": int(ls), "loopEnd": int(le), "rms": rms, "_data": y})

    if not zones: return None
    # level: every note at the same loudness within its layer, softer layers a bit quieter
    loud = max(np.median(layer_rms[v]) for v in vels if layer_rms[v])
    layer_gain = {v: float(np.clip((np.median(layer_rms[v]) / loud) ** 0.5, 0.35, 1.0)) if layer_rms[v] else 1.0 for v in vels}
    target = 0.12
    for z in zones:
        g = target / z["rms"] * layer_gain[z["vel"]]
        y = z.pop("_data") * g
        pk = np.abs(y).max()
        if pk > 0.98: y *= 0.98 / pk
        wavfile.write(os.path.join(out_dir, z["file"].replace(".ogg", ".wav")), SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
        del z["rms"]

    # key and velocity ranges
    roots = sorted({z["root"] for z in zones})
    key_range = {}
    for i, r in enumerate(roots):
        lo = roots[i - 1] + (r - roots[i - 1]) // 2 + 1 if i > 0 else max(0, r - 12)
        hi = r + (roots[i + 1] - r) // 2 if i + 1 < len(roots) else min(127, r + 12)
        key_range[r] = (lo, hi)
    vel_edges = {v: (int(round(127 * k / len(vels))) + (1 if k else 0), int(round(127 * (k + 1) / len(vels))))
                 for k, v in enumerate(vels)}
    for z in zones:
        z["lo"], z["hi"] = key_range[z["root"]]
        z["velLo"], z["velHi"] = vel_edges[z["vel"]]
        del z["vel"]

    meta = {"id": iid, "name": name, "category": cat, "sampleRate": SR, "sustained": sustained,
            "attack": att, "release": rel, "zones": sorted(zones, key=lambda z: (z["root"], z["velLo"]))}
    with open(os.path.join(out_dir, "instrument.json"), "w") as fp:
        json.dump(meta, fp, indent=1)
    report.append(f"{iid:14s} {len(roots):2d} notes x {len(vels)} layers  range {roots[0]}-{roots[-1]}")
    return meta


def main(vsco, piano, out):
    report = []
    for inst in INSTRUMENTS:
        if inst[3] is None:
            files = sorted(glob.glob(os.path.join(piano, "*.wav")))
        else:
            files = sorted(glob.glob(os.path.join(vsco, inst[3], "*.wav")))
        process(inst, files, os.path.join(out, inst[0]), report)
    print("\n".join(report))


if __name__ == "__main__":
    main(*sys.argv[1:4])
