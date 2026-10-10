#!/usr/bin/env python3
"""Generates the PYRA factory preset bank (500 presets).

Every sub-category has a hand-written recipe; each preset in it is a deliberate,
seeded variation of that recipe (brightness, movement, width, character, FX).
The output is normalised for loudness afterwards by `PyraRenderTest --normalize`.

    python3 Tools/make_presets.py Resources/FactoryPresets.raw.json
"""
import json, math, random, sys, hashlib

# ----------------------------------------------------------------------------- constants
WT = {n: i for i, n in enumerate([
    "Basic Shapes", "Sine", "Saw", "Square PWM", "Tri > Saw", "Analog Saw", "Hard Sync", "Harmonic Sweep",
    "Odd Harmonics", "Organ", "Vowels", "Choir", "FM Bell", "FM Metal", "FM Bass", "Wavefold", "Digital Bits",
    "Chip", "Growl", "Spectral Comb", "Piano", "Strings", "Brass", "Reed", "Glass", "Sub Bass", "Noise Spectra",
    "Phaser Saw"])}
SHAPE_POS = {"sine": 0.0, "tri": 1 / 3, "saw": 2 / 3, "square": 1.0}   # positions inside "Basic Shapes"

LP12, LP24, LADDER, HP12, HP24, BP12, BP24, NOTCH, PEAK, ALLPASS, COMBP, COMBM, FORMANT, PHASER = range(14)
SINE, TRI, SAWUP, SAWDN, SQUARE, PULSE, STAIRS, EXPUP, EXPDN, SH, SMOOTHRND = range(11)
RETRIG, FREE, ENVMODE = range(3)
DIV = {d: i for i, d in enumerate(["8 Bars", "4 Bars", "2 Bars", "1 Bar", "1/2", "1/2 D", "1/2 T", "1/4", "1/4 D",
                                   "1/4 T", "1/8", "1/8 D", "1/8 T", "1/16", "1/16 D", "1/16 T", "1/32", "1/32 T", "1/64"])}
WARP = {n: i for i, n in enumerate(["Off", "Sync", "Bend+", "Bend-", "PWM", "Asym", "Mirror", "Fold", "Quantize"])}
F1, F2, BOTH, DIRECT = range(4)
POLY, MONO, LEGATO = range(3)

SRC = {n: i for i, n in enumerate(["None", "Env1", "Env2", "Env3", "Env4", "Lfo1", "Lfo2", "Lfo3", "Lfo4", "Velocity",
                                   "Note", "Random", "ModWheel", "Pressure", "PitchBend", "Slide",
                                   "Macro1", "Macro2", "Macro3", "Macro4"])}
OSC_FIELDS = ["level", "pan", "pitch", "fine", "pos", "detune", "blend", "width", "warp", "fm"]
FILT_FIELDS = ["cutoff", "res", "drive", "mix", "env"]


def D_osc(o, field):  return 1 + o * 10 + OSC_FIELDS.index(field)
def D_filt(f, field): return 34 + f * 5 + FILT_FIELDS.index(field)
D_SUB, D_NOISE, D_NOISECOL = 31, 32, 33
def D_lfo(l): return 44 + l
D_AMP = 48


def cut(hz):
    """Cutoff in Hz -> normalised modulation distance (for readability of mod amounts)."""
    return math.log(hz / 20) / math.log(1000)


# ----------------------------------------------------------------------------- preset builder
class Preset:
    def __init__(self, name, cat, sub, tags):
        self.name, self.cat, self.subcat, self.tags = name, cat, sub, list(tags)
        self.p, self.mods, self.arp = {}, [], None

    def set(self, **kw): self.p.update(kw); return self
    def osc(self, i, **kw):
        for k, v in kw.items():
            if k == "wt" and isinstance(v, str): v = WT[v]
            if k == "warp" and isinstance(v, str): v = WARP[v]
            self.p[f"osc{'ABC'[i]}_{k}"] = v
        if "on" not in kw: self.p[f"osc{'ABC'[i]}_on"] = 1
        return self
    def shape(self, i, name, **kw): return self.osc(i, wt=0, pos=SHAPE_POS[name], **kw)
    def filt(self, i, **kw):
        for k, v in kw.items(): self.p[f"f{i + 1}_{k}"] = v
        if "on" not in kw: self.p[f"f{i + 1}_on"] = 1
        return self
    def env(self, i, **kw):
        for k, v in kw.items(): self.p[f"env{i + 1}_{k}"] = v
        return self
    def lfo(self, i, **kw):
        for k, v in kw.items():
            if k == "div" and isinstance(v, str): v = DIV[v]
            self.p[f"lfo{i + 1}_{k}"] = v
        return self
    def fx(self, name, **kw):
        self.p[f"fx_{name}_on"] = 1
        for k, v in kw.items(): self.p[f"fx_{name}_{k}"] = v
        return self
    def mod(self, src, dst, amt):
        self.mods.append((SRC[src], dst, round(amt, 4))); return self
    def sub(self, **kw):
        self.p["sub_on"] = 1
        for k, v in kw.items(): self.p[f"sub_{k}"] = v
        return self
    def noise(self, **kw):
        self.p["noise_on"] = 1
        for k, v in kw.items(): self.p[f"noise_{k}"] = v
        return self
    def analog(self, age=None, **kw):
        if age is not None: self.p["an_age"] = age
        for k, v in kw.items(): self.p[k if k in ("drift", "spread") else f"an_{k}"] = v
        return self
    def arpeggio(self, **kw):
        self.p["arp_on"] = 1
        for k, v in kw.items():
            if k == "rate" and isinstance(v, str): v = DIV[v]
            self.p[f"arp_{k}"] = v
        return self

    def to_json(self, pid):
        params = {k: (round(v, 5) if isinstance(v, float) else v) for k, v in self.p.items()}
        for n, (s, d, a) in enumerate(self.mods[:16]):
            params[f"mod{n + 1}_src"] = s; params[f"mod{n + 1}_dst"] = d; params[f"mod{n + 1}_amt"] = a
        out = {"id": pid, "name": self.name, "category": self.cat, "sub": self.subcat, "tags": self.tags, "params": params}
        if self.arp: out["arp"] = self.arp
        return out


# ----------------------------------------------------------------------------- shared flavour helpers
def space(p, r, amount=1.0, kind=None):
    """Adds a reverb and/or delay with sensible, varied settings."""
    kind = kind or r.choice(["rev", "rev", "dly", "both"])
    if kind in ("rev", "both"):
        p.fx("rev", type=r.choice([0, 1, 1, 2]) if amount > 0.6 else r.choice([0, 1]),
             size=round(r.uniform(0.35, 0.85), 2), decay=round(r.uniform(1.2, 4.5) * amount + 0.5, 2),
             damp=round(r.uniform(0.3, 0.7), 2), mix=round(r.uniform(0.12, 0.3) * amount, 3), pre=round(r.uniform(5, 40), 1))
    if kind in ("dly", "both"):
        p.fx("dly", mode=r.choice([0, 1, 2]), div=r.choice([DIV["1/8 D"], DIV["1/4"], DIV["1/8"], DIV["1/4 D"]]),
             fb=round(r.uniform(0.25, 0.5), 2), mix=round(r.uniform(0.12, 0.25) * amount, 3), highcut=r.choice([5000, 7000, 9000]))


def width(p, r):
    if r.random() < 0.5: p.fx("cho", depth=round(r.uniform(0.3, 0.6), 2), rate=round(r.uniform(0.3, 1.0), 2), mix=round(r.uniform(0.25, 0.45), 2))


def character(p, r, chance=0.35):
    if r.random() < chance: p.analog(age=round(r.uniform(0.15, 0.55), 2))


def macros(p, filt=0, pos_osc=0):
    """Every preset gets useful macros: M1 = tone, M2 = timbre."""
    if p.p.get(f"f{filt + 1}_on", 1 if filt == 0 else 0):
        p.mod("Macro1", D_filt(filt, "cutoff"), 0.3)
    p.mod("Macro2", D_osc(pos_osc, "pos"), 0.35)


# ----------------------------------------------------------------------------- recipes
# Each recipe: (category, sub-category, count, tags, word pool, builder(p, i, r))
RECIPES = []
def recipe(cat, sub, count, tags, words):
    def deco(fn):
        RECIPES.append((cat, sub, count, tags, words, fn)); return fn
    return deco


# ============================== BASS (70)
@recipe("Bass", "Sub", 8, ["Mono", "Clean"], ["Floor", "Deep", "Pressure", "Pure", "Round", "Abyss", "Core", "Quake"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=round(r.uniform(0.02, 0.08), 3), polyphony=1)
    p.osc(0, wt="Sub Bass", pos=round(i / 7 * 0.7, 2), level=0.85)
    p.filt(0, type=LP24, cutoff=r.choice([300, 450, 700, 1200]), res=0.05)
    p.env(0, att=0.003, dec=1.0, sus=1.0, rel=0.08)
    if i % 3 == 2: p.fx("dist", type=0, drive=0.2, mix=0.5)
    macros(p)

@recipe("Bass", "Reese", 10, ["Mono", "Wide", "Dark"], ["Classic", "Neuro", "Liquid", "Steel", "Halftime", "Smoke", "Tunnel", "Venom", "Orbit", "Concrete"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.03, polyphony=1)
    for o in (0, 1):
        p.osc(o, wt="Analog Saw" if o == 0 else "Saw", voices=r.choice([3, 4, 5]), detune=round(r.uniform(0.12, 0.28), 2),
              width=round(r.uniform(0.4, 0.9), 2), level=0.6, fine=round(r.uniform(-12, 12), 1))
    p.osc(1, oct=r.choice([0, 0, -1]))
    p.sub(shape=0, oct=-1, level=0.5, route=DIRECT)
    p.filt(0, type=r.choice([LP24, LADDER]), cutoff=r.choice([350, 500, 700, 950]), res=round(r.uniform(0.1, 0.35), 2), drive=0.3)
    p.lfo(1, shape=SINE, sync=0, rate=round(r.uniform(0.1, 0.4), 2), mode=FREE)
    p.mod("Lfo2", D_filt(0, "cutoff"), round(r.uniform(0.04, 0.1), 3))
    if r.random() < 0.5: p.fx("pha", rate=0.15, depth=0.5, mix=0.35)
    if r.random() < 0.6: p.fx("dist", type=r.choice([0, 2, 5]), drive=round(r.uniform(0.2, 0.5), 2), mix=0.7)
    macros(p)

@recipe("Bass", "Wobble", 10, ["Mono", "Movement", "Dubstep"], ["Quarter", "Eighth", "Triplet", "Stomp", "Yoyo", "Rubber", "Lurch", "Swamp", "Gremlin", "Talker"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.02, polyphony=1)
    p.osc(0, wt=r.choice(["Saw", "Growl", "Square PWM"]), voices=2, detune=0.1, level=0.8)
    p.sub(shape=0, oct=-1, level=0.55, route=DIRECT)
    p.filt(0, type=r.choice([LP24, LADDER, BP12, FORMANT]), cutoff=r.choice([180, 250, 350]), res=round(r.uniform(0.35, 0.6), 2), drive=0.4)
    div = ["1/4", "1/8", "1/8 T", "1/4 T", "1/16", "1/4", "1/8", "1/8 D", "1/16", "1/8 T"][i]
    p.lfo(0, shape=r.choice([SINE, TRI, SAWDN, EXPDN]), sync=1, div=div, mode=RETRIG, smooth=0.1)
    p.mod("Lfo1", D_filt(0, "cutoff"), round(r.uniform(0.35, 0.5), 2))
    p.mod("Lfo1", D_osc(0, "pos"), 0.4)
    p.fx("dist", type=r.choice([0, 2, 4, 5]), drive=round(r.uniform(0.35, 0.65), 2), mix=0.8)
    p.fx("comp", thresh=-20, ratio=4, makeup=4)
    macros(p)

@recipe("Bass", "Growl", 8, ["Mono", "Aggressive", "Movement"], ["Beast", "Snarl", "Mangler", "Grinder", "Cyborg", "Maw", "Forge", "Rotor"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.02, polyphony=1)
    p.osc(0, wt="Growl", pos=0.3, voices=2, detune=0.08, warp=r.choice(["Fold", "Bend+", "Off"]), warpAmt=0.3, level=0.8)
    p.osc(1, wt="FM Metal", pos=0.3, level=0.35, oct=-1)
    p.sub(shape=0, oct=-1, level=0.5, route=DIRECT)
    p.filt(0, type=FORMANT, cutoff=r.choice([400, 800, 1500]), res=0.6)
    p.lfo(0, shape=r.choice([SINE, TRI, SMOOTHRND]), sync=1, div=r.choice(["1/8", "1/16", "1/8 T", "1/4"]))
    p.mod("Lfo1", D_osc(0, "pos"), 0.5).mod("Lfo1", D_filt(0, "cutoff"), 0.3)
    p.fx("dist", type=r.choice([3, 4, 5]), drive=0.55, mix=0.9).fx("comp", thresh=-22, ratio=6, makeup=5)
    macros(p)

@recipe("Bass", "Pluck", 8, ["Mono", "Pluck", "Punchy"], ["Bounce", "Finger", "Thump", "Snappy", "Dotted", "Stab", "Kick", "Rubber"])
def _(p, i, r):
    p.set(voiceMode=MONO, polyphony=1)
    p.shape(0, r.choice(["saw", "square"]), level=0.8)
    if r.random() < 0.5: p.shape(1, "saw", oct=-1, level=0.4, fine=6)
    p.filt(0, type=r.choice([LP24, LADDER]), cutoff=r.choice([120, 180, 250]), res=round(r.uniform(0.15, 0.45), 2), env=round(r.uniform(0.35, 0.55), 2))
    p.env(1, att=0.0, dec=round(r.uniform(0.12, 0.35), 2), sus=0.0)
    p.env(0, att=0.001, dec=round(r.uniform(0.3, 0.7), 2), sus=round(r.uniform(0.0, 0.4), 2), rel=0.08)
    macros(p)

@recipe("Bass", "808", 8, ["Mono", "Trap", "Hip Hop"], ["Boom", "Tokyo", "Glide", "Distorted", "Long", "Short", "Atlanta", "Slider"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=round(r.uniform(0.04, 0.12), 3), glideLegato=1, polyphony=1)
    p.osc(0, wt="Sine", level=0.9)
    p.filt(0, on=0)
    p.env(1, att=0.0, dec=0.05, sus=0.0)
    p.mod("Env2", D_osc(0, "pitch"), round(r.uniform(0.12, 0.25), 2))
    p.env(0, att=0.001, dec=round(r.uniform(1.2, 3.5), 2), sus=0.0, rel=0.15, dCrv=0.5)
    p.fx("dist", type=r.choice([0, 2, 5, 0]), drive=round(r.uniform(0.15, 0.6), 2), tone=0.5, mix=1.0)
    p.fx("comp", thresh=-14, ratio=3, makeup=2)
    p.mod("Macro1", D_osc(0, "pitch"), 0.0)
    p.mod("Macro2", D_SUB, 0.0)

@recipe("Bass", "Acid", 8, ["Mono", "Acid", "Squelch"], ["303", "Squelch", "Rave", "Chicago", "Lysergic", "Bubble", "Warehouse", "Night"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.06, polyphony=1)
    p.shape(0, "saw" if i % 2 == 0 else "square", level=0.8)
    p.filt(0, type=LADDER, cutoff=r.choice([300, 450, 650]), res=round(r.uniform(0.7, 0.85), 2), drive=0.4,
           env=round(r.uniform(0.35, 0.55), 2))
    p.env(1, dec=round(r.uniform(0.15, 0.3), 2), sus=0.0)
    p.mod("Velocity", D_filt(0, "env"), 0.25)
    p.fx("dist", type=r.choice([0, 5, 2]), drive=round(r.uniform(0.3, 0.6), 2), mix=0.6)
    if r.random() < 0.6: p.fx("dly", div=DIV["1/8 D"], fb=0.35, mix=0.15)
    macros(p)

@recipe("Bass", "FM", 10, ["Mono", "Digital", "FM"], ["Metal", "Rubber", "Wire", "Glass", "Chrome", "Bit", "Hollow", "Ping", "Zap", "Coil"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.02, polyphony=1)
    p.osc(0, wt="FM Bass", pos=round(r.uniform(0.3, 0.7), 2), level=0.8)
    p.osc(1, wt="Sine", level=0.0, oct=r.choice([0, 1, 2]))
    p.osc(0, fmSrc=2, fmAmt=round(r.uniform(0.1, 0.35), 2))
    p.env(1, dec=round(r.uniform(0.15, 0.6), 2), sus=0.0)
    p.mod("Env2", D_osc(0, "fm"), round(r.uniform(0.2, 0.45), 2)).mod("Env2", D_osc(0, "pos"), 0.25)
    p.sub(shape=0, oct=-1, level=0.4, route=DIRECT)
    p.filt(0, type=LP24, cutoff=3000, res=0.1)
    macros(p)


# ============================== LEAD (65)
@recipe("Lead", "Supersaw", 12, ["Wide", "Trance", "EDM", "Bright"], ["Anthem", "Euphoria", "Festival", "Horizon", "Mainstage", "Uplift", "Halo", "Skyline", "Ascend", "Neon", "Titan", "Aurora"])
def _(p, i, r):
    for o, oc in zip((0, 1, 2), (0, r.choice([-1, 1]), 0)):
        p.osc(o, wt=r.choice(["Saw", "Analog Saw"]), voices=r.choice([7, 9]), detune=round(r.uniform(0.3, 0.45), 2), blend=0.8,
              width=1.0, level=0.45, oct=oc)
    p.osc(2, on=1 if i % 2 else 0)
    p.filt(0, type=LP24, cutoff=r.choice([5000, 8000, 12000]), res=0.1)
    p.filt(1, type=HP12, cutoff=120)
    p.env(0, att=0.003, dec=0.6, sus=0.85, rel=round(r.uniform(0.3, 0.7), 2))
    space(p, r, 1.0, "both"); width(p, r)
    macros(p)

@recipe("Lead", "Mono", 10, ["Mono", "Analog", "Classic"], ["Moog", "Solo", "Hero", "Prophet", "Wire", "Fat", "Classic", "Bite", "Silk", "Rider"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=round(r.uniform(0.04, 0.12), 3), polyphony=1)
    p.shape(0, "saw", level=0.7)
    p.shape(1, r.choice(["square", "saw"]), fine=round(r.uniform(4, 10), 1), level=0.5, oct=r.choice([0, -1]))
    p.filt(0, type=LADDER, cutoff=r.choice([1200, 2000, 3000]), res=round(r.uniform(0.2, 0.45), 2), env=0.25, key=0.5)
    p.env(1, dec=0.4, sus=0.2)
    p.lfo(0, shape=SINE, sync=0, rate=5.5, delay=0.35, fade=0.4)
    p.mod("ModWheel", D_osc(0, "pitch"), 0.004).mod("Lfo1", D_osc(0, "fine"), 0.12).mod("Lfo1", D_osc(1, "fine"), 0.12)
    character(p, r, 0.7)
    space(p, r, 0.7, "dly")
    macros(p)

@recipe("Lead", "Sync", 8, ["Mono", "Aggressive", "Classic"], ["Sync Rip", "Tear", "Shriek", "Cutter", "Laser", "Sweep", "Rally", "Jet"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.03, polyphony=1)
    p.osc(0, wt="Hard Sync", pos=0.1, level=0.8, voices=r.choice([1, 2, 3]), detune=0.1)
    p.env(2, att=0.0, dec=round(r.uniform(0.4, 1.2), 2), sus=round(r.uniform(0.1, 0.4), 2))
    p.mod("Env3", D_osc(0, "pos"), round(r.uniform(0.5, 0.8), 2))
    p.lfo(0, shape=TRI, sync=0, rate=round(r.uniform(0.3, 1.5), 2))
    p.mod("Lfo1", D_osc(0, "pos"), 0.15)
    p.filt(0, type=LP24, cutoff=6000, res=0.15)
    if r.random() < 0.6: p.fx("dist", type=0, drive=0.3, mix=0.5)
    space(p, r, 0.6, "dly")
    macros(p)

@recipe("Lead", "Screamer", 7, ["Mono", "Aggressive", "Distorted"], ["Banshee", "Siren", "Riot", "Fury", "Inferno", "Howl", "Shred"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.05, polyphony=1)
    p.shape(0, "saw", voices=3, detune=0.15, level=0.7)
    p.shape(1, "square", oct=1, level=0.35)
    p.filt(0, type=r.choice([BP24, PEAK, LADDER]), cutoff=r.choice([1500, 2500]), res=0.6, drive=0.6)
    p.fx("dist", type=r.choice([2, 5, 1]), drive=round(r.uniform(0.55, 0.8), 2), tone=0.6, mix=1.0)
    p.mod("ModWheel", D_filt(0, "cutoff"), 0.25).mod("Velocity", D_filt(0, "cutoff"), 0.15)
    space(p, r, 0.6, "both")
    macros(p)

@recipe("Lead", "Soft", 8, ["Soft", "Smooth", "Mellow"], ["Whisper", "Velvet", "Breeze", "Feather", "Lullaby", "Cloud", "Satin", "Glow"])
def _(p, i, r):
    p.shape(0, r.choice(["tri", "sine"]), level=0.8)
    p.shape(1, "tri", oct=1, level=round(r.uniform(0.1, 0.3), 2))
    p.filt(0, type=LP12, cutoff=r.choice([1500, 2500, 4000]), res=0.1)
    p.env(0, att=round(r.uniform(0.02, 0.08), 3), dec=0.5, sus=0.8, rel=0.6)
    p.lfo(0, shape=SINE, sync=0, rate=5.0, delay=0.5, fade=0.6)
    p.mod("Lfo1", D_osc(0, "fine"), 0.08)
    width(p, r); space(p, r, 1.0, "both")
    macros(p)

@recipe("Lead", "Chip", 7, ["Retro", "8-bit", "Game"], ["Pixel", "Arcade", "Cartridge", "Boss Level", "1-Up", "Sprite", "Pocket"])
def _(p, i, r):
    p.set(voiceMode=r.choice([MONO, LEGATO]), glide=r.choice([0.0, 0.03]), polyphony=1)
    p.osc(0, wt="Chip", pos=round(r.choice([0.0, 1 / 7, 2 / 7, 3 / 7]), 3), level=0.6)
    p.filt(0, on=0)
    p.env(0, att=0.0, dec=0.3, sus=0.7, rel=0.05)
    p.lfo(0, shape=SQUARE, sync=1, div="1/32", delay=0.25)
    p.mod("Lfo1", D_osc(0, "pitch"), round(r.choice([0.0, 0.0052, 0.0104]), 4))
    p.analog(bits=round(r.uniform(0.2, 0.45), 2), rate=round(r.uniform(0.0, 0.3), 2))
    if r.random() < 0.5: p.fx("dly", div=DIV["1/8"], fb=0.3, mix=0.15, mode=0)
    macros(p)

@recipe("Lead", "Vocal", 7, ["Vocal", "Formant", "Movement"], ["Talkbox", "Yeah", "Oooh", "Mouth", "Speaker", "Robot", "Chant"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.06, polyphony=1)
    p.osc(0, wt="Vowels", pos=0.2, voices=2, detune=0.1, level=0.8)
    p.lfo(0, shape=r.choice([SINE, TRI, SMOOTHRND]), sync=1, div=r.choice(["1/4", "1/2", "1/8"]))
    p.mod("Lfo1", D_osc(0, "pos"), 0.6)
    p.filt(0, type=FORMANT, cutoff=r.choice([300, 900, 3000]), res=0.5, mix=0.6)
    p.mod("Lfo1", D_filt(0, "cutoff"), 0.3)
    space(p, r, 0.7)
    macros(p)

@recipe("Lead", "Pluck Lead", 6, ["Pluck", "Bright"], ["Spark", "Flick", "Glint", "Dart", "Ping", "Zing"])
def _(p, i, r):
    p.shape(0, "saw", voices=3, detune=0.2, level=0.6)
    p.shape(1, "square", oct=1, level=0.3)
    p.filt(0, type=LP24, cutoff=600, res=0.25, env=0.5)
    p.env(1, dec=round(r.uniform(0.2, 0.4), 2), sus=0.15)
    p.env(0, dec=0.6, sus=0.4, rel=0.3)
    space(p, r, 0.8, "both")
    macros(p)


# ============================== PAD (60)
@recipe("Pad", "Warm", 12, ["Analog", "Warm", "Wide"], ["Blanket", "Hearth", "Amber", "Sunset", "Honey", "Embrace", "Cocoon", "Dusk", "Lantern", "Cashmere", "Ember", "Harbor"])
def _(p, i, r):
    p.osc(0, wt="Analog Saw", voices=5, detune=round(r.uniform(0.2, 0.35), 2), level=0.55)
    p.osc(1, wt=r.choice(["Analog Saw", "Tri > Saw", "Square PWM"]), pos=0.4, voices=3, detune=0.2, oct=r.choice([0, -1, 1]), level=0.45)
    p.filt(0, type=r.choice([LP24, LADDER]), cutoff=r.choice([900, 1400, 2200]), res=0.15)
    p.env(0, att=round(r.uniform(0.4, 1.2), 2), dec=1.5, sus=0.85, rel=round(r.uniform(1.2, 3.0), 2))
    p.lfo(0, shape=SINE, sync=0, rate=round(r.uniform(0.08, 0.3), 2), mode=FREE)
    p.mod("Lfo1", D_filt(0, "cutoff"), 0.06)
    p.analog(age=round(r.uniform(0.2, 0.6), 2))
    width(p, r); space(p, r, 1.2, "rev")
    macros(p)

@recipe("Pad", "Evolving", 12, ["Movement", "Evolving", "Cinematic"], ["Tide", "Nebula", "Drift", "Metamorph", "Bloom", "Current", "Shifting Sands", "Aurora", "Mirage", "Flux", "Kaleido", "Seasons"])
def _(p, i, r):
    tabs = ["Harmonic Sweep", "Spectral Comb", "Phaser Saw", "Noise Spectra", "Vowels", "Wavefold"]
    p.osc(0, wt=tabs[i % len(tabs)], voices=4, detune=0.2, level=0.55)
    p.osc(1, wt=r.choice(tabs), voices=3, detune=0.25, oct=r.choice([-1, 1]), level=0.4)
    p.lfo(0, shape=r.choice([TRI, SINE, SMOOTHRND]), sync=0, rate=round(r.uniform(0.04, 0.15), 3), mode=FREE)
    p.lfo(1, shape=SMOOTHRND, sync=0, rate=round(r.uniform(0.1, 0.3), 2), mode=FREE)
    p.mod("Lfo1", D_osc(0, "pos"), 0.45).mod("Lfo2", D_osc(1, "pos"), 0.35).mod("Lfo2", D_filt(0, "cutoff"), 0.08)
    p.filt(0, type=r.choice([LP24, LP12, PHASER]), cutoff=r.choice([1500, 3000, 6000]), res=0.2)
    p.env(0, att=round(r.uniform(0.8, 2.5), 2), dec=2, sus=0.9, rel=round(r.uniform(2, 4), 2))
    space(p, r, 1.4, "rev"); width(p, r)
    macros(p)

@recipe("Pad", "Dark", 8, ["Dark", "Cinematic"], ["Underworld", "Catacomb", "Eclipse", "Hollow", "Midnight", "Void", "Crypt", "Fog"])
def _(p, i, r):
    p.osc(0, wt=r.choice(["Analog Saw", "Square PWM", "Odd Harmonics"]), voices=4, detune=0.25, level=0.6, oct=-1)
    p.sub(shape=1, oct=-1, level=0.3)
    p.noise(type=2, level=0.08, color=-0.6)
    p.filt(0, type=r.choice([LP24, LADDER]), cutoff=r.choice([350, 500, 700]), res=0.3)
    p.lfo(0, shape=SINE, sync=0, rate=0.07, mode=FREE)
    p.mod("Lfo1", D_filt(0, "cutoff"), 0.08)
    p.env(0, att=1.5, dec=2, sus=1.0, rel=3.0)
    space(p, r, 1.5, "rev")
    macros(p)

@recipe("Pad", "Choir", 10, ["Vocal", "Choir", "Cinematic"], ["Cathedral", "Angels", "Monks", "Chapel", "Ethereal", "Heaven", "Requiem", "Hymn", "Vespers", "Psalm"])
def _(p, i, r):
    p.osc(0, wt="Choir", pos=round(r.uniform(0.0, 0.8), 2), voices=5, detune=0.2, level=0.6)
    p.osc(1, wt="Vowels", pos=round(r.uniform(0.0, 1.0), 2), voices=3, detune=0.15, level=0.35, oct=r.choice([0, 1]))
    p.filt(0, type=FORMANT, cutoff=r.choice([500, 1000, 2000]), res=0.3, mix=0.5)
    p.filt(1, type=HP12, cutoff=150)
    p.lfo(0, shape=SINE, sync=0, rate=0.12, mode=FREE)
    p.mod("Lfo1", D_osc(0, "pos"), 0.12)
    p.env(0, att=round(r.uniform(0.4, 1.0), 2), dec=1, sus=0.9, rel=2.5)
    p.fx("cho", depth=0.5, mix=0.4)
    space(p, r, 1.5, "rev")
    macros(p)

@recipe("Pad", "Glass", 8, ["Bright", "Digital", "Shimmer"], ["Crystal", "Prism", "Frost", "Icicle", "Diamond", "Quartz", "Glacier", "Mirror"])
def _(p, i, r):
    p.osc(0, wt=r.choice(["Glass", "FM Bell"]), pos=round(r.uniform(0.2, 0.7), 2), voices=3, detune=0.15, level=0.6)
    p.osc(1, wt="Sine", oct=1, level=0.25)
    p.filt(0, type=LP12, cutoff=8000, res=0.1)
    p.env(0, att=0.5, dec=2, sus=0.8, rel=3)
    p.fx("rev", type=2, size=0.8, decay=6, shim=round(r.uniform(0.3, 0.7), 2), mix=0.35)
    p.fx("dly", div=DIV["1/4 D"], fb=0.4, mix=0.15, mode=2)
    macros(p)

@recipe("Pad", "Ambient", 10, ["Ambient", "Texture", "Wide"], ["Stillness", "Breathing Room", "Weightless", "Horizon Line", "Slow Motion", "Afterglow", "Haze", "Reverie", "Undertow", "Satellite"])
def _(p, i, r):
    p.osc(0, wt=r.choice(["Sine", "Tri > Saw", "Harmonic Sweep", "Glass"]), voices=6, detune=0.15, level=0.5)
    p.osc(1, wt="Noise Spectra", pos=round(r.random(), 2), level=0.2, oct=1)
    p.noise(type=1, level=0.05, color=0.3)
    p.filt(0, type=LP12, cutoff=r.choice([2000, 4000]), res=0.1)
    p.lfo(0, shape=SMOOTHRND, sync=0, rate=0.1, mode=FREE)
    p.mod("Lfo1", D_osc(1, "pos"), 0.4)
    p.env(0, att=round(r.uniform(1.5, 4.0), 2), dec=2, sus=1.0, rel=round(r.uniform(3, 6), 2))
    p.fx("rev", type=r.choice([1, 2]), size=0.9, decay=round(r.uniform(5, 10), 1), mix=0.4, shim=0.4)
    p.fx("dly", div=DIV["1/2"], fb=0.5, mix=0.15, mode=1)
    macros(p)


# ============================== KEYS (45)
@recipe("Keys", "Electric Piano", 10, ["Realistic", "Vintage", "Soft"], ["Suitcase", "Stage 73", "Tine", "Wurly", "Soul", "Neo Soul", "Velvet Keys", "Late Night", "Lounge", "Dyno"])
def _(p, i, r):
    p.osc(0, wt="FM Bell", pos=round(r.uniform(0.05, 0.25), 2), level=0.6)
    p.osc(1, wt="Sine", level=0.6)
    p.filt(0, type=LP12, cutoff=r.choice([2500, 4000, 6000]), res=0.05)
    p.env(0, att=0.002, dec=round(r.uniform(1.8, 3.5), 2), sus=0.15, rel=0.35)
    p.env(1, dec=0.6, sus=0.0)
    p.mod("Env2", D_osc(0, "level"), 0.0).mod("Velocity", D_osc(0, "pos"), 0.2).mod("Velocity", D_filt(0, "cutoff"), 0.12)
    p.set(velAmp=0.7)
    if i % 2 == 0:
        p.lfo(0, shape=SINE, sync=0, rate=round(r.uniform(3.5, 6.0), 1), mode=FREE)
        p.mod("Lfo1", D_AMP, -0.18)   # tremolo
    if r.random() < 0.5: p.fx("pha", rate=0.4, depth=0.4, mix=0.3)
    else: p.fx("cho", depth=0.35, mix=0.3)
    p.analog(age=0.25)
    space(p, r, 0.6, "rev")
    macros(p)

@recipe("Keys", "Piano", 8, ["Realistic", "Acoustic"], ["Grand", "Upright", "Felt", "Concert", "Parlour", "Bright Grand", "Soft Hammers", "Ballad"])
def _(p, i, r):
    p.osc(0, wt="Piano", pos=0.8, level=0.7)
    p.osc(1, wt="Piano", pos=0.4, fine=round(r.uniform(2, 5), 1), level=0.3)
    p.env(1, dec=round(r.uniform(1.0, 2.0), 2), sus=0.0)
    p.mod("Env2", D_osc(0, "pos"), -0.6).mod("Velocity", D_osc(0, "pos"), 0.2)
    p.filt(0, type=LP12, cutoff=r.choice([3000, 5000, 9000]), res=0.0, key=0.4)
    p.mod("Velocity", D_filt(0, "cutoff"), 0.2)
    p.noise(type=0, level=0.0, route=F1)
    p.env(2, dec=0.03, sus=0.0)
    p.mod("Env3", D_NOISE, 0.12)   # hammer
    p.env(0, att=0.001, dec=round(r.uniform(3, 6), 2), sus=0.0, rel=0.45, dCrv=0.8)
    p.set(velAmp=0.8)
    p.fx("rev", type=0 if i % 2 else 1, size=0.5, decay=1.8, mix=0.15)
    if i == 2: p.filt(0, cutoff=1500)
    macros(p)

@recipe("Keys", "Organ", 9, ["Organ", "Vintage"], ["Drawbar", "Gospel", "Rock B3", "Church", "Jazz Combo", "Rotary", "Percussive", "Full Stops", "Soulful"])
def _(p, i, r):
    p.osc(0, wt="Organ", pos=round(i / 8, 3), level=0.7)
    p.filt(0, on=0)
    p.env(0, att=0.005, dec=0.1, sus=1.0, rel=0.06)
    if i in (1, 5, 6):
        p.lfo(0, shape=SINE, sync=0, rate=6.5, mode=FREE)
        p.mod("Lfo1", D_osc(0, "pan"), 0.35).mod("Lfo1", D_osc(0, "fine"), 0.04)
    if i == 6:
        p.osc(1, wt="Sine", oct=1, level=0.0, pos=0)
        p.env(1, dec=0.25, sus=0.0)
        p.mod("Env2", D_osc(1, "level"), 0.6)
    p.fx("dist", type=2, drive=round(r.uniform(0.1, 0.35), 2), mix=0.6)
    p.fx("cho", depth=0.4, rate=0.8, mix=0.3)
    space(p, r, 0.5, "rev")
    macros(p)

@recipe("Keys", "Bells", 8, ["Bell", "Digital", "Bright"], ["Carillon", "Celesta", "Music Box", "Tubular", "Temple", "Chime", "Glock", "Gamelan"])
def _(p, i, r):
    p.osc(0, wt=r.choice(["FM Bell", "Glass", "FM Metal"]), pos=round(r.uniform(0.3, 0.8), 2), level=0.6)
    p.osc(1, wt="Sine", oct=r.choice([1, 2]), level=0.25)
    p.env(0, att=0.001, dec=round(r.uniform(2, 6), 2), sus=0.0, rel=round(r.uniform(1, 3), 2))
    p.env(1, dec=1.5, sus=0.0)
    p.mod("Env2", D_osc(0, "pos"), -0.3)
    p.filt(0, type=LP12, cutoff=10000)
    space(p, r, 1.0, "both")
    macros(p)

@recipe("Keys", "Mallets", 5, ["Realistic", "Percussive"], ["Marimba", "Vibraphone", "Xylophone", "Kalimba", "Wood Block"])
def _(p, i, r):
    p.osc(0, wt="Sine", level=0.8)
    p.osc(1, wt="Glass", pos=0.1, oct=2 if i != 1 else 1, level=0.25)
    p.env(1, dec=0.08, sus=0.0)
    p.mod("Env2", D_osc(1, "level"), 0.5)
    p.env(0, att=0.001, dec=[0.5, 2.5, 0.25, 0.8, 0.12][i], sus=0.0, rel=0.2)
    if i == 1:
        p.lfo(0, shape=SINE, sync=0, rate=5, mode=FREE); p.mod("Lfo1", D_AMP, -0.2)
    p.filt(0, on=0)
    p.fx("rev", type=0, size=0.4, decay=1.2, mix=0.15)
    macros(p)

@recipe("Keys", "Clav", 5, ["Funk", "Percussive"], ["Funky Clav", "Wah Clav", "Muted Clav", "Bright Clav", "Phased Clav"])
def _(p, i, r):
    p.osc(0, wt="Square PWM", pos=0.75, level=0.7)
    p.osc(1, wt="Saw", level=0.3)
    p.filt(0, type=BP12 if i == 1 else HP12, cutoff=[900, 600, 400, 700, 500][i], res=0.3)
    p.filt(1, type=LP24, cutoff=4000 if i != 2 else 1500)
    p.env(0, att=0.001, dec=0.5, sus=0.2, rel=0.08)
    if i == 1:
        p.mod("Env2", D_filt(0, "cutoff"), 0.4); p.env(1, dec=0.25, sus=0.0)
    if i == 4: p.fx("pha", rate=0.5, mix=0.4)
    p.mod("Velocity", D_filt(1, "cutoff"), 0.2)
    macros(p)


# ============================== PLUCK (45)
@recipe("Pluck", "Synth Pluck", 15, ["Pluck", "EDM", "Bright"], ["Pop", "Drip", "Tropical", "Future", "Snap", "Bubble", "Candy", "Kiss", "Pebble", "Drop", "Morning", "Pastel", "Flick", "Ripple", "Daylight"])
def _(p, i, r):
    p.osc(0, wt=r.choice(["Saw", "Analog Saw", "Square PWM", "Tri > Saw"]), voices=r.choice([1, 3, 5]), detune=0.2, level=0.6)
    if r.random() < 0.6: p.shape(1, r.choice(["square", "saw", "tri"]), oct=1, level=0.3)
    p.filt(0, type=r.choice([LP24, LP12]), cutoff=r.choice([300, 500, 800]), res=round(r.uniform(0.1, 0.35), 2), env=round(r.uniform(0.4, 0.6), 2))
    p.env(1, dec=round(r.uniform(0.12, 0.35), 2), sus=0.0)
    p.env(0, att=0.001, dec=round(r.uniform(0.25, 0.6), 2), sus=0.0, rel=round(r.uniform(0.2, 0.5), 2))
    space(p, r, 0.9); width(p, r)
    macros(p)

@recipe("Pluck", "Bell Pluck", 8, ["Pluck", "Bell", "Digital"], ["Twinkle", "Star Dust", "Fairy", "Sparkle", "Crystal Drop", "Dew", "Snowflake", "Wish"])
def _(p, i, r):
    p.osc(0, wt="FM Bell", pos=round(r.uniform(0.2, 0.6), 2), level=0.6)
    p.osc(1, wt="Glass", pos=0.3, oct=1, level=0.3)
    p.env(0, att=0.001, dec=round(r.uniform(0.4, 1.0), 2), sus=0.0, rel=0.6)
    p.filt(0, type=LP12, cutoff=9000)
    space(p, r, 1.1, "both")
    macros(p)

@recipe("Pluck", "Stab", 8, ["Stab", "House", "Punchy"], ["Rave Stab", "Garage", "Warehouse Stab", "Deep Stab", "Hit", "Jab", "Punch", "Chop"])
def _(p, i, r):
    p.shape(0, "saw", voices=5, detune=0.25, level=0.55)
    p.shape(1, "square", oct=-1, level=0.4)
    p.filt(0, type=LP24, cutoff=r.choice([700, 1200]), res=0.3, env=0.4)
    p.env(1, dec=0.15, sus=0.1)
    p.env(0, att=0.001, dec=0.25, sus=0.0, rel=0.15)
    if r.random() < 0.6: p.fx("dist", type=0, drive=0.3, mix=0.5)
    space(p, r, 0.7, "rev")
    macros(p)

@recipe("Pluck", "Harp & Koto", 8, ["Realistic", "Plucked String"], ["Harp", "Koto", "Guzheng", "Lyre", "Kanun", "Zither", "Sitar Pluck", "Dulcimer"])
def _(p, i, r):
    # Karplus-Strong: a short noise burst excites a comb filter tuned to the played note
    p.osc(0, on=0)
    p.noise(type=r.choice([0, 1]), level=0.0, route=F1, color=round(r.uniform(-0.2, 0.4), 2))
    p.env(2, dec=0.03, sus=0.0, dCrv=0.8)
    p.mod("Env3", D_NOISE, 1.0)
    p.filt(0, type=COMBP, cutoff=261.63, key=1.0, res=round(r.uniform(0.86, 0.96), 3), drive=0.25)
    p.filt(1, type=LP12, cutoff=r.choice([3000, 5000, 7000]))
    p.set(filterRouting=0)
    p.env(0, att=0.001, dec=4.0, sus=0.0, rel=0.8)
    space(p, r, 0.7, "rev")
    p.mod("Macro1", D_filt(1, "cutoff"), 0.3); p.mod("Macro2", D_filt(0, "res"), 0.04)

@recipe("Pluck", "Dreamy", 6, ["Pluck", "Ambient", "Lo-Fi"], ["Daydream", "Postcard", "Polaroid", "Cassette", "Bedroom", "Old Photo"])
def _(p, i, r):
    p.shape(0, "tri", level=0.7)
    p.shape(1, "saw", oct=1, level=0.15)
    p.filt(0, type=LP12, cutoff=2000, env=0.3)
    p.env(1, dec=0.3, sus=0.0)
    p.env(0, att=0.002, dec=0.8, sus=0.0, rel=0.8)
    p.fx("tape", wow=0.4, flutter=0.3, sat=0.3, tone=0.5)
    p.analog(age=0.5, noise=0.3)
    p.fx("dly", mode=1, div=DIV["1/4 D"], fb=0.45, mix=0.25)
    p.fx("rev", type=1, decay=3, mix=0.25)
    macros(p)


# ============================== STRINGS (25)
@recipe("Strings", "Ensemble", 8, ["Realistic", "Orchestral"], ["Section", "Violins", "Violas", "Cellos", "Full Orchestra", "Chamber", "Legato", "Warm Section"])
def _(p, i, r):
    p.osc(0, wt="Strings", pos=round(r.uniform(0.3, 0.8), 2), voices=5, detune=0.12, level=0.55,
          oct=[0, 1, 0, -1, 0, 0, 0, -1][i])
    p.osc(1, wt="Analog Saw", voices=3, detune=0.15, level=0.25)
    p.filt(0, type=LP12, cutoff=r.choice([3000, 4500]), res=0.1)
    p.filt(1, type=HP12, cutoff=120)
    p.env(0, att=round(r.uniform(0.15, 0.4), 2), dec=1, sus=0.9, rel=round(r.uniform(0.5, 1.2), 2))
    p.lfo(0, shape=SINE, sync=0, rate=5.2, delay=0.4, fade=0.8)
    p.mod("Lfo1", D_osc(0, "fine"), 0.06).mod("ModWheel", D_filt(0, "cutoff"), 0.15)
    p.mod("Velocity", D_filt(0, "cutoff"), 0.1)
    p.fx("cho", depth=0.3, mix=0.25)
    p.fx("rev", type=1, size=0.7, decay=2.8, mix=0.25)
    macros(p)

@recipe("Strings", "Cinematic", 6, ["Cinematic", "Orchestral", "Slow"], ["Epic Swell", "Tension", "Overture", "Score", "Trailer", "Distant Hills"])
def _(p, i, r):
    p.osc(0, wt="Strings", pos=0.5, voices=7, detune=0.15, level=0.55)
    p.osc(1, wt="Strings", pos=0.2, voices=5, detune=0.2, oct=-1, level=0.4)
    p.filt(0, type=LP24, cutoff=1500, res=0.1)
    p.env(2, att=round(r.uniform(1.5, 3), 2), dec=1, sus=1.0)
    p.mod("Env3", D_filt(0, "cutoff"), 0.25)
    p.env(0, att=round(r.uniform(1.0, 2.5), 2), dec=1, sus=1.0, rel=2.5)
    p.fx("rev", type=1, size=0.9, decay=4.5, mix=0.35)
    macros(p)

@recipe("Strings", "Pizzicato", 6, ["Realistic", "Plucked String", "Orchestral"], ["Pizz", "Pizz Section", "Pizz Cello", "Pizz Short", "Pizz Hall", "Pizz Bright"])
def _(p, i, r):
    p.osc(0, on=0)
    p.noise(type=1, level=0.0, route=F1)
    p.env(2, dec=0.03, sus=0.0, dCrv=0.8)
    p.mod("Env3", D_NOISE, 1.0)
    p.filt(0, type=COMBP, cutoff=261.63, key=1.0, res=round(r.uniform(0.75, 0.86), 3))
    p.filt(1, type=LP12, cutoff=[2500, 3000, 1800, 2200, 2500, 5000][i])
    p.osc(1, wt="Strings", pos=0.5, level=0.25, route=F2)
    p.env(0, att=0.001, dec=0.6, sus=0.0, rel=0.3)
    p.fx("rev", type=1 if i == 4 else 0, size=0.6, decay=2.0, mix=0.25)
    p.mod("Macro1", D_filt(1, "cutoff"), 0.3)

@recipe("Strings", "Solo", 5, ["Realistic", "Expressive", "Mono"], ["Solo Violin", "Solo Cello", "Fiddle", "Erhu", "Viola Solo"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.07, polyphony=1)
    p.osc(0, wt="Strings", pos=0.7, level=0.7, oct=[0, -1, 0, 0, 0][i])
    p.filt(0, type=PEAK, cutoff=[2500, 900, 3000, 1800, 1500][i], res=0.4)
    p.filt(1, type=LP12, cutoff=5000)
    p.env(0, att=0.12, dec=1, sus=0.9, rel=0.4)
    p.lfo(0, shape=SINE, sync=0, rate=5.8, delay=0.3, fade=0.5)
    p.mod("Lfo1", D_osc(0, "fine"), 0.12).mod("Pressure", D_filt(1, "cutoff"), 0.2).mod("ModWheel", D_osc(0, "fine"), 0.1)
    p.fx("rev", type=1, size=0.6, decay=2.4, mix=0.2)
    macros(p)


# ============================== BRASS & WINDS (25)
@recipe("Brass & Winds", "Brass Section", 8, ["Realistic", "Orchestral"], ["Horns", "Trumpets", "Trombones", "French Horn", "Fanfare", "Big Band", "Swell Brass", "Low Brass"])
def _(p, i, r):
    p.osc(0, wt="Brass", pos=0.4, voices=4, detune=0.1, level=0.6, oct=[0, 0, -1, 0, 0, 0, 0, -1][i])
    p.filt(0, type=LP12, cutoff=r.choice([600, 900]), res=0.15, env=0.35)
    p.env(1, att=round(r.uniform(0.05, 0.15), 2), dec=0.6, sus=0.5)
    p.mod("Env2", D_osc(0, "pos"), 0.4).mod("Velocity", D_filt(0, "env"), 0.15)
    p.env(0, att=round(r.uniform(0.03, 0.12), 2), dec=1, sus=0.9, rel=0.35)
    p.lfo(0, shape=SINE, sync=0, rate=5, delay=0.5, fade=0.5)
    p.mod("Lfo1", D_osc(0, "fine"), 0.05)
    p.fx("rev", type=1, size=0.7, decay=2.5, mix=0.2)
    macros(p)

@recipe("Brass & Winds", "Synth Brass", 7, ["Analog", "80s"], ["Jump", "OB Brass", "Poly Brass", "Retro Brass", "Hollywood", "Arena", "Stab Brass"])
def _(p, i, r):
    p.shape(0, "saw", voices=4, detune=0.22, level=0.55)
    p.shape(1, "saw", fine=8, level=0.45)
    p.filt(0, type=LADDER, cutoff=500, res=0.2, env=0.45)
    p.env(1, att=0.08, dec=0.5, sus=0.4)
    p.env(0, att=0.02, dec=0.8, sus=0.85, rel=0.3)
    p.analog(age=round(r.uniform(0.3, 0.6), 2))
    p.fx("cho", depth=0.4, mix=0.3)
    space(p, r, 0.7, "rev")
    macros(p)

@recipe("Brass & Winds", "Flute", 5, ["Realistic", "Breath"], ["Concert Flute", "Pan Flute", "Shakuhachi", "Recorder", "Airy Flute"])
def _(p, i, r):
    p.set(voiceMode=LEGATO if i != 4 else POLY, glide=0.04)
    p.osc(0, wt="Sine", level=0.7)
    p.osc(1, wt="Tri > Saw", pos=0.0, oct=1, level=0.12)
    p.noise(type=1, level=round(r.uniform(0.08, 0.2), 2), route=F2, color=0.2)
    p.filt(1, type=BP12, cutoff=2500, res=0.3)
    p.set(filterRouting=1)
    p.filt(0, type=LP12, cutoff=4000)
    p.env(0, att=0.06, dec=0.5, sus=0.85, rel=0.15)
    p.lfo(0, shape=SINE, sync=0, rate=5, delay=0.4, fade=0.4)
    p.mod("Lfo1", D_osc(0, "fine"), 0.08).mod("Lfo1", D_AMP, -0.06)
    p.fx("rev", type=1, size=0.6, decay=2.2, mix=0.2)
    macros(p)

@recipe("Brass & Winds", "Reed", 5, ["Realistic", "Woodwind"], ["Clarinet", "Oboe", "Alto Sax", "Harmonica", "Bassoon"])
def _(p, i, r):
    p.set(voiceMode=LEGATO, glide=0.04, polyphony=1)
    p.osc(0, wt="Reed", pos=[0.1, 0.6, 0.9, 0.8, 0.3][i], level=0.7, oct=-1 if i == 4 else 0)
    p.filt(0, type=LP12, cutoff=[2000, 3000, 2500, 3500, 1200][i], res=0.15, env=0.15)
    p.env(1, att=0.05, dec=0.3, sus=0.6)
    p.env(0, att=0.04, dec=0.5, sus=0.85, rel=0.12)
    p.noise(type=1, level=0.05, route=F1)
    p.lfo(0, shape=SINE, sync=0, rate=5.3, delay=0.35, fade=0.4)
    p.mod("Lfo1", D_osc(0, "fine"), 0.08).mod("Pressure", D_filt(0, "cutoff"), 0.2)
    if i == 2: p.fx("dist", type=2, drive=0.15, mix=0.4)
    p.fx("rev", type=0, size=0.5, decay=1.5, mix=0.18)
    macros(p)


# ============================== GUITAR (20)
def karplus(p, r, res, bright):
    p.osc(0, on=0)
    p.noise(type=0, level=0.0, route=F1, color=round(r.uniform(-0.3, 0.3), 2))
    p.env(2, dec=0.03, sus=0.0, dCrv=0.8)
    p.mod("Env3", D_NOISE, 1.0)
    p.filt(0, type=COMBP, cutoff=261.63, key=1.0, res=res, drive=0.25)
    p.filt(1, type=LP12, cutoff=bright)
    p.mod("Velocity", D_filt(1, "cutoff"), 0.15)

@recipe("Guitar", "Clean", 6, ["Realistic", "Plucked String"], ["Clean Strat", "Jazz Box", "Acoustic", "12 String", "Chorus Clean", "Fingerpicked"])
def _(p, i, r):
    karplus(p, r, round(r.uniform(0.93, 0.975), 3), r.choice([3500, 5000]))
    p.env(0, att=0.001, dec=5, sus=0.0, rel=0.5)
    if i == 3: p.osc(1, wt="Saw", oct=1, level=0.0)
    if i == 4: p.fx("cho", depth=0.5, mix=0.45)
    p.fx("rev", type=0, size=0.5, decay=1.6, mix=0.18)
    p.mod("Macro1", D_filt(1, "cutoff"), 0.3)

@recipe("Guitar", "Muted", 4, ["Realistic", "Funk"], ["Palm Mute", "Chicken Pick", "Funk Mute", "Ghost Notes"])
def _(p, i, r):
    karplus(p, r, round(r.uniform(0.7, 0.8), 3), 2500)
    p.env(0, att=0.001, dec=0.25, sus=0.0, rel=0.1)
    p.fx("comp", thresh=-20, ratio=4, makeup=4)
    p.mod("Macro1", D_filt(1, "cutoff"), 0.3)

@recipe("Guitar", "Nylon", 4, ["Realistic", "Acoustic"], ["Spanish", "Bossa", "Classical", "Flamenco"])
def _(p, i, r):
    karplus(p, r, round(r.uniform(0.9, 0.95), 3), 2200)
    p.filt(0, type=COMBP)
    p.env(0, att=0.002, dec=3.5, sus=0.0, rel=0.4)
    p.fx("rev", type=0, size=0.4, decay=1.4, mix=0.15)
    p.mod("Macro1", D_filt(1, "cutoff"), 0.3)

@recipe("Guitar", "Distorted", 6, ["Distorted", "Rock"], ["Crunch", "Power Chord", "Fuzz", "Stack", "Lead Drive", "Grunge"])
def _(p, i, r):
    karplus(p, r, round(r.uniform(0.975, 0.99), 3), 5000)
    p.osc(1, wt="Saw", level=0.15, route=F2)
    p.env(0, att=0.001, dec=4, sus=0.6, rel=0.3)
    p.fx("dist", type=r.choice([2, 5, 1]), drive=round(r.uniform(0.55, 0.85), 2), tone=0.45, mix=1.0)
    p.fx("eq", midf=800, midg=3, highf=7000, highg=-6)
    p.fx("rev", type=0, size=0.4, decay=1.2, mix=0.12)
    p.mod("Macro1", D_filt(1, "cutoff"), 0.3)


# ============================== ARP (35)
def arp_pattern(r, steps, style):
    pat = []
    pools = {"melodic": [0, 0, 2, 4, 7, 4, 2, 9, 7, 11, 14],
             "rhythmic": [0, 0, 0, 7, 0, 12, 0, 5],
             "wide": [0, 7, 14, 4, 11, 2, 9, 16, -5]}
    pool = pools[style]
    for k in range(32):
        on = 1 if k == 0 or r.random() > (0.12 if style != "rhythmic" else 0.25) else 0
        pat.append([r.choice(pool) if k else 0, round(min(1.0, 0.5 + r.random() * 0.45 + (0.12 if k % 4 == 0 else 0)), 2),
                    round(0.4 + r.random() * 0.6, 2), on, 1 if r.random() < 0.08 else 0])
    return pat

@recipe("Arp", "Classic", 12, ["Arp", "Movement", "Synced"], ["Cascade", "Runner", "Spiral", "Escalator", "Ladder", "Gallop", "Ticker", "Pinball", "Stairway", "Waterfall", "Shuttle", "Fountain"])
def _(p, i, r):
    p.shape(0, r.choice(["saw", "square"]), voices=r.choice([1, 3]), detune=0.2, level=0.6)
    p.filt(0, type=LP24, cutoff=r.choice([800, 1500]), res=0.3, env=0.35)
    p.env(1, dec=0.18, sus=0.1)
    p.env(0, att=0.001, dec=0.3, sus=0.3, rel=0.2)
    p.arpeggio(mode=[0, 1, 2, 3, 4, 5, 0, 2, 4, 1, 3, 0][i], rate=r.choice(["1/16", "1/16", "1/8", "1/16 T"]), oct=r.choice([1, 2, 3]),
               gate=round(r.uniform(0.4, 0.8), 2))
    p.lfo(0, shape=SINE, sync=1, div="2 Bars", mode=FREE); p.mod("Lfo1", D_filt(0, "cutoff"), 0.12)
    space(p, r, 0.8, "dly")
    macros(p)

@recipe("Arp", "Melodic", 13, ["Arp", "Melodic", "Scale"], ["Lullaby Box", "Daybreak", "Wanderer", "Origami", "Firefly", "Petals", "Chimes", "Starlight", "Lantern Walk", "Sakura", "Riverside", "Glimmer", "Carousel"])
def _(p, i, r):
    p.osc(0, wt=r.choice(["FM Bell", "Glass", "Tri > Saw", "Chip"]), pos=round(r.uniform(0.1, 0.5), 2), level=0.6)
    p.env(0, att=0.001, dec=0.35, sus=0.0, rel=0.3)
    p.filt(0, type=LP12, cutoff=6000)
    scale = [1, 2, 10, 11, 5, 7, 18, 1, 2, 8, 6, 19, 10][i]
    p.arpeggio(mode=7, rate=r.choice(["1/16", "1/8"]), steps=r.choice([8, 12, 16]), scale=scale, oct=r.choice([1, 2]), gate=0.6)
    p.arp = arp_pattern(r, 16, "melodic")
    space(p, r, 1.0, "both")
    macros(p)

@recipe("Arp", "Bass Arp", 10, ["Arp", "Bass", "Synced"], ["Rolling", "Octave Jump", "Pulse Line", "Night Drive", "Outrun", "Engine", "Motor", "Highway", "Berlin", "Machine"])
def _(p, i, r):
    p.set(polyphony=4)
    p.shape(0, "saw", level=0.7)
    p.sub(shape=0, oct=-1, level=0.4, route=DIRECT)
    p.filt(0, type=LADDER, cutoff=r.choice([300, 500]), res=0.4, env=0.4)
    p.env(1, dec=0.12, sus=0.0)
    p.env(0, att=0.001, dec=0.2, sus=0.2, rel=0.08)
    p.arpeggio(mode=7, rate="1/16", steps=r.choice([8, 16]), oct=1)
    p.arp = arp_pattern(r, 16, "rhythmic")
    p.fx("dist", type=0, drive=0.3, mix=0.5)
    macros(p)


# ============================== SEQUENCE (20)
@recipe("Sequence", "Gated", 10, ["Rhythmic", "Synced", "Movement"], ["Trance Gate", "Chopper", "Stutter", "Pulse Pad", "Heartbeat", "Shutter", "Strobe", "Morse", "Ticktock", "Slicer"])
def _(p, i, r):
    p.osc(0, wt="Analog Saw", voices=5, detune=0.25, level=0.55)
    p.osc(1, wt="Square PWM", pos=0.3, voices=3, detune=0.2, level=0.35, oct=1)
    p.filt(0, type=LP24, cutoff=r.choice([1500, 3000]), res=0.2)
    p.env(0, att=0.05, dec=1, sus=1.0, rel=0.4)
    p.lfo(0, shape=r.choice([SQUARE, PULSE, SAWDN]), sync=1, div=r.choice(["1/16", "1/8", "1/16 D"]), uni=1, smooth=0.05)
    p.mod("Lfo1", D_AMP, -0.95)
    space(p, r, 0.8, "both")
    macros(p)

@recipe("Sequence", "Step Sequence", 10, ["Sequence", "Synced", "Melodic"], ["Nightcall", "Circuit", "Conveyor", "Binary", "Signal", "Relay", "Transit", "Clockwork", "Grid", "Loop Station"])
def _(p, i, r):
    p.shape(0, r.choice(["saw", "square"]), level=0.6)
    p.filt(0, type=LP24, cutoff=700, res=0.45, env=0.45)
    p.env(1, dec=0.15, sus=0.0)
    p.env(0, att=0.001, dec=0.25, sus=0.0, rel=0.15)
    p.arpeggio(mode=7, rate="1/16", steps=16, scale=r.choice([2, 5, 6, 11, 16]), oct=r.choice([1, 2]))
    p.arp = arp_pattern(r, 16, r.choice(["melodic", "wide", "rhythmic"]))
    p.lfo(0, shape=TRI, sync=1, div="4 Bars", mode=FREE); p.mod("Lfo1", D_filt(0, "cutoff"), 0.2)
    space(p, r, 0.8, "dly")
    macros(p)


# ============================== CHORDS (25)
CHORD_TYPES = [("Major", 4, 7), ("Minor", 3, 7), ("Sus2", 2, 7), ("Sus4", 5, 7), ("Maj7", 4, 11), ("Min7", 3, 10),
               ("Add9", 4, 14), ("Power Fifth", 7, 12), ("Min9", 3, 14), ("Fifths", 7, 19)]

@recipe("Chords", "One Finger", 15, ["Chord", "One Finger"], [])
def _(p, i, r):
    name, a, b = CHORD_TYPES[i % len(CHORD_TYPES)]
    p.name = f"{name} Pluck Chord" if i >= 10 else f"{name} Chord"
    tone = ["Saw", "Analog Saw", "FM Bell", "Tri > Saw", "Square PWM"][i % 5]
    for o, semi in zip((0, 1, 2), (0, a, b)):
        p.osc(o, wt=tone, semi=semi, level=0.45, voices=3 if i % 2 else 1, detune=0.15)
    p.filt(0, type=LP24, cutoff=r.choice([1500, 2500, 4000]), res=0.15)
    if i >= 10:
        p.env(0, att=0.001, dec=0.35, sus=0.0, rel=0.25); p.filt(0, env=0.4); p.env(1, dec=0.2, sus=0.0); p.filt(0, cutoff=500)
    else:
        p.env(0, att=0.01, dec=1, sus=0.85, rel=0.6)
    space(p, r, 0.8)
    macros(p)

@recipe("Chords", "Stab", 10, ["Chord", "House", "Stab"], ["Deep House Stab", "Detroit", "Organ Stab", "Rave Chord", "Dub Chord", "Garage Chord", "Lo-Fi Chord", "Techno Stab", "Disco Chord", "Jazz Stab"])
def _(p, i, r):
    _, a, b = CHORD_TYPES[[1, 5, 0, 1, 8, 4, 5, 1, 0, 4][i]]
    for o, semi in zip((0, 1, 2), (0, a, b)):
        p.osc(o, wt=["Saw", "Square PWM", "Organ", "Saw", "Tri > Saw"][i % 5], pos=0.5, semi=semi, level=0.45)
    p.filt(0, type=LP24, cutoff=r.choice([400, 700]), res=0.3, env=0.45)
    p.env(1, dec=0.15, sus=0.0)
    p.env(0, att=0.001, dec=0.3, sus=0.0, rel=0.2)
    if i in (4, 7): p.fx("dly", mode=1, div=DIV["1/8 D"], fb=0.5, mix=0.25)
    if i == 6: p.fx("tape", wow=0.4, flutter=0.3, sat=0.4); p.analog(age=0.6, bits=0.3)
    space(p, r, 0.6, "rev")
    macros(p)


# ============================== DRUMS (30)
@recipe("Drums", "Kick", 8, ["Drum", "Kick"], ["Kick Deep", "Kick Punch", "Kick 909", "Kick Boom", "Kick Tight", "Kick Hard", "Kick Sub", "Kick Click"])
def _(p, i, r):
    p.set(polyphony=1)
    p.osc(0, wt="Sine", level=0.95)
    p.filt(0, on=0)
    p.env(1, dec=round(r.uniform(0.03, 0.08), 3), sus=0.0, dCrv=0.8)
    p.mod("Env2", D_osc(0, "pitch"), round(r.uniform(0.35, 0.5), 2))
    p.env(0, att=0.0, dec=round(r.uniform(0.25, 0.7), 2), sus=0.0, rel=0.05)
    p.noise(type=0, level=0.0, route=DIRECT)
    p.env(2, dec=0.006, sus=0.0)
    p.mod("Env3", D_NOISE, [0.1, 0.25, 0.3, 0.05, 0.3, 0.35, 0.0, 0.5][i])
    p.fx("dist", type=0, drive=round(r.uniform(0.1, 0.4), 2), mix=1.0)
    p.set(osc_note=1)

@recipe("Drums", "Snare & Clap", 8, ["Drum", "Snare"], ["Snare Tight", "Snare Fat", "Snare 808", "Rimshot", "Clap", "Clap Wide", "Snare Lo-Fi", "Snare Rock"])
def _(p, i, r):
    p.osc(0, wt="Sine", level=0.6, oct=1 if i != 3 else 2)
    p.env(1, dec=0.03, sus=0.0)
    p.mod("Env2", D_osc(0, "pitch"), 0.12)
    p.noise(type=0, level=0.8, route=F1)
    p.filt(0, type=HP12 if i not in (4, 5) else BP12, cutoff=r.choice([800, 1500, 2500]), res=0.2)
    p.set(filterRouting=1)
    p.osc(0, route=DIRECT)
    p.env(0, att=0.0, dec=round(r.uniform(0.12, 0.3), 2), sus=0.0, rel=0.08)
    if i in (4, 5):
        p.lfo(0, shape=SAWDN, sync=0, rate=95, mode=ENVMODE); p.mod("Lfo1", D_AMP, -0.6); p.osc(0, on=0)
    if i == 5: p.fx("cho", depth=0.6, mix=0.5)
    if i == 6: p.analog(bits=0.5, rate=0.4)
    p.fx("rev", type=0, size=0.3, decay=0.9, mix=0.15)

@recipe("Drums", "Hats & Cymbals", 7, ["Drum", "Hi-Hat"], ["Hat Closed", "Hat Open", "Hat Tick", "Hat Shaker", "Ride", "Crash", "Hat Metallic"])
def _(p, i, r):
    p.osc(0, wt="FM Metal", pos=0.9, level=0.4 if i in (4, 5, 6) else 0.0, oct=3)
    p.noise(type=0, level=0.8)
    p.filt(0, type=HP24, cutoff=[7000, 6500, 9000, 5000, 4000, 3000, 6000][i], res=0.1)
    p.env(0, att=0.0, dec=[0.06, 0.45, 0.03, 0.12, 1.4, 2.5, 0.2][i], sus=0.0, rel=[0.04, 0.3, 0.02, 0.06, 0.8, 1.5, 0.1][i])
    p.fx("rev", type=0, size=0.3, decay=0.8, mix=0.1)

@recipe("Drums", "Perc & Toms", 7, ["Drum", "Percussion"], ["Tom Low", "Tom High", "Conga", "Bongo", "Cowbell", "Clave", "Laser Perc"])
def _(p, i, r):
    p.osc(0, wt="Sine" if i < 4 else "Square PWM", pos=0.5, level=0.8, oct=[0, 1, 1, 2, 2, 3, 1][i])
    p.env(1, dec=[0.08, 0.06, 0.03, 0.02, 0.01, 0.005, 0.25][i], sus=0.0)
    p.mod("Env2", D_osc(0, "pitch"), [0.15, 0.15, 0.08, 0.08, 0.02, 0.0, 0.6][i])
    if i == 4: p.osc(1, wt="Square PWM", pos=0.5, semi=7, level=0.6, oct=2)
    p.filt(0, type=BP12 if i in (4, 5) else LP12, cutoff=[800, 1500, 2500, 3000, 1600, 2500, 5000][i], res=0.3)
    p.env(0, att=0.0, dec=[0.5, 0.35, 0.2, 0.15, 0.3, 0.08, 0.3][i], sus=0.0, rel=0.08)


# ============================== FX (35)
@recipe("FX", "Riser", 8, ["FX", "Transition", "Build-up"], ["Uplifter", "Takeoff", "Lift Off", "Tension Riser", "Noise Riser", "Rocket", "Ascension", "Countdown"])
def _(p, i, r):
    p.noise(type=0, level=0.6, route=F1)
    p.osc(0, wt="Saw", voices=7, detune=0.4, level=0.3 if i % 2 else 0.0)
    p.filt(0, type=BP12 if i % 3 == 0 else LP24, cutoff=200, res=0.4)
    p.env(2, att=round(r.uniform(4, 8), 1), dec=1, sus=1.0, aCrv=-0.4)
    p.mod("Env3", D_filt(0, "cutoff"), 0.75).mod("Env3", D_osc(0, "pitch"), 0.25)
    p.lfo(0, shape=SINE, sync=1, div="1/16", mode=RETRIG); p.mod("Env3", D_lfo(0), 0.4)
    p.env(0, att=round(r.uniform(2, 6), 1), dec=1, sus=1.0, rel=0.5)
    p.fx("rev", type=1, decay=4, mix=0.3)

@recipe("FX", "Downlifter", 7, ["FX", "Transition"], ["Drop Down", "Falling", "Descent", "Landing", "Collapse", "Sink", "Gravity"])
def _(p, i, r):
    p.noise(type=0, level=0.6, route=F1)
    p.osc(0, wt="Saw", level=0.35, voices=5, detune=0.3)
    p.filt(0, type=LP24, cutoff=12000, res=0.3)
    p.env(2, dec=round(r.uniform(2, 4), 1), sus=0.0, dCrv=0.2)
    p.mod("Env3", D_filt(0, "cutoff"), 0.0)
    p.mod("Env2", D_filt(0, "cutoff"), 0.0)
    p.env(1, att=0.0, dec=round(r.uniform(2, 4), 1), sus=0.0)
    p.filt(0, cutoff=150, env=0.85)
    p.mod("Env2", D_osc(0, "pitch"), 0.4)
    p.env(0, att=0.001, dec=4, sus=0.0, rel=1.0)
    p.fx("rev", type=1, decay=5, mix=0.35)

@recipe("FX", "Impact", 6, ["FX", "Hit", "Cinematic"], ["Boom Hit", "Slam", "Thunder", "Sub Drop", "Braam", "Cannon"])
def _(p, i, r):
    p.osc(0, wt="Sine", level=0.9, oct=-1)
    p.env(1, dec=round(r.uniform(0.6, 2.0), 2), sus=0.0)
    p.mod("Env2", D_osc(0, "pitch"), 0.3)
    p.noise(type=2, level=0.5, route=F1)
    p.filt(0, type=LP24, cutoff=600, env=0.4)
    p.osc(1, wt="Brass", pos=0.8, voices=5, detune=0.3, level=0.5 if i == 4 else 0.0, oct=-1)
    p.env(0, att=0.0, dec=round(r.uniform(2, 5), 1), sus=0.0, rel=1.5)
    p.fx("dist", type=0, drive=0.4, mix=0.6)
    p.fx("rev", type=1, size=0.9, decay=5, mix=0.35)

@recipe("FX", "Noise & Wind", 7, ["FX", "Texture", "Ambient"], ["Wind", "Ocean", "Static", "Radio", "Rain", "Storm", "Vinyl Crackle"])
def _(p, i, r):
    p.osc(0, on=0)
    p.noise(type=[1, 2, 4, 0, 3, 1, 3][i], level=0.8, route=F1, color=[0.0, -0.5, 0.2, 0.0, 0.3, -0.2, 0.0][i])
    p.filt(0, type=BP12 if i in (0, 3, 5) else LP12, cutoff=[800, 600, 2000, 1500, 4000, 700, 3000][i], res=0.4 if i in (0, 5) else 0.1)
    p.lfo(0, shape=SMOOTHRND, sync=0, rate=round(r.uniform(0.1, 0.5), 2), mode=FREE)
    p.mod("Lfo1", D_filt(0, "cutoff"), 0.2)
    p.env(0, att=1.0, dec=1, sus=1.0, rel=2.0)
    p.fx("rev", type=1, decay=4, mix=0.3)

@recipe("FX", "Sci-Fi", 7, ["FX", "Sci-Fi", "Digital"], ["Laser", "Transmission", "Alien", "Teleport", "Robot Talk", "Scanner", "Glitch"])
def _(p, i, r):
    p.osc(0, wt=r.choice(["FM Metal", "Digital Bits", "Wavefold", "Spectral Comb"]), level=0.6)
    p.lfo(0, shape=[SAWDN, SH, SMOOTHRND, SAWUP, SH, TRI, SH][i], sync=0, rate=[8, 12, 0.5, 3, 15, 1, 20][i])
    p.mod("Lfo1", D_osc(0, "pitch"), [0.3, 0.2, 0.1, 0.4, 0.05, 0.15, 0.25][i]).mod("Lfo1", D_osc(0, "pos"), 0.4)
    p.filt(0, type=r.choice([BP12, COMBP, PHASER]), cutoff=1500, res=0.5)
    p.env(0, att=0.01, dec=1, sus=0.9, rel=0.8)
    p.analog(bits=round(r.uniform(0.0, 0.4), 2))
    space(p, r, 1.0, "both")


# ----------------------------------------------------------------------------- build
def main(out):
    bank, names = [], set()
    for cat, sub, count, tags, words, fn in RECIPES:
        for i in range(count):
            seed = int(hashlib.md5(f"{cat}/{sub}/{i}".encode()).hexdigest()[:8], 16)
            r = random.Random(seed)
            base = words[i] if i < len(words) else f"{sub} {i + 1}"
            name = base if (sub in ("Electric Piano", "Piano", "Organ", "Mallets", "Clav", "Harp & Koto", "Ensemble", "Pizzicato",
                                    "Solo", "Brass Section", "Flute", "Reed", "Kick", "Snare & Clap", "Hats & Cymbals",
                                    "Perc & Toms", "Clean", "Muted", "Nylon", "Distorted") or cat in ("Chords",)) else f"{base}"
            p = Preset(name, cat, sub, tags)
            fn(p, i, r)
            p.p.pop("osc_note", None)
            nm = p.name
            k = 2
            while nm in names: nm = f"{p.name} {k}"; k += 1
            names.add(nm); p.name = nm
            pid = f"factory:{cat}/{sub}/{i + 1:02d}".lower().replace(" ", "-").replace("&", "and")
            bank.append(p.to_json(pid))
    cats = {}
    for b in bank: cats[b["category"]] = cats.get(b["category"], 0) + 1
    print(len(bank), "presets", cats)
    with open(out, "w") as f: json.dump(bank, f, separators=(",", ":"))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "Resources/FactoryPresets.raw.json")
