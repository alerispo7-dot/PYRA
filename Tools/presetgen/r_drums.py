"""DRUMS: realistic, synth, ethnic, orchestral, percussion, tonal, drum loops, percussion loops.

Sampled kits put one sound on each key (General MIDI layout from C1 = MIDI 36: kick, snare D1, hats F#1 / A#1...).
Loops are sequencer patterns that play the kit: hold C1."""
from core import *
from registry import arch

D = "Drums"
KITS = ["Studio Drum Kit", "Vintage Drum Kit", "Jazz Drum Kit"]
K, RIM, SN, CLAP, SN2, LT, CH, PH, MT, OH, HT, CR, RD = 0, 1, 2, 3, 4, 5, 6, 8, 9, 10, 11, 13, 14


def kit(p, r, inst):
    p.set(polyphony=24, velAmp=0.8)
    p.sample(inst, level=0.9, vel=0.9)
    p.filt(0, on=0)
    p.env(0, att=0.0, dec=3.0, sus=1.0, rel=0.4)


def drum_bus(p, r, punch=True):
    if punch: p.fx("comp", thresh=rr(r, -24, -14, 1), ratio=rr(r, 3, 6, 1), att=rr(r, 5, 25, 1), rel=rr(r, 60, 180, 0), makeup=rr(r, 2, 5, 1))
    if r.random() < 0.5: p.fx("eq", lowf=rr(r, 60, 90, 0), lowg=rr(r, 1, 4, 1), highf=rr(r, 7000, 11000, 0), highg=rr(r, 0, 3, 1))


# ------------------------------------------------------------------------------------------- Realistic Drums
@arch(D, "Realistic Drums", ["Studio Kit", "Rock Kit", "Pop Kit", "Session Kit", "Live Kit"], role="perc", chars=["clean", "vintage", "dark", "bright", "lofi", "wide", "fx", "clean"])
def _(p, r, i):
    kit(p, r, KITS[i % 3])
    drum_bus(p, r)
    reverb(p, r, "Room", mix=(0.06, 0.16), size=(0.2, 0.5), decay=(0.5, 1.2))
    macros(p, tone=False)
    p.add_tags("Kit")


@arch(D, "Realistic Drums", ["Big Room Kit", "Arena Drums", "Gated Kit", "80s Gated Drums", "Stadium Kit"], role="perc", chars=["clean", "bright", "vintage", "wide"])
def _(p, r, i):
    kit(p, r, pick(r, KITS))
    p.fx("comp", thresh=-24, ratio=8, att=10, rel=80, makeup=6)
    reverb(p, r, "Hall", mix=(0.18, 0.3), size=(0.6, 0.9), decay=(1.2, 2.5))
    macros(p, tone=False)
    p.add_tags("Kit")


# ------------------------------------------------------------------------------------------- Synth Drums
@arch(D, "Synth Drums", ["Drum Machine", "Retro Machine", "Analog Machine", "Beatbox", "Rhythm Box"], role="perc", chars=["clean", "vintage", "lofi", "digital", "fx", "dark"])
def _(p, r, i):
    kit(p, r, "Drum Machine")
    drum_bus(p, r)
    macros(p, tone=False)
    p.add_tags("Kit")


@arch(D, "Synth Drums", ["Synth Kick", "808 Kick", "909 Kick", "Techno Kick", "Boom Kick", "Punch Kick"], role="perc", chars=["clean", "digital", "dark", "lofi", "vintage"])
def _(p, r, i):
    p.osc(0, wt="Sine", level=0.95)
    p.env(1, att=0.0, dec=rr(r, 0.03, 0.09, 3), sus=0.0)
    p.mod("Env 2", "Osc A Pitch", rr(r, 0.4, 0.8, 2))
    p.noise(type="White", level=rr(r, 0.1, 0.3, 2), route="Direct")
    p.env(2, att=0.0, dec=0.008, sus=0.0); p.set(noise_level=0.0); p.mod("Env 3", "Noise Level", rr(r, 0.2, 0.4, 2))
    p.filt(0, on=0)
    p.env(0, att=0.0, dec=rr(r, 0.25, 0.8, 2), sus=0.0, rel=0.05)
    p.set(transpose=-24)
    p.fx("dist", type=pick(r, ["Soft Clip", "Tube", "Hard Clip"]), drive=rr(r, 0.1, 0.5, 2), mix=1.0)
    macros(p, tone=False)


@arch(D, "Synth Drums", ["Synth Snare", "Noise Snare", "909 Snare", "Electro Snare", "Trap Snare"], role="perc", chars=["clean", "digital", "dark", "lofi", "fx"])
def _(p, r, i):
    p.osc(0, wt="Triangle", level=0.5)
    p.env(1, dec=0.03, sus=0.0); p.mod("Env 2", "Osc A Pitch", 0.15)
    p.noise(type=pick(r, ["White", "Pink", "Digital"]), level=0.6, route="Filter 1")
    p.filt(0, type=pick(r, ["HP 12", "BP 12"]), cutoff=rr(r, 800, 2500, 0), res=0.1)
    p.env(0, att=0.0, dec=rr(r, 0.15, 0.35, 2), sus=0.0, rel=0.08)
    p.fx("comp", thresh=-18, ratio=4, makeup=3)
    reverb(p, r, "Room", mix=(0.08, 0.18))
    macros(p)


@arch(D, "Synth Drums", ["Synth Hats", "Metal Hat", "808 Hat", "Tick Hat", "Open Hat"], role="perc", chars=["clean", "digital", "lofi", "bright"])
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Ring Mod", "FM Metal", "Square"]), pos=0.6, level=0.3, oct=3)
    p.noise(type=pick(r, ["White", "Blue", "Violet"]), level=0.6, route="Filter 1")
    p.filt(0, type="HP 24", cutoff=rr(r, 6000, 10000, 0), res=0.1)
    p.env(0, att=0.0, dec=rr(r, 0.04, 0.4, 2), sus=0.0, rel=0.03)
    macros(p)


@arch(D, "Synth Drums", ["Synth Clap", "Electro Clap", "Snap Clap", "Big Clap", "Room Clap"], role="perc", chars=["clean", "digital", "vintage", "wide"])
def _(p, r, i):
    p.osc_off(0)
    p.noise(type="White", level=0.8, route="Filter 1")
    p.filt(0, type="BP 12", cutoff=rr(r, 1000, 1800, 0), res=0.25)
    p.lfo(0, shape="Saw Down", rate=rr(r, 30, 45, 1), mode="Retrig"); p.mod("LFO 1", "Amp", 0.4)
    p.env(0, att=0.0, dec=rr(r, 0.15, 0.3, 2), sus=0.0, rel=0.1)
    reverb(p, r, "Room", mix=(0.12, 0.25))
    macros(p)


# ------------------------------------------------------------------------------------------- Ethnic / Orchestral / Percussion
@arch(D, "Ethnic Drums", ["World Percussion", "Hand Drums", "Global Kit", "Cajon & Djembe", "Latin Kit"], role="perc", chars=["clean", "wide", "dark", "vintage", "fx", "bright"])
def _(p, r, i):
    kit(p, r, pick(r, ["World Percussion", "Hand Percussion"]))
    drum_bus(p, r, punch=r.random() < 0.5)
    reverb(p, r, "Room", mix=(0.08, 0.18))
    macros(p, tone=False); p.add_tags("Kit")


@arch(D, "Ethnic Drums", ["Taiko", "War Drums", "Tribal Drums", "Thunder Drums", "Ritual Drum"], role="perc", chars=["clean", "dark", "wide", "fx"])
def _(p, r, i):
    p.osc(0, wt="Sine", level=0.9, oct=-1)
    p.noise(type="Brown", level=0.5, route="Filter 1")
    p.env(1, dec=0.06, sus=0.0); p.mod("Env 2", "Osc A Pitch", 0.2)
    p.filt(0, type="LP 12", cutoff=rr(r, 600, 1500, 0))
    p.env(0, att=0.0, dec=rr(r, 0.6, 1.5, 2), sus=0.0, rel=0.4)
    p.fx("dist", type="Soft Clip", drive=0.25, mix=0.5)
    reverb(p, r, "Hall", mix=(0.25, 0.4), size=(0.8, 1.0))
    macros(p)


@arch(D, "Orchestral Drums", ["Orchestral Percussion", "Concert Percussion", "Symphonic Kit", "Score Percussion", "Pit Percussion"], role="perc", chars=["clean", "wide", "dark", "bright", "fx"])
def _(p, r, i):
    kit(p, r, "Orchestral Percussion")
    reverb(p, r, "Hall", mix=(0.18, 0.32), size=(0.7, 1.0), decay=(2, 4))
    macros(p, tone=False); p.add_tags("Kit")


@arch(D, "Orchestral Drums", ["Timpani", "Synth Timpani", "Kettle Drum", "Epic Timpani", "Timpani Roll"], role="perc", chars=["clean", "dark", "wide"])
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Sine", "Half Sine"]), level=0.8)
    p.osc(1, wt="Sine", semi=7, oct=1, level=0.2)
    p.noise(type="Brown", level=0.3, route="Filter 1")
    p.env(2, dec=0.05, sus=0.0); p.mod("Env 3", "Noise Level", 0.5); p.set(noise_level=0.05)
    p.env(1, dec=0.08, sus=0.0); p.mod("Env 2", "Osc A Pitch", 0.04)
    p.filt(0, type="LP 12", cutoff=rr(r, 800, 2000, 0), key=0.5)
    p.env(0, att=0.0, dec=rr(r, 1.2, 2.5, 2), sus=0.0, rel=1.0)
    if i % 2 == 1:
        p.lfo(0, shape="Saw Down", rate=rr(r, 12, 18, 1), mode="Free"); p.mod("LFO 1", "Amp", 0.3); p.env(0, dec=4.0, sus=0.8); p.add_tags("Roll")
    reverb(p, r, "Hall", mix=(0.2, 0.35))
    macros(p)
    p.add_tags("Tonal")


@arch(D, "Percussion", ["Hand Percussion", "Shakers & Bells", "Percussion Kit", "Groove Percussion", "Studio Perc"], role="perc", chars=["clean", "wide", "dark", "bright", "vintage", "lofi"])
def _(p, r, i):
    kit(p, r, pick(r, ["Hand Percussion", "World Percussion"]))
    reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p, tone=False); p.add_tags("Kit")


@arch(D, "Percussion", ["Synth Perc", "Blip Perc", "Wood Perc", "Click Perc", "Zap Perc"], role="perc", chars=["clean", "digital", "lofi", "fx"])
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM 3:1", "Sine", "Ring Mod", "Half Sine"]), pos=0.5, level=0.7)
    p.env(1, dec=rr(r, 0.01, 0.05, 3), sus=0.0); p.mod("Env 2", "Osc A Pitch", rr(r, -0.3, 0.5, 2))
    p.filt(0, type="BP 12", cutoff=rr(r, 1000, 4000, 0), res=0.3, key=0.8)
    p.env(0, att=0.0, dec=rr(r, 0.06, 0.25, 2), sus=0.0, rel=0.05)
    reverb(p, r, "Room", mix=(0.08, 0.18))
    macros(p)


# ------------------------------------------------------------------------------------------- Tonal Drums
@arch(D, "Tonal Drums", ["Hang Drum", "Handpan", "Steel Tongue", "Meditation Hang", "Zen Pan"], role="real", chars=["clean", "wide", "dark", "fx", "bright"])
def _(p, r, i):
    p.set(polyphony=16)
    p.sample("Hang Drum", level=0.9)
    p.env(0, att=0.0, dec=3.0, sus=1.0, rel=1.0)
    reverb(p, r, "Hall", mix=(0.2, 0.35))
    macros(p)


@arch(D, "Tonal Drums", ["Tuned Toms", "808 Toms", "Melodic Toms", "Synth Toms", "Disco Toms"], role="perc", chars=["clean", "vintage", "digital", "lofi"])
def _(p, r, i):
    p.osc(0, wt="Sine", level=0.85)
    p.env(1, dec=rr(r, 0.05, 0.15, 2), sus=0.0); p.mod("Env 2", "Osc A Pitch", rr(r, 0.1, 0.25, 2))
    p.noise(type="Pink", level=0.15, route="Filter 1")
    p.env(2, dec=0.02, sus=0.0); p.mod("Env 3", "Noise Level", 0.3); p.set(noise_level=0.0)
    p.filt(0, type="LP 12", cutoff=3000, key=0.6)
    p.env(0, att=0.0, dec=rr(r, 0.3, 0.8, 2), sus=0.0, rel=0.2)
    reverb(p, r, "Room", mix=(0.08, 0.18))
    macros(p)


@arch(D, "Tonal Drums", ["Tabla", "Tabla Bend", "Bayan", "Indian Drum", "Dholak"], role="perc", chars=["clean", "wide", "dark"])
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Sine", "Half Sine"]), level=0.8)
    p.env(1, att=0.0, dec=0.3, sus=0.0); p.mod("Env 2", "Osc A Pitch", -rr(r, 0.04, 0.1, 2))
    p.noise(type="Pink", level=0.2, route="Filter 1")
    p.env(2, dec=0.015, sus=0.0); p.mod("Env 3", "Noise Level", 0.4); p.set(noise_level=0.0)
    p.filt(0, type="Peak", cutoff=1200, res=0.3, key=0.7)
    p.env(0, att=0.0, dec=rr(r, 0.3, 0.6, 2), sus=0.0, rel=0.2)
    reverb(p, r, "Room", mix=(0.1, 0.2))
    macros(p); p.add_tags("Emulated")


# ------------------------------------------------------------------------------------------- Loops (hold C1)
DRUM_LOOPS = {
    "Rock Beat": [K, None, CH, None, SN, None, CH, None, K, None, K, CH, SN, None, CH, None],
    "Hip Hop Beat": [K, None, None, CH, SN, None, CH, K, None, K, CH, None, SN, None, CH, CH],
    "House Groove": [K, None, OH, None, K, None, OH, None, K, None, OH, None, K, None, OH, CLAP],
    "Breakbeat": [K, None, CH, SN, None, SN, CH, K, None, K, SN, None, CH, SN, None, CH],
    "Trap Hats": [K, CH, CH, CH, SN, CH, CH, K, CH, K, CH, CH, SN, CH, CH, CH],
    "Shuffle": [K, None, CH, SN, None, CH, K, None, CH, SN, None, CH, K, None, CH, SN],
    "Funk Groove": [K, None, CH, None, SN, CH, None, K, CH, K, SN, CH, None, SN, CH, None],
    "Disco Beat": [K, CH, OH, CH, K, CH, OH, CH, K, CH, OH, CH, K, CH, OH, SN],
    "Reggae One Drop": [CH, None, CH, None, CH, None, CH, None, K, None, CH, None, CH, None, RIM, None],
    "Tom Groove": [K, None, LT, None, MT, None, HT, None, K, LT, None, MT, SN, None, HT, None],
    "Jungle Break": [K, None, SN, None, None, K, SN, None, K, None, SN, K, None, None, SN, CH],
    "Half Time": [K, None, CH, None, CH, None, CH, None, SN, None, CH, None, CH, None, CH, K],
}
PERC_LOOPS = {   # Hand Percussion kit, absolute keys; played from C2 (48), the sequencer reaches 14 steps up or down
    "Conga Groove": [36, None, 52, 53, None, 52, 44, None, 36, None, 52, None, 53, 52, 44, None],
    "Bongo Run": [48, 50, 48, None, 50, 48, 50, None, 48, 48, 50, None, 50, 48, None, 50],
    "Shaker Groove": [44, 46, 44, 46, 44, 46, 44, 46, 44, 46, 44, 46, 44, 46, 44, 46],
    "Darbuka Rhythm": [41, None, 43, 43, None, 41, 43, None, 41, None, 43, None, 43, 43, 41, None],
    "Latin Perc": [37, None, None, 37, None, None, 37, None, None, 37, None, 37, None, None, 37, None],
    "Cajon Flamenco": [38, None, 36, 38, None, 36, None, 38, 36, None, 38, None, 36, 38, None, 38],
    "Cowbell Groove": [56, None, 56, None, 57, None, 56, 56, None, 56, None, 57, 56, None, 57, None],
    "Woodblock Tick": [62, None, 62, 62, None, 62, None, 62, 62, None, 62, None, 62, 62, None, 62],
}


def loop_pattern(r, base, swing=False):
    out = []
    for s, d in enumerate(base):
        if d is None: out.append(None); continue
        acc = 1.0 if s % 4 == 0 else rr(r, 0.55, 0.85, 2)
        out.append((min(d, 14), acc, 0.9, 0))
    return out


@arch(D, "Drum Loop", list(DRUM_LOOPS.keys()), role="perc", chars=["clean", "vintage", "lofi", "dark", "bright", "fx", "wide", "digital"])
def _(p, r, i):
    name = list(DRUM_LOOPS.keys())[i % len(DRUM_LOOPS)]
    kit(p, r, pick(r, KITS + ["Drum Machine"]))
    p.arpeggio(pattern=loop_pattern(r, DRUM_LOOPS[name]), mode="Sequencer", rate=pick(r, ["1/16", "1/16", "1/8"]), steps=16, gate=0.9, scale="Chromatic",
               swing=rr(r, 0.0, 0.4, 2), latch=0)
    drum_bus(p, r)
    reverb(p, r, "Room", mix=(0.06, 0.14))
    macros(p, tone=False)
    p.add_tags("Loop", "Hold C1")


@arch(D, "Percs Loop", list(PERC_LOOPS.keys()), role="perc", chars=["clean", "wide", "lofi", "dark", "fx", "vintage"])
def _(p, r, i):
    name = list(PERC_LOOPS.keys())[i % len(PERC_LOOPS)]
    kit(p, r, "Hand Percussion")
    pat = [(k - 48, 1.0 if s % 4 == 0 else rr(r, 0.5, 0.85, 2), 0.9, 0) if k is not None else None for s, k in enumerate(PERC_LOOPS[name])]
    p.arpeggio(pattern=pat, mode="Sequencer", rate="1/16", steps=16, gate=0.9, scale="Chromatic", swing=rr(r, 0.0, 0.5, 2))
    reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p, tone=False)
    p.add_tags("Loop", "Hold C2")
