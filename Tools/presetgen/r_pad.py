"""PAD: soft, huge, bright, ambient, warm, dynamic."""
from core import *
from registry import arch

PD = "Pad"


def pad_env(p, r, att=(0.3, 1.5), rel=(1.0, 3.0), sus=(0.8, 1.0)):
    p.env(0, att=rr(r, *att, 2), dec=1.5, sus=rr(r, *sus, 2), rel=rr(r, *rel, 2))


def pad_macros(p, timbre="Osc A WT Pos"):
    macros(p, timbre=timbre, extra=[("Macro 3", "LFO 1 Rate", 0.4), ("Mod Wheel", "Filter 1 Cutoff", 0.25)])


# ------------------------------------------------------------------------------------------- Soft Pad
@arch(PD, "Soft Pad", ["Soft Pad", "Cotton Pad", "Velvet Pad", "Feather Pad", "Cloud Pad", "Silk Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Soft Saw", "Triangle", "Half Sine", "Lowpass Saw", "Trapezoid"]), voices=pick(r, [2, 3, 4]), detune=rr(r, 0.1, 0.25, 2), level=0.6)
    p.osc(1, wt=pick(r, ["Sine", "Triangle"]), oct=pick(r, [-1, 1]), level=rr(r, 0.2, 0.4, 2))
    p.filt(0, type=pick(r, ["LP 12", "LP 6", "LP 24"]), cutoff=rr(r, 900, 2500, 0), res=0.08)
    pad_env(p, r, (0.4, 1.8))
    p.lfo(0, shape="Sine", rate=rr(r, 0.05, 0.25, 2), mode="Free")
    p.mod("LFO 1", "Filter 1 Cutoff", rr(r, 0.04, 0.1, 3))
    chorus(p, r, mix=(0.25, 0.45)); reverb(p, r, "Hall", mix=(0.25, 0.4), decay=(3, 6))
    pad_macros(p)


@arch(PD, "Soft Pad", ["Choir Pad", "Breath Pad", "Air Pad", "Whisper Pad", "Hush Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Choir", "Whisper", "Vowels", "Vowel I-O-U"]), pos=rr(r, 0.1, 0.6, 2), voices=4, detune=0.15, level=0.55)
    p.noise(type=pick(r, ["Breath", "Pink", "Wind"]), level=rr(r, 0.04, 0.1, 3), route="Filter 1")
    p.filt(0, type=pick(r, ["LP 12", "Formant"]), cutoff=rr(r, 1500, 3500, 0), res=0.2, mix=0.8)
    pad_env(p, r, (0.6, 2.0))
    p.lfo(0, shape="Smooth Random", rate=rr(r, 0.1, 0.4, 2), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", rr(r, 0.1, 0.25, 2))
    reverb(p, r, "Hall", mix=(0.3, 0.45), decay=(4, 8))
    pad_macros(p)


@arch(PD, "Soft Pad", ["Felt Pad", "Lullaby Pad", "Nursery Pad", "Glow Pad", "Morning Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Sine", "Octaves", "Even Harmonics", "Glass"]), pos=rr(r, 0.0, 0.5, 2), level=0.6, voices=2, detune=0.08)
    p.osc(1, wt="Triangle", oct=1, level=0.15)
    p.filt(0, type="LP 12", cutoff=rr(r, 1800, 4000, 0))
    pad_env(p, r, (0.3, 1.0))
    p.fx("tape", wow=rr(r, 0.1, 0.25, 2), flutter=0.1, sat=0.2, mix=1.0)
    reverb(p, r, "Hall", mix=(0.25, 0.4))
    pad_macros(p)


# ------------------------------------------------------------------------------------------- Huge Pad
@arch(PD, "Huge Pad", ["Huge Pad", "Titan Pad", "Arena Pad", "Colossus", "Epic Pad", "Mountain Pad"], role="pad")
def _(p, r, i):
    for o, oc in zip((0, 1, 2), (0, -1, 1)):
        p.osc(o, wt=pick(r, ["Saw", "Saw Stack", "Analog Saw", "Double Saw"]), voices=pick(r, [7, 9, 12]), detune=rr(r, 0.3, 0.5, 2), width=1.0, level=0.4, oct=oc)
    p.filt(0, type="LP 24", cutoff=rr(r, 2500, 6000, 0), res=0.1)
    p.filt(1, type="HP 12", cutoff=rr(r, 80, 200, 0))
    pad_env(p, r, (0.2, 1.0), (1.5, 4.0))
    reverb(p, r, pick(r, ["Hall", "Shimmer"]), mix=(0.25, 0.4), size=(0.7, 1.0), decay=(4, 9))
    delay(p, r, mode="Ping-Pong", mix=(0.08, 0.15))
    p.set(spread=rr(r, 0.4, 0.8, 2))
    pad_macros(p, "Osc A Detune")


@arch(PD, "Huge Pad", ["Cinematic Pad", "Trailer Pad", "Wall Pad", "Massive Pad", "Orchestra Pad"], role="pad")
def _(p, r, i):
    p.sample(pick(r, ["Violins", "Cellos", "Violas", "Church Organ", "Soft Pipe Organ"]), level=0.7, osc_off=False)
    p.osc(0, wt=pick(r, ["Strings", "Saw Stack", "Choir"]), voices=7, detune=0.35, level=0.35)
    p.osc(1, wt="Sine", oct=-1, level=0.3)
    p.filt(0, type="LP 24", cutoff=rr(r, 2500, 7000, 0))
    pad_env(p, r, (0.5, 2.0), (2.0, 4.0))
    reverb(p, r, "Hall", mix=(0.3, 0.45), size=(0.8, 1.0), decay=(5, 10))
    pad_macros(p, "Osc A Level")


@arch(PD, "Huge Pad", ["Supernova", "Hyper Pad", "Mega Pad", "Galaxy Pad", "Fortress Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Harmonic Sweep", "Spectral Comb", "Octaves & Fifths", "Prime Harmonics", "Saw Stack"]), pos=rr(r, 0.2, 0.6, 2), voices=9, detune=0.35, level=0.45)
    p.osc(1, wt=pick(r, ["Analog PWM", "Square PWM"]), voices=5, detune=0.3, level=0.35, oct=-1)
    p.sub(shape="Sine", oct=-1, level=0.3)
    p.lfo(0, shape="Triangle", rate=rr(r, 0.05, 0.2, 2), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", 0.2).mod("LFO 1", "Osc B WT Pos", 0.2)
    p.filt(0, type="LP 24", cutoff=rr(r, 3000, 8000, 0), res=0.12)
    pad_env(p, r, (0.3, 1.2), (2.0, 4.0))
    p.fx("dist", type="Soft Clip", drive=0.2, mix=0.4)
    reverb(p, r, "Hall", mix=(0.25, 0.4), size=(0.8, 1.0))
    pad_macros(p)


# ------------------------------------------------------------------------------------------- Bright Pad
@arch(PD, "Bright Pad", ["Glass Pad", "Crystal Pad", "Ice Pad", "Prism Pad", "Diamond Pad", "Frost Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Glass", "FM 1:9 Glass", "FM Chorus Bell", "Octaves", "Harmonic Pairs"]), pos=rr(r, 0.2, 0.7, 2), voices=4, detune=0.12, level=0.55)
    p.osc(1, wt="Sine", oct=1, level=rr(r, 0.15, 0.3, 2))
    p.filt(0, type="LP 12", cutoff=rr(r, 5000, 12000, 0))
    p.filt(1, type="HP 12", cutoff=rr(r, 200, 500, 0))
    pad_env(p, r, (0.3, 1.0))
    p.lfo(0, shape="Sine", rate=rr(r, 0.1, 0.4, 2), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", rr(r, 0.1, 0.25, 2))
    reverb(p, r, "Shimmer", mix=(0.25, 0.4), decay=(4, 8))
    pad_macros(p)


@arch(PD, "Bright Pad", ["Sunrise Pad", "Golden Pad", "Shine Pad", "Halo Pad", "Heaven Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw", "Analog Saw", "Saw Stack"]), voices=7, detune=0.25, level=0.45)
    p.osc(1, wt=pick(r, ["Analog Saw", "Saw"]), voices=5, detune=0.3, level=0.35, oct=1)
    p.filt(0, type="LP 12", cutoff=rr(r, 6000, 14000, 0), res=0.05)
    p.filt(1, type="HP 24", cutoff=rr(r, 250, 600, 0))
    pad_env(p, r, (0.2, 0.8))
    chorus(p, r); reverb(p, r, pick(r, ["Hall", "Shimmer"]), mix=(0.2, 0.35))
    pad_macros(p, "Osc A Detune")


@arch(PD, "Bright Pad", ["Airy Strings Pad", "Bright Strings", "Silver Pad", "Sky Strings", "Light Pad"], role="pad")
def _(p, r, i):
    p.sample(pick(r, ["Violins", "Solo Violin", "Wine Glasses", "Vibraphone"]), level=0.6, osc_off=False, oct=pick(r, [0, 1]))
    p.osc(0, wt=pick(r, ["Glass", "Strings", "Octaves"]), voices=3, detune=0.15, level=0.35)
    p.filt(0, type="LP 12", cutoff=rr(r, 6000, 12000, 0))
    pad_env(p, r, (0.4, 1.5))
    reverb(p, r, "Shimmer", mix=(0.25, 0.4))
    pad_macros(p, "Osc A Level")


# ------------------------------------------------------------------------------------------- Ambient Pad
@arch(PD, "Ambient Pad", ["Ambient Wash", "Drift Pad", "Fog Pad", "Ocean Pad", "Horizon", "Nebula"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Noise Spectra", "Spectral Comb", "Whisper", "Harmonic Sweep", "Glass"]), pos=rr(r, 0.1, 0.8, 2), voices=4, detune=0.2, level=0.5)
    p.noise(type=pick(r, ["Ocean", "Wind", "Rain", "Pink", "Breath"]), level=rr(r, 0.08, 0.2, 2), route="Filter 1")
    p.filt(0, type=pick(r, ["LP 12", "BP 12", "Dual BP"]), cutoff=rr(r, 800, 3000, 0), res=0.2)
    pad_env(p, r, (1.0, 4.0), (2.0, 6.0))
    p.lfo(0, shape="Smooth Random", rate=rr(r, 0.05, 0.2, 2), mode="Free")
    p.lfo(1, shape="Sine", rate=rr(r, 0.03, 0.1, 2), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", 0.3).mod("LFO 2", "Filter 1 Cutoff", 0.15)
    reverb(p, r, pick(r, ["Hall", "Shimmer"]), mix=(0.35, 0.55), decay=(6, 14), size=(0.8, 1.0))
    delay(p, r, div="1/4 D", mix=(0.1, 0.2), fb=(0.4, 0.6))
    pad_macros(p)


@arch(PD, "Ambient Pad", ["Granular Cloud", "Frozen Pad", "Particle Pad", "Dust Pad", "Spore Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Soft Saw", "Choir", "Glass", "Octaves & Fifths"]), voices=3, detune=0.15, level=0.55)
    p.filt(0, type="LP 12", cutoff=rr(r, 2000, 6000, 0))
    pad_env(p, r, (0.8, 3.0), (2.0, 5.0))
    p.fx("grn", size=rr(r, 60, 250, 0), density=rr(r, 10, 40, 1), spray=rr(r, 0.3, 0.8, 2), pitch=pick(r, [0, 12, 0, -12, 7]), prand=rr(r, 0.1, 0.4, 2), rev=rr(r, 0.1, 0.4, 2), mix=rr(r, 0.4, 0.7, 2))
    reverb(p, r, "Hall", mix=(0.3, 0.5), decay=(5, 10))
    pad_macros(p)
    p.add_tags("Granular")


@arch(PD, "Ambient Pad", ["Distant Pad", "Memory Pad", "Tape Pad", "Lost Signal", "Old Radio Pad"], role="pad")
def _(p, r, i):
    p.sample(pick(r, ["Wine Glasses", "Church Organ", "Cellos", "Hand Chimes", "Voice Aah"]), level=0.6, osc_off=True)
    p.filt(0, type="LP 12", cutoff=rr(r, 1500, 4000, 0))
    pad_env(p, r, (0.8, 2.5), (2.0, 4.0))
    p.fx("tape", wow=rr(r, 0.3, 0.6, 2), flutter=0.3, sat=0.4, tone=0.4, mix=1.0)
    p.fx("grn", size=150, density=20, spray=0.5, mix=0.3)
    reverb(p, r, "Hall", mix=(0.35, 0.5), decay=(5, 10))
    pad_macros(p)


# ------------------------------------------------------------------------------------------- Warm Pad
@arch(PD, "Warm Pad", ["Warm Pad", "Analog Pad", "OB Pad", "Vintage Pad", "Brass Pad", "Amber Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Analog Saw", "Saw"]), voices=pick(r, [2, 3]), detune=0.12, level=0.55)
    p.osc(1, wt=pick(r, ["Analog PWM", "Analog Saw"]), voices=2, detune=0.1, level=0.45, fine=rr(r, 5, 10, 1))
    p.filt(0, type=pick(r, ["LP 24", "LP Ladder"]), cutoff=rr(r, 700, 1800, 0), res=rr(r, 0.1, 0.25, 2), env=0.15)
    p.env(1, att=0.5, dec=1.0, sus=0.6)
    pad_env(p, r, (0.3, 1.2))
    p.analog(age=rr(r, 0.1, 0.3, 2), drift=rr(r, 0.2, 0.4, 2))
    chorus(p, r); reverb(p, r, "Hall", mix=(0.2, 0.32))
    pad_macros(p, "Osc B WT Pos")


@arch(PD, "Warm Pad", ["Dark Pad", "Shadow Pad", "Ember Pad", "Night Pad", "Smoke Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Lowpass Saw", "Warm Square", "Soft Saw"]), voices=4, detune=0.15, level=0.55)
    p.sub(shape=pick(r, ["Sine", "Soft Triangle"]), oct=-1, level=0.35)
    p.filt(0, type="LP 24", cutoff=rr(r, 400, 1000, 0), res=0.15)
    pad_env(p, r, (0.6, 2.0), (2.0, 4.0))
    p.lfo(0, shape="Sine", rate=rr(r, 0.05, 0.15, 2), mode="Free")
    p.mod("LFO 1", "Filter 1 Cutoff", 0.08)
    reverb(p, r, "Hall", mix=(0.25, 0.4), damp=(0.6, 0.85))
    pad_macros(p)


@arch(PD, "Warm Pad", ["String Machine", "Solina Pad", "Ensemble Pad", "Polymoog Pad", "Vintage Strings Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw", "Analog Saw"]), level=0.5)
    p.osc(1, wt="Saw", oct=1, level=0.35, fine=5)
    p.filt(0, type="LP 12", cutoff=rr(r, 2000, 5000, 0))
    p.filt(1, type="HP 12", cutoff=150)
    pad_env(p, r, (0.15, 0.6))
    p.fx("cho", depth=0.8, rate=rr(r, 0.4, 0.9, 2), mix=0.6, fb=0.2)
    p.fx("pha", rate=0.15, depth=0.4, mix=0.25)
    reverb(p, r, "Hall", mix=(0.2, 0.3))
    pad_macros(p)


# ------------------------------------------------------------------------------------------- Dynamic Pad
@arch(PD, "Dynamic Pad", ["Pulse Pad", "Gated Pad", "Trance Gate Pad", "Rhythm Pad", "Chopped Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw Stack", "Analog Saw", "Soft Saw"]), voices=5, detune=0.3, level=0.5)
    p.osc(1, wt="Triangle", oct=1, level=0.25)
    p.filt(0, type="LP 24", cutoff=rr(r, 2000, 5000, 0), res=0.12)
    pad_env(p, r, (0.05, 0.4))
    p.set(motion_amt=rr(r, 0.6, 0.95, 2), motion_shape=pick(r, ["Trance Gate", "Gate", "Pump", "Stutter"]), motion_rate=pick(r, ["1/16", "1/8", "1/8 T", "1/16 D"]))
    delay(p, r, div="1/8 D"); reverb(p, r, "Hall", mix=(0.15, 0.3))
    pad_macros(p)
    p.add_tags("Rhythmic")


@arch(PD, "Dynamic Pad", ["Sweep Pad", "Filter Sweep", "Evolving Pad", "Breathing Pad", "Tide Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw", "Analog PWM", "Harmonic Sweep", "Vowels"]), voices=4, detune=0.2, level=0.55)
    p.osc(1, wt="Saw", oct=-1, level=0.3, fine=7)
    p.filt(0, type=pick(r, ["LP 24", "BP 12", "Phaser", "Comb +"]), cutoff=rr(r, 500, 1500, 0), res=rr(r, 0.3, 0.55, 2))
    p.lfo(0, shape=pick(r, ["Sine", "Triangle", "Swell", "S-Curve Up"]), div=pick(r, ["2 Bars", "4 Bars", "1 Bar", "8 Bars"]), mode="Free")
    p.mod("LFO 1", "Filter 1 Cutoff", rr(r, 0.25, 0.45, 2)).mod("LFO 1", "Osc A WT Pos", 0.2)
    pad_env(p, r, (0.3, 1.5))
    reverb(p, r, "Hall", mix=(0.25, 0.4))
    pad_macros(p)


@arch(PD, "Dynamic Pad", ["Morph Pad", "Shifting Pad", "Kaleido Pad", "Mutation Pad", "Living Pad"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Vowel A-E", "Talking", "Prime Harmonics", "Digital Bits", "Spectral Comb", "Fold Sine"]), pos=0.0, voices=3, detune=0.15, level=0.5)
    p.osc(1, wt=pick(r, ["Glass", "Octaves", "Choir"]), voices=3, detune=0.1, level=0.35)
    p.lfo(0, shape="Triangle", div=pick(r, ["1 Bar", "2 Bars"]), mode="Free")
    p.lfo(1, shape="Smooth Random", rate=rr(r, 0.1, 0.5, 2), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", 0.5).mod("LFO 2", "Osc B WT Pos", 0.3).mod("LFO 2", "Osc A Pan", 0.4)
    p.filt(0, type="LP 12", cutoff=rr(r, 2500, 6000, 0))
    pad_env(p, r, (0.4, 1.5))
    p.set(chaos_amt=rr(r, 0.1, 0.3, 2), chaos_speed=rr(r, 0.2, 1.0, 2), chaos_seed=r.randrange(1, 9999))
    reverb(p, r, "Hall", mix=(0.25, 0.4))
    pad_macros(p)
