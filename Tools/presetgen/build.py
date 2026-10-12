#!/usr/bin/env python3
"""Builds the PYRA v0.9 factory bank: 2000 new presets + the earlier ones moved into the new categories.

    python3 Tools/presetgen/build.py <old FactoryPresets.json> <out raw json>
"""
import json, hashlib, random, sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from core import P, Namer, apply_character, CHARACTERS
from registry import ARCH, ROLE, CHARS
import r_bass, r_synth, r_pad, r_keys, r_guitar, r_orch, r_arps, r_drums, r_fx   # noqa: F401 (they register the recipes)

# the user's categories, in browser order, with how many new presets each sub-category gets
LAYOUT = [
    ("Bass", [("Synth Bass", 40), ("Electric Bass", 30), ("Acoustic Bass", 22), ("Reese Bass", 30), ("Hard Bass", 28), ("Soft Bass", 25), ("Sub Bass", 25), ("Acid Bass", 28), ("808", 32)]),
    ("Synth", [("Soft Lead", 50), ("Hard Lead", 55), ("Pluck", 60), ("Polysynth", 60), ("Synth Keys", 50), ("Chords", 55)]),
    ("Pad", [("Soft Pad", 35), ("Huge Pad", 40), ("Bright Pad", 35), ("Ambient Pad", 40), ("Warm Pad", 35), ("Dynamic Pad", 35)]),
    ("Keyboard", [("Acoustic Piano", 30), ("Grand Piano", 30), ("Electric Piano", 30)]),
    ("Organs", [("Electric Organ", 25), ("Pipe Organ", 15), ("Reed Organ", 15), ("Church Organ", 15)]),
    ("Bells", [("Bells", 20), ("Tubular Bells", 12), ("Church Bells", 12), ("Celesta", 10), ("Xylophone", 12), ("Marimba", 14), ("Steel Pan", 12), ("Mallet", 16), ("Chimes", 12)]),
    ("Guitar", [("Acoustic Guitar", 20), ("Electric Guitar", 22), ("Distorted Guitar", 20), ("Harps", 14), ("Clav", 12), ("Harpsichord", 12), ("Ethnic Guitar", 20)]),
    ("Strings", [("Solo Strings", 22), ("Ensemble Strings", 22), ("Orchestral Strings", 22), ("Solo Cello", 18), ("Ensemble Cello", 16)]),
    ("Brass", [("Solo Brass", 12), ("Ensemble Brass", 14), ("Orchestral Brass", 12), ("Synth Brass", 18), ("Trombone", 10), ("Saxophone", 14), ("Horn", 10), ("Trumpet", 12), ("Tuba", 8)]),
    ("Winds", [("Assorted Winds", 12), ("Flute", 14), ("Recorder", 10), ("Clarinet", 10), ("Oboe", 10), ("Bassoon", 8), ("Accordion", 14), ("Harmonica", 12)]),
    ("Vox", [("Solo Vox", 18), ("Ensemble Choir", 20), ("Orchestral Choir", 16), ("Scat", 16)]),
    ("Arps", [("Simple Arpeggios", 35), ("Complex Arpeggios", 35), ("Phrase", 35), ("Accent", 30), ("Pulsating", 35)]),
    ("Drums", [("Realistic Drums", 20), ("Synth Drums", 25), ("Ethnic Drums", 15), ("Orchestral Drums", 12), ("Percussion", 16), ("Tonal Drums", 14), ("Drum Loop", 16), ("Percs Loop", 12)]),
    ("FX", [("Sound FX", 18), ("Texture", 20), ("Drones", 20), ("Risers", 18), ("Downlifter", 14), ("Hit", 16), ("Foley", 14)]),
]

# where the earlier presets go: (old category, old sub) -> (macro, sub); None = decided by the sampled instrument
OLD_MAP = {
    ("Arp", "Bass Arp"): ("Arps", "Pulsating"), ("Arp", "Classic"): ("Arps", "Simple Arpeggios"), ("Arp", "Melodic"): ("Arps", "Complex Arpeggios"),
    ("Sequence", "Gated"): ("Arps", "Pulsating"), ("Sequence", "Step Sequence"): ("Arps", "Phrase"),
    ("Bass", "808"): ("Bass", "808"), ("Bass", "Acid"): ("Bass", "Acid Bass"), ("Bass", "FM"): ("Bass", "Synth Bass"), ("Bass", "Growl"): ("Bass", "Hard Bass"),
    ("Bass", "Pluck"): ("Bass", "Synth Bass"), ("Bass", "Reese"): ("Bass", "Reese Bass"), ("Bass", "Sub"): ("Bass", "Sub Bass"), ("Bass", "Wobble"): ("Bass", "Hard Bass"),
    ("Brass & Winds", "Brass Section"): ("Brass", "Synth Brass"), ("Brass & Winds", "Flute"): ("Winds", "Flute"), ("Brass & Winds", "Reed"): ("Winds", "Assorted Winds"),
    ("Brass & Winds", "Synth Brass"): ("Brass", "Synth Brass"), ("Brass & Winds", "Sampled Brass & Winds"): None,
    ("Chords", "One Finger"): ("Synth", "Chords"), ("Chords", "Stab"): ("Synth", "Chords"),
    ("Drums", "Hats & Cymbals"): ("Drums", "Synth Drums"), ("Drums", "Kick"): ("Drums", "Synth Drums"), ("Drums", "Perc & Toms"): ("Drums", "Percussion"),
    ("Drums", "Snare & Clap"): ("Drums", "Synth Drums"),
    ("FX", "Downlifter"): ("FX", "Downlifter"), ("FX", "Impact"): ("FX", "Hit"), ("FX", "Noise & Wind"): ("FX", "Texture"), ("FX", "Riser"): ("FX", "Risers"), ("FX", "Sci-Fi"): ("FX", "Sound FX"),
    ("Guitar", "Clean"): ("Guitar", "Electric Guitar"), ("Guitar", "Distorted"): ("Guitar", "Distorted Guitar"), ("Guitar", "Muted"): ("Guitar", "Electric Guitar"), ("Guitar", "Nylon"): ("Guitar", "Acoustic Guitar"),
    ("Keys", "Bells"): ("Bells", "Bells"), ("Keys", "Clav"): ("Guitar", "Clav"), ("Keys", "Electric Piano"): ("Keyboard", "Electric Piano"), ("Keys", "Mallets"): ("Bells", "Mallet"),
    ("Keys", "Organ"): ("Organs", "Electric Organ"), ("Keys", "Piano"): ("Keyboard", "Acoustic Piano"), ("Keys", "Sampled Mallets"): None, ("Keys", "Sampled Piano"): None,
    ("Lead", "Chip"): ("Synth", "Soft Lead"), ("Lead", "Mono"): ("Synth", "Hard Lead"), ("Lead", "Pluck Lead"): ("Synth", "Pluck"), ("Lead", "Screamer"): ("Synth", "Hard Lead"),
    ("Lead", "Soft"): ("Synth", "Soft Lead"), ("Lead", "Supersaw"): ("Synth", "Hard Lead"), ("Lead", "Sync"): ("Synth", "Hard Lead"), ("Lead", "Vocal"): ("Synth", "Soft Lead"),
    ("Pad", "Ambient"): ("Pad", "Ambient Pad"), ("Pad", "Choir"): ("Vox", "Ensemble Choir"), ("Pad", "Dark"): ("Pad", "Warm Pad"), ("Pad", "Evolving"): ("Pad", "Dynamic Pad"),
    ("Pad", "Glass"): ("Pad", "Bright Pad"), ("Pad", "Hybrid"): ("Pad", "Huge Pad"), ("Pad", "Warm"): ("Pad", "Warm Pad"),
    ("Pluck", "Bell Pluck"): ("Synth", "Pluck"), ("Pluck", "Dreamy"): ("Synth", "Pluck"), ("Pluck", "Harp & Koto"): ("Guitar", "Harps"), ("Pluck", "Stab"): ("Synth", "Pluck"),
    ("Pluck", "Synth Pluck"): ("Synth", "Pluck"),
    ("Strings", "Cinematic"): ("Strings", "Orchestral Strings"), ("Strings", "Ensemble"): ("Strings", "Ensemble Strings"), ("Strings", "Pizzicato"): ("Strings", "Ensemble Strings"),
    ("Strings", "Sampled Strings"): None, ("Strings", "Solo"): ("Strings", "Solo Strings"),
}
BY_SAMPLE = {1: ("Keyboard", "Grand Piano"), 2: ("Keyboard", "Acoustic Piano"), 3: ("Strings", "Ensemble Strings"), 4: ("Strings", "Ensemble Strings"),
             5: ("Strings", "Ensemble Cello"), 6: ("Strings", "Orchestral Strings"), 7: ("Strings", "Solo Strings"), 8: ("Strings", "Ensemble Strings"),
             9: ("Guitar", "Harps"), 10: ("Brass", "Trumpet"), 11: ("Brass", "Horn"), 12: ("Brass", "Trombone"), 13: ("Brass", "Tuba"),
             14: ("Winds", "Flute"), 15: ("Winds", "Clarinet"), 16: ("Winds", "Oboe"), 17: ("Winds", "Bassoon"), 18: ("Bells", "Marimba"), 19: ("Bells", "Bells")}


def seed_of(*parts):
    return int(hashlib.md5("/".join(map(str, parts)).encode()).hexdigest()[:8], 16)


def remap_old(old, namer):
    out = []
    for p in old:
        key = (p["category"], p.get("sub", ""))
        dest = OLD_MAP.get(key)
        if dest is None:
            prm = p.get("params", {})
            dest = BY_SAMPLE.get(int(prm.get("smp_inst", 0)) if prm.get("smp_on") else 0, ("Synth", "Polysynth"))
        q = dict(p); q["category"], q["sub"] = dest
        namer.used.add(q["name"].lower())
        out.append(q)
    return out


def build_new(namer, salt=""):
    bank = []
    for macro, subs in LAYOUT:
        for sub, count in subs:
            arches = ARCH.get((macro, sub))
            if not arches: raise SystemExit(f"no recipe for {macro} / {sub}")
            role = ROLE.get((macro, sub), "lead")
            chars = CHARS.get((macro, sub), CHARACTERS)
            for i in range(count):
                a = arches[i % len(arches)]
                k = i // len(arches)                       # how many times this archetype was used already
                char = chars[(k * 3 + arches.index(a)) % len(chars)]
                r = random.Random(seed_of(macro, sub, i, salt))
                p = P("", macro, sub)
                a["build"](p, r, k)
                apply_character(p, r, char, role)
                p.name = namer.make(r, a["nouns"], char, a["extra"])
                bank.append(p.to_json(f"factory:v9/{macro.lower()}/{sub.lower().replace(' ', '-')}/{i + 1:02d}"))
    return bank


if __name__ == "__main__":
    old = json.load(open(sys.argv[1]))
    namer = Namer()
    old2 = remap_old(old, namer)
    new = build_new(namer)
    order = {m: n for n, (m, _) in enumerate(LAYOUT)}
    subs = {(m, s): n for m, ss in LAYOUT for n, (s, _) in enumerate(ss)}
    full = sorted(new + old2, key=lambda p: (order.get(p["category"], 99), subs.get((p["category"], p["sub"]), 99)))
    json.dump(full, open(sys.argv[2], "w"), indent=0)
    print(f"{len(new)} new + {len(old2)} earlier = {len(full)} presets")
