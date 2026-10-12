#!/usr/bin/env python3
"""Picks a few presets per category and renders a short musical phrase of each for the website.

    python3 Tools/site/demos.py <out dir>
Needs build-linux/.../PyraRenderTest (with --demo) and ffmpeg. Writes <out>/<id>.mp3 and <out>/demos.json.
"""
import json, os, subprocess, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "presetgen"))
from qa import browse_key

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
RT = os.path.join(ROOT, "build-linux", "PyraRenderTest_artefacts", "Release", "PyraRenderTest")
P = json.load(open(os.path.join(ROOT, "Tools", "params.json")))
BPM = 120.0
B = 60.0 / BPM          # one beat in seconds

# (category, sub-category, a preset name or how far into the sub-category's list: 0 = first realistic starter, 1 = most extreme)
PICKS = [
    ("Bass", "Synth Bass", .7), ("Bass", "Reese Bass", .6), ("Bass", "808", "Analog Bending 808"), ("Bass", "Acid Bass", .6), ("Bass", "Electric Bass", .2),
    ("Synth", "Hard Lead", "Inferno"), ("Synth", "Soft Lead", .5), ("Synth", "Pluck", .6), ("Synth", "Polysynth", .6), ("Synth", "Chords", "Massive Rhythm Chords"),
    ("Pad", "Huge Pad", .6), ("Pad", "Ambient Pad", .7), ("Pad", "Warm Pad", "Studio OB Pad"), ("Pad", "Dynamic Pad", .7),
    ("Keyboard", "Grand Piano", .1), ("Keyboard", "Electric Piano", "Studio Wurlitzer"),
    ("Organs", "Electric Organ", .3), ("Organs", "Church Organ", .2),
    ("Bells", "Bells", "Huge Chapel Bells"), ("Bells", "Marimba", .2),
    ("Guitar", "Acoustic Guitar", .15), ("Guitar", "Distorted Guitar", .4), ("Guitar", "Ethnic Guitar", .3),
    ("Strings", "Ensemble Strings", "Cinematic Violins"), ("Strings", "Orchestral Strings", .3),
    ("Brass", "Orchestral Brass", .2), ("Brass", "Synth Brass", "Huge OB Brass"), ("Brass", "Saxophone", .15),
    ("Winds", "Flute", .15), ("Winds", "Accordion", .2),
    ("Vox", "Ensemble Choir", .3), ("Vox", "Orchestral Choir", .3),
    ("Arps", "Complex Arpeggios", .6), ("Arps", "Pulsating", .6), ("Arps", "Phrase", .5),
    ("Drums", "Realistic Drums", .3), ("Drums", "Drum Loop", .4), ("Drums", "Percs Loop", .4),
    ("FX", "Risers", .5), ("FX", "Hit", .5), ("FX", "Drones", .5),
]

PROG = [(45, [57, 60, 64]), (41, [57, 60, 65]), (48, [55, 60, 64]), (43, [55, 59, 62])]   # Am F C G
MELODY = [(0, 1, 69), (1, .5, 72), (1.5, .5, 76), (2, 1.5, 74), (3.5, .5, 72),
          (4, 1, 72), (5, .5, 69), (5.5, .5, 72), (6, 2, 67),
          (8, 1, 67), (9, .5, 72), (9.5, .5, 76), (10, 1.5, 79), (11.5, .5, 77),
          (12, 1, 76), (13, .5, 74), (13.5, .5, 72), (14, 2, 71)]


def n(t, l, k, v=.85): return [round(t * B, 4), round(l * B, 4), int(k), v]


def choice(param, value):
    c = P[param].get("choices")
    return c[int(value)] if c else value


def phrase(pr):
    cat, sub, prm = pr["category"], pr["sub"], pr["params"]
    out = []
    if prm.get("chord_on"):
        if choice("chord_mode", prm.get("chord_mode", 0)) == "Progression": return [n(0, 15.5, 57)]
        for bar, (root, _) in enumerate(PROG):
            for t, l in ((0, 1.5), (1.5, 1), (2.5, 1.5)): out.append(n(bar * 4 + t, l, root + 12))
        return out
    if cat == "Drums":
        if sub == "Drum Loop": return [n(0, 16, 36)]
        if sub == "Percs Loop": return [n(0, 16, 48)]
        for bar in range(4):
            b = bar * 4
            for t in (0, 1.5, 2.5): out.append(n(b + t, .25, 36, .95))
            for t in (1, 3): out.append(n(b + t, .25, 38, .9))
            for k in range(8): out.append(n(b + k * .5, .2, 42, .55 + .3 * (k % 2 == 0)))
        out.append(n(0, .5, 49, .8))
        return out
    if cat == "FX":
        if sub == "Hit": return [n(0, 2, 48, 1.0), n(8, 2, 48, 1.0)]
        if sub == "Drones": return [n(0, 15.5, 45), n(0, 15.5, 52)]
        return [n(0, 15.5, 60)]
    if cat == "Bass":
        if sub == "808":
            for bar, (root, _) in enumerate(PROG):
                out += [n(bar * 4, 2.5, root - 12, .95), n(bar * 4 + 2.75, 1, root - 12 + (7 if bar % 2 else 12), .85)]
            return out
        for bar, (root, _) in enumerate(PROG):
            r = root - 12
            for k, (t, iv) in enumerate([(0, 0), (.5, 0), (1, 12), (1.5, 0), (2, 0), (2.5, 7), (3, 12), (3.5, 10)]):
                out.append(n(bar * 4 + t, .45, r + iv, .95 if k % 2 == 0 else .75))
        return out
    if cat == "Arps":
        for bar, (root, ch) in enumerate(PROG):
            out += [n(bar * 4, 3.95, k) for k in [root] + ch]
        return out
    solo = sub in ("Hard Lead", "Soft Lead", "Saxophone", "Flute", "Ethnic Guitar") or cat == "Winds" and sub != "Accordion"
    if solo:
        shift = {"Saxophone": -5, "Ethnic Guitar": -12}.get(sub, 0)
        return [n(t, l * .95, k + shift) for t, l, k in MELODY]
    if cat == "Bells" or sub == "Pluck":
        up = 12 if cat == "Bells" and sub != "Marimba" else 0
        for bar, (root, ch) in enumerate(PROG):
            tones = ch + [ch[0] + 12, ch[1] + 12]
            for k in range(8): out.append(n(bar * 4 + k * .5, .45, tones[[0, 1, 2, 3, 4, 3, 2, 1][k]] + up, .9 if k % 2 == 0 else .7))
        return out
    if cat in ("Pad", "Strings", "Vox") or sub in ("Church Organ", "Orchestral Brass", "Synth Brass"):
        for bar, (root, ch) in enumerate(PROG):
            out += [n(bar * 4, 4.0, k, .8) for k in [root] + ch]
        return out
    # keys, organs, guitars, polysynth, accordion: rhythmic chords with the bass note
    for bar, (root, ch) in enumerate(PROG):
        out.append(n(bar * 4, 3.9, root, .8))
        for t, l in ((0, 1.4), (1.5, .9), (2.5, 1.4)): out += [n(bar * 4 + t, l, k, .85 if t == 0 else .7) for k in ch]
    return out


def main(outdir):
    os.makedirs(outdir, exist_ok=True)
    bank = json.load(open(os.path.join(ROOT, "Resources", "FactoryPresets.json")))
    jobs, meta = [], []
    for cat, sub, where in PICKS:
        cands = [p for p in bank if p["category"] == cat and p["sub"] == sub and "Lo-Fi" not in p.get("tags", [])]
        cands.sort(key=browse_key)
        if isinstance(where, str): pr = next(p for p in bank if p["category"] == cat and p["sub"] == sub and p["name"] == where)
        else: pr = cands[min(len(cands) - 1, int(where * len(cands)))]
        did = f"{cat}-{sub}".lower().replace(" ", "-")
        secs = 10.5 if cat != "Drums" else 8.6
        pr = json.loads(json.dumps(pr)); pr["params"]["master"] = float(pr["params"].get("master", -4)) - 10.0   # headroom; loudness is set afterwards
        jobs.append({"name": did, "preset": pr, "bpm": BPM, "seconds": secs, "notes": phrase(pr)})
        meta.append({"id": did, "category": cat, "sub": sub, "name": pr["name"], "tags": pr.get("tags", []),
                     "sampled": bool(pr["params"].get("smp_on"))})
    work = os.path.join(outdir, "wav"); os.makedirs(work, exist_ok=True)
    jf = os.path.join(work, "jobs.json"); json.dump(jobs, open(jf, "w"))
    subprocess.run([RT, "--demo", jf, work], check=True)
    for m in meta:
        src = os.path.join(work, m["id"] + ".wav"); dst = os.path.join(outdir, m["id"] + ".mp3")
        # same loudness for every demo, short fade at the end
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", src, "-af",
                        "loudnorm=I=-16:TP=-1.5:LRA=11,afade=t=out:st=%.2f:d=1.2" % ((10.5 if m["category"] != "Drums" else 8.6) - 1.2),
                        "-ar", "44100", "-c:a", "libmp3lame", "-b:a", "160k", dst], check=True)
    json.dump(meta, open(os.path.join(outdir, "demos.json"), "w"), indent=1, ensure_ascii=False)
    print(len(meta), "demos")


if __name__ == "__main__":
    main(sys.argv[1])
