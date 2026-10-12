"""SYNTH: soft lead, hard lead, pluck, polysynth, synth keys, chords."""
from core import *
from registry import arch

S = "Synth"
MOODS = PARAMS["chord_mood"]["choices"][1:]


def legato_lead(p, r, glide=(0.03, 0.1)):
    p.set(voiceMode="Legato", polyphony=1, glide=rr(r, *glide, 3), glideLegato=1)


# ------------------------------------------------------------------------------------------- Soft Lead
@arch(S, "Soft Lead", ["Sine Lead", "Whistle Lead", "Flute Lead", "Soft Lead", "Breath Lead", "Glide Lead"], role="lead")
def _(p, r, i):
    legato_lead(p, r)
    p.shape(0, pick(r, ["sine", "tri"]), level=0.8)
    if r.random() < 0.6: p.shape(1, "tri", oct=1, level=rr(r, 0.08, 0.2, 2))
    p.noise(type=pick(r, ["Breath", "Pink", "Wind"]), level=rr(r, 0.03, 0.1, 3), route="Filter 1")
    p.filt(0, type="LP 12", cutoff=rr(r, 2000, 6000, 0), res=0.1, key=0.4)
    p.env(0, att=rr(r, 0.02, 0.08, 3), dec=0.5, sus=0.85, rel=rr(r, 0.2, 0.6, 2))
    vibrato(p, r, cents=(8, 20), oscs=(0, 1))
    delay(p, r, mix=(0.12, 0.22))
    macros(p, extra=[("Macro 2", "Noise Level", 0.25), ("Mod Wheel", "LFO 4 Rate", 0.2)])


@arch(S, "Soft Lead", ["Vocal Lead", "Talk Lead", "Vowel Lead", "Mouth Lead", "Chant Lead"], role="lead")
def _(p, r, i):
    legato_lead(p, r)
    p.osc(0, wt=pick(r, ["Vowels", "Vowel A-E", "Vowel I-O-U", "Choir", "Whisper"]), pos=rr(r, 0.1, 0.5, 2), voices=2, detune=0.08, level=0.8)
    p.filt(0, type="Formant", cutoff=rr(r, 600, 1800, 0), res=rr(r, 0.3, 0.6, 2), mix=0.7)
    p.lfo(0, shape=pick(r, ["Sine", "Triangle", "Smooth Random"]), rate=rr(r, 0.2, 1.2, 2), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", rr(r, 0.2, 0.4, 2)).mod("LFO 1", "Filter 1 Cutoff", 0.1)
    p.env(0, att=0.03, dec=0.5, sus=0.85, rel=0.4)
    vibrato(p, r, cents=(10, 20))
    reverb(p, r, "Hall", mix=(0.15, 0.25))
    macros(p, timbre="Osc A WT Pos")


@arch(S, "Soft Lead", ["Chip Lead", "8-Bit Lead", "Arcade Lead", "Pixel Lead", "Square Lead"], role="lead")
def _(p, r, i):
    p.set(voiceMode=pick(r, ["Mono", "Legato"]), polyphony=1, glide=pick(r, [0.0, 0.03]))
    p.osc(0, wt=pick(r, ["Chip", "Square", "Glitch Steps", "Bit Sine"]), pos=rr(r, 0.0, 0.4, 2), level=0.6)
    p.filt(0, on=0)
    p.env(0, att=0.0, dec=0.3, sus=0.7, rel=0.05)
    p.lfo(0, shape="Square", div="1/32", delay=rr(r, 0.15, 0.35, 2))
    p.mod("LFO 1", "Osc A Pitch", pick(r, [0.0, SEMI * 12 / 24, SEMI]))
    p.analog(bits=rr(r, 0.2, 0.45, 2), rate=rr(r, 0.0, 0.3, 2))
    delay(p, r, div="1/8", mode="Digital", mix=(0.1, 0.18))
    macros(p, tone=False, timbre="Osc A WT Pos")
    p.add_tags("Retro")


@arch(S, "Soft Lead", ["Ocarina Lead", "Pan Lead", "Bamboo Lead", "Air Lead", "Pipe Lead"], role="lead")
def _(p, r, i):
    legato_lead(p, r, (0.02, 0.06))
    p.osc(0, wt=pick(r, ["Half Sine", "Sine > Square", "Trapezoid", "Reed"]), pos=rr(r, 0.0, 0.3, 2), level=0.75)
    p.noise(type="Breath", level=rr(r, 0.06, 0.15, 3), route="Filter 1")
    p.env(2, att=0.0, dec=0.12, sus=0.0)
    p.mod("Env 3", "Noise Level", 0.3)
    p.filt(0, type=pick(r, ["LP 12", "BP 12"]), cutoff=rr(r, 1500, 4000, 0), res=0.15, key=0.5)
    p.env(0, att=rr(r, 0.03, 0.08, 3), dec=0.4, sus=0.9, rel=0.25)
    vibrato(p, r, cents=(6, 15), delay=(0.3, 0.7))
    reverb(p, r, "Hall", mix=(0.15, 0.3))
    macros(p, extra=[("Macro 2", "Noise Level", 0.3)])


@arch(S, "Soft Lead", ["Silk Lead", "Saw Lead Soft", "Dream Lead", "Mellow Lead", "Satin Lead"], role="lead")
def _(p, r, i):
    legato_lead(p, r)
    p.osc(0, wt=pick(r, ["Soft Saw", "Lowpass Saw", "Warm Square"]), voices=pick(r, [1, 2, 3]), detune=0.1, level=0.7)
    p.osc(1, wt="Sine", oct=-1, level=0.3)
    p.filt(0, type=pick(r, ["LP 12", "LP 24"]), cutoff=rr(r, 1200, 3000, 0), res=0.15, env=0.15, key=0.4)
    p.env(1, dec=0.6, sus=0.4)
    p.env(0, att=0.02, dec=0.6, sus=0.85, rel=0.5)
    vibrato(p, r)
    chorus(p, r, mix=(0.2, 0.35)); delay(p, r, mix=(0.1, 0.2))
    macros(p, extra=[("Mod Wheel", "Filter 1 Cutoff", 0.2)])


# ------------------------------------------------------------------------------------------- Hard Lead
@arch(S, "Hard Lead", ["Supersaw Lead", "Trance Lead", "Anthem Lead", "Festival Lead", "Euphoria", "Mainstage Lead"], role="lead")
def _(p, r, i):
    for o, oc in zip((0, 1, 2), (0, pick(r, [-1, 1, 0]), 0)):
        p.osc(o, wt=pick(r, ["Saw", "Analog Saw", "Saw Stack"]), voices=pick(r, [7, 9, 11]), detune=rr(r, 0.3, 0.5, 2), blend=0.8, width=1.0, level=0.45, oct=oc)
    if r.random() < 0.5: p.osc_off(2)
    p.filt(0, type="LP 24", cutoff=rr(r, 5000, 12000, 0), res=0.1)
    p.filt(1, type="HP 12", cutoff=rr(r, 100, 250, 0))
    p.env(0, att=0.003, dec=0.6, sus=0.85, rel=rr(r, 0.3, 0.7, 2))
    reverb(p, r, "Hall", mix=(0.15, 0.28)); delay(p, r, div=pick(r, ["1/8 D", "1/4 D"]), mode="Ping-Pong")
    macros(p, timbre="Osc A Detune")


@arch(S, "Hard Lead", ["Sync Lead", "Sync Rip", "Cutter", "Laser Lead", "Tear Lead", "Jet Lead"], role="lead")
def _(p, r, i):
    legato_lead(p, r, (0.02, 0.05))
    p.osc(0, wt=pick(r, ["Hard Sync", "Square Sync"]), pos=0.1, level=0.8, voices=pick(r, [1, 2, 3]), detune=0.1)
    p.env(2, att=0.0, dec=rr(r, 0.4, 1.2, 2), sus=rr(r, 0.1, 0.4, 2))
    p.mod("Env 3", "Osc A WT Pos", rr(r, 0.5, 0.8, 2))
    p.lfo(0, shape="Triangle", rate=rr(r, 0.3, 1.5, 2))
    p.mod("LFO 1", "Osc A WT Pos", 0.15)
    p.filt(0, type="LP 24", cutoff=6000, res=0.15)
    if r.random() < 0.6: p.fx("dist", type="Soft Clip", drive=0.3, mix=0.5)
    delay(p, r)
    macros(p, timbre="Osc A WT Pos")


@arch(S, "Hard Lead", ["Screamer", "Banshee", "Siren Lead", "Riot Lead", "Distorted Lead", "Shred Lead"], role="lead")
def _(p, r, i):
    legato_lead(p, r, (0.03, 0.07))
    p.shape(0, "saw", voices=3, detune=0.15, level=0.7)
    p.shape(1, "square", oct=1, level=0.35)
    p.filt(0, type=pick(r, ["BP 24", "Peak", "LP Ladder", "MS-20 LP"]), cutoff=rr(r, 1500, 2800, 0), res=0.6, drive=0.6)
    p.fx("dist", type=pick(r, ["Tube", "Overdrive", "Hard Clip", "Diode"]), drive=rr(r, 0.55, 0.85, 2), tone=0.6, mix=1.0)
    p.mod("Mod Wheel", "Filter 1 Cutoff", 0.25).mod("Velocity", "Filter 1 Cutoff", 0.15)
    vibrato(p, r, cents=(10, 25), delay=(0.3, 0.6))
    reverb(p, r, "Hall", mix=(0.12, 0.2)); delay(p, r)
    macros(p)


@arch(S, "Hard Lead", ["Big Room Lead", "EDM Stab Lead", "Hands Up Lead", "Rave Lead", "Hoover Lead"], role="lead")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw Stack", "Phaser Saw", "Double Saw"]), voices=pick(r, [5, 7]), detune=rr(r, 0.3, 0.55, 2), level=0.55)
    p.osc(1, wt=pick(r, ["Square PWM", "Analog PWM"]), pos=0.3, voices=3, detune=0.3, level=0.4, oct=-1)
    p.filt(0, type="LP 24", cutoff=rr(r, 3000, 9000, 0), res=0.15, env=0.25)
    p.env(1, dec=0.35, sus=0.3)
    p.env(0, att=0.002, dec=0.5, sus=0.75, rel=0.25)
    if r.random() < 0.5: p.set(glide=0.06)
    p.fx("dist", type="Soft Clip", drive=rr(r, 0.2, 0.4, 2), mix=0.6)
    reverb(p, r, "Hall", mix=(0.15, 0.25))
    macros(p, timbre="Osc A Detune")


@arch(S, "Hard Lead", ["FM Lead", "Digital Lead", "Metal Lead", "Chrome Lead", "DX Lead"], role="lead")
def _(p, r, i):
    legato_lead(p, r, (0.0, 0.05))
    p.osc(0, wt=pick(r, ["FM 3:1", "FM 2:1", "FM Feedback", "FM Metal"]), pos=rr(r, 0.2, 0.6, 2), level=0.7)
    p.osc(1, wt="Sine", oct=pick(r, [1, 2]), level=0.0)
    p.osc(0, fmSrc="Osc B", fmAmt=rr(r, 0.15, 0.4, 2))
    p.env(1, dec=rr(r, 0.3, 0.9, 2), sus=rr(r, 0.2, 0.5, 2))
    p.mod("Env 2", "Osc A FM", 0.3).mod("Mod Wheel", "Osc A FM", 0.3)
    p.filt(0, type="LP 12", cutoff=8000, res=0.05)
    vibrato(p, r)
    delay(p, r, mode="Digital")
    macros(p, timbre="Osc A FM")


# ------------------------------------------------------------------------------------------- Pluck
@arch(S, "Pluck", ["Synth Pluck", "House Pluck", "Pluck", "Tropical Pluck", "Pop Pluck", "Club Pluck"], role="lead")
def _(p, r, i):
    p.shape(0, pick(r, ["saw", "square", "saw"]), voices=pick(r, [1, 3, 5]), detune=rr(r, 0.1, 0.25, 2), level=0.7)
    if r.random() < 0.5: p.shape(1, "square", oct=1, level=0.25)
    p.filt(0, type=pick(r, ["LP 24", "LP 12", "LP Ladder"]), cutoff=rr(r, 250, 700, 0), res=rr(r, 0.15, 0.4, 2), env=rr(r, 0.45, 0.7, 2))
    p.env(1, att=0.0, dec=rr(r, 0.12, 0.35, 2), sus=0.0)
    p.env(0, att=0.0, dec=rr(r, 0.25, 0.7, 2), sus=0.0, rel=rr(r, 0.15, 0.4, 2))
    p.mod("Velocity", "Filter 1 Env Amt", 0.2)
    delay(p, r, div=pick(r, ["1/8 D", "1/4", "1/8"]))
    reverb(p, r, "Hall", mix=(0.1, 0.2))
    macros(p, extra=[("Macro 2", "Filter 1 Env Amt", 0.3)])


@arch(S, "Pluck", ["Bell Pluck", "FM Pluck", "Glass Pluck", "Crystal Pluck", "Chime Pluck"], role="lead")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM Bell", "FM Chorus Bell", "FM 1:9 Glass", "Glass", "Octaves"]), pos=rr(r, 0.1, 0.6, 2), level=0.7)
    p.osc(1, wt="Sine", oct=1, level=rr(r, 0.1, 0.25, 2))
    p.env(2, att=0.0, dec=rr(r, 0.2, 0.6, 2), sus=0.0)
    p.mod("Env 3", "Osc A WT Pos", rr(r, 0.2, 0.4, 2))
    p.filt(0, type="LP 12", cutoff=rr(r, 4000, 10000, 0))
    p.env(0, att=0.0, dec=rr(r, 0.4, 1.2, 2), sus=0.0, rel=rr(r, 0.4, 1.0, 2))
    delay(p, r, mode="Ping-Pong"); reverb(p, r, pick(r, ["Hall", "Shimmer"]), mix=(0.15, 0.3))
    macros(p, timbre="Osc A WT Pos")


@arch(S, "Pluck", ["Dreamy Pluck", "Lofi Pluck", "Soft Pluck", "Mallet Pluck", "Kalimba Pluck"], role="lead")
def _(p, r, i):
    p.shape(0, pick(r, ["tri", "sine"]), level=0.8)
    p.osc(1, wt=pick(r, ["Glass", "Half Sine", "Octaves"]), level=0.3, oct=1)
    p.filt(0, type="LP 12", cutoff=rr(r, 1500, 4000, 0), res=0.1, env=0.2)
    p.env(1, dec=0.2, sus=0.0)
    p.env(0, att=0.0, dec=rr(r, 0.4, 0.9, 2), sus=0.0, rel=0.4)
    chorus(p, r, mix=(0.2, 0.4)); delay(p, r, div="1/4 D"); reverb(p, r, "Hall", mix=(0.18, 0.3))
    macros(p)


@arch(S, "Pluck", ["Koto Pluck", "Harp Pluck", "String Pluck", "Zither Pluck", "Nylon Pluck"], role="lead")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["PD Reso", "Spectral Comb", "Harmonic Sweep", "Strings"]), pos=rr(r, 0.2, 0.6, 2), level=0.75)
    p.noise(type="White", level=0.15, route="Filter 1")
    p.env(2, att=0.0, dec=0.02, sus=0.0)
    p.mod("Env 3", "Noise Level", 0.6)
    p.filt(0, type=pick(r, ["Comb +", "LP 12", "Resonator"]), cutoff=rr(r, 1000, 3000, 0), res=rr(r, 0.4, 0.7, 2), key=1.0, mix=0.7)
    p.env(0, att=0.0, dec=rr(r, 0.6, 1.4, 2), sus=0.0, rel=0.5)
    reverb(p, r, "Room", mix=(0.12, 0.2))
    macros(p)


@arch(S, "Pluck", ["Stab Pluck", "Future Pluck", "Wobble Pluck", "Chord Pluck", "Bounce Pluck"], role="lead")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw Stack", "Double Saw", "Analog Saw"]), voices=pick(r, [5, 7]), detune=0.3, level=0.6)
    p.filt(0, type="LP 24", cutoff=rr(r, 400, 1000, 0), res=0.2, env=0.55)
    p.env(1, dec=rr(r, 0.15, 0.3, 2), sus=0.0)
    p.env(0, att=0.0, dec=rr(r, 0.3, 0.6, 2), sus=0.0, rel=0.2)
    if r.random() < 0.5:
        p.lfo(0, shape="Sine", div=pick(r, ["1/8", "1/16"]), mode="Retrig")
        p.mod("LFO 1", "Filter 1 Cutoff", 0.15)
    p.fx("comp", thresh=-18, ratio=4, makeup=3)
    reverb(p, r, "Hall", mix=(0.12, 0.22))
    macros(p)


# ------------------------------------------------------------------------------------------- Polysynth
@arch(S, "Polysynth", ["Juno Poly", "80s Poly", "Analog Poly", "Poly Saw", "Retro Poly", "Oberheim Poly"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Analog Saw", "Saw"]), level=0.6)
    p.osc(1, wt=pick(r, ["Analog PWM", "Square PWM"]), pos=0.3, level=0.45, fine=rr(r, 4, 9, 1))
    p.lfo(0, shape="Triangle", rate=rr(r, 0.3, 0.9, 2), mode="Free")
    p.mod("LFO 1", "Osc B WT Pos", 0.15)
    p.filt(0, type=pick(r, ["LP 24", "LP Ladder"]), cutoff=rr(r, 1200, 3500, 0), res=rr(r, 0.1, 0.3, 2), env=rr(r, 0.15, 0.35, 2), key=0.3)
    p.env(1, att=0.0, dec=rr(r, 0.3, 0.8, 2), sus=0.3)
    p.env(0, att=rr(r, 0.003, 0.03, 3), dec=0.8, sus=0.8, rel=rr(r, 0.3, 0.8, 2))
    chorus(p, r, mix=(0.3, 0.5)); reverb(p, r, "Hall", mix=(0.1, 0.2))
    macros(p, timbre="Osc B WT Pos")


@arch(S, "Polysynth", ["Hyper Poly", "Super Poly", "Modern Poly", "Wide Poly", "Stack Poly"], role="pad")
def _(p, r, i):
    for o in (0, 1):
        p.osc(o, wt=pick(r, ["Saw Stack", "Double Saw", "Saw", "Phaser Saw"]), voices=pick(r, [5, 7, 9]), detune=rr(r, 0.2, 0.4, 2), level=0.45, oct=[0, pick(r, [0, 1, -1])][o])
    p.filt(0, type="LP 24", cutoff=rr(r, 2500, 7000, 0), res=0.1, env=0.2)
    p.env(1, dec=0.6, sus=0.5)
    p.env(0, att=0.005, dec=1.0, sus=0.85, rel=0.6)
    reverb(p, r, "Hall", mix=(0.15, 0.25)); delay(p, r, mode="Ping-Pong")
    macros(p, timbre="Osc A Detune")


@arch(S, "Polysynth", ["Brassy Poly", "Prophet Poly", "Poly Stab", "Sync Poly", "Bright Poly"], role="pad")
def _(p, r, i):
    p.shape(0, "saw", level=0.6)
    p.osc(1, wt=pick(r, ["Hard Sync", "Square Sync"]), pos=rr(r, 0.2, 0.5, 2), level=0.4)
    p.filt(0, type="LP 24", cutoff=rr(r, 900, 2200, 0), res=0.2, env=rr(r, 0.35, 0.55, 2))
    p.env(1, att=rr(r, 0.01, 0.06, 3), dec=rr(r, 0.3, 0.7, 2), sus=0.4)
    p.env(0, att=0.005, dec=0.6, sus=0.75, rel=0.4)
    p.mod("Velocity", "Filter 1 Env Amt", 0.2)
    p.analog(drift=0.2)
    chorus(p, r, mix=(0.2, 0.35)); reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p, timbre="Osc B WT Pos")


@arch(S, "Polysynth", ["Wavetable Poly", "Digital Poly", "Morph Poly", "Vector Poly", "PPG Poly"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Harmonic Sweep", "Digital Bits", "PD Saw", "Spectral Comb", "Octaves & Fifths", "Prime Harmonics", "Glitch Steps"]), pos=rr(r, 0.1, 0.5, 2), level=0.6, voices=3, detune=0.15)
    p.osc(1, wt=pick(r, ["Saw", "Analog PWM", "Triangle"]), level=0.35, fine=6)
    p.lfo(0, shape=pick(r, ["Triangle", "Sine", "Smooth Random"]), rate=rr(r, 0.1, 0.6, 2), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", rr(r, 0.15, 0.35, 2))
    p.filt(0, type="LP 12", cutoff=rr(r, 2500, 8000, 0), res=0.15)
    p.env(0, att=0.01, dec=1.0, sus=0.8, rel=0.6)
    reverb(p, r, "Hall", mix=(0.12, 0.22))
    macros(p, timbre="Osc A WT Pos")


# ------------------------------------------------------------------------------------------- Synth Keys
@arch(S, "Synth Keys", ["FM Keys", "DX Keys", "Digital Keys", "Tine Keys", "Glass Keys"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM E-Piano", "FM 1:9 Glass", "FM Chorus Bell", "FM 2:1"]), pos=rr(r, 0.1, 0.5, 2), level=0.7)
    p.osc(1, wt="Sine", level=rr(r, 0.2, 0.4, 2))
    p.env(2, att=0.0, dec=rr(r, 0.3, 0.8, 2), sus=0.1)
    p.mod("Env 3", "Osc A WT Pos", 0.3).mod("Velocity", "Osc A WT Pos", 0.25)
    p.filt(0, type="LP 12", cutoff=rr(r, 3000, 9000, 0))
    p.env(0, att=0.0, dec=rr(r, 1.5, 3.5, 2), sus=rr(r, 0.0, 0.3, 2), rel=0.4)
    chorus(p, r, mix=(0.2, 0.4)); reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p, timbre="Osc A WT Pos")


@arch(S, "Synth Keys", ["Synth Piano", "Hybrid Keys", "Analog Keys", "Soft Keys", "Plucked Keys"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Piano", "Half Sine", "Trapezoid", "Soft Saw"]), pos=rr(r, 0.1, 0.5, 2), level=0.7)
    p.shape(1, "tri", oct=1, level=0.25)
    p.filt(0, type="LP 24", cutoff=rr(r, 1200, 3500, 0), res=0.1, env=0.35, key=0.5)
    p.env(1, dec=rr(r, 0.4, 1.0, 2), sus=0.1)
    p.env(0, att=0.0, dec=rr(r, 1.0, 2.5, 2), sus=0.0, rel=0.4)
    velocity_tone(p, 0.25)
    reverb(p, r, "Room", mix=(0.1, 0.18))
    macros(p)


@arch(S, "Synth Keys", ["Hybrid Wurli", "Synth EP", "Layer Keys", "EP Pad Keys", "Electro Keys"], role="keys")
def _(p, r, i):
    p.sample(pick(r, ["Wurlitzer", "Electric Grand", "FM E-Piano", "Pianet"]), level=0.75, osc_off=False)
    p.osc(0, wt=pick(r, ["Soft Saw", "Analog PWM", "Glass"]), level=0.25, voices=3, detune=0.15)
    p.env(0, att=0.0, dec=2.0, sus=0.4, rel=0.5)
    p.filt(0, type="LP 12", cutoff=rr(r, 4000, 10000, 0))
    velocity_tone(p, 0.15)
    chorus(p, r); reverb(p, r, "Hall", mix=(0.1, 0.2))
    macros(p, extra=[("Macro 2", "Osc A Level", 0.3)])


@arch(S, "Synth Keys", ["Clavinet Synth", "Pulse Keys", "Funk Keys", "Wah Keys", "Stab Keys"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Square PWM", "Analog PWM"]), pos=rr(r, 0.6, 0.9, 2), level=0.7)
    p.filt(0, type=pick(r, ["BP 12", "LP 24", "Peak"]), cutoff=rr(r, 600, 1500, 0), res=rr(r, 0.3, 0.6, 2), env=0.4)
    p.env(1, dec=0.18, sus=0.1)
    p.env(0, att=0.0, dec=rr(r, 0.3, 0.6, 2), sus=0.1, rel=0.1)
    p.mod("Velocity", "Filter 1 Env Amt", 0.25)
    if r.random() < 0.5: p.fx("pha", rate=0.4, depth=0.6, mix=0.4)
    macros(p)


# ------------------------------------------------------------------------------------------- Chords
def chord_sound(p, r, kind):
    if kind == "piano":
        p.sample(pick(r, ["Grand Piano", "Upright Piano", "Vintage Piano", "Concert Grand"]), level=0.85)
        p.env(0, att=0.0, dec=2.0, sus=1.0, rel=0.4); reverb(p, r, "Hall", mix=(0.1, 0.2))
    elif kind == "ep":
        p.sample(pick(r, ["Wurlitzer", "Electric Grand", "Pianet", "FM E-Piano"]), level=0.85)
        p.env(0, att=0.0, dec=2.0, sus=1.0, rel=0.3); chorus(p, r); reverb(p, r, "Room", mix=(0.08, 0.16))
    elif kind == "organ":
        p.osc(0, wt=pick(r, ["Organ", "Octaves & Fifths", "Octaves"]), pos=rr(r, 0.2, 0.7, 2), level=0.6)
        p.env(0, att=0.005, dec=0.5, sus=1.0, rel=0.1); chorus(p, r, rate=(3.0, 5.5))
    elif kind == "stab":
        p.osc(0, wt=pick(r, ["Saw Stack", "Analog Saw", "Double Saw"]), voices=5, detune=0.3, level=0.5)
        p.filt(0, type="LP 24", cutoff=rr(r, 800, 2500, 0), res=0.2, env=0.4); p.env(1, dec=0.25, sus=0.1)
        p.env(0, att=0.0, dec=0.4, sus=0.0, rel=0.25); reverb(p, r, "Hall", mix=(0.12, 0.22))
    elif kind == "pad":
        p.osc(0, wt=pick(r, ["Soft Saw", "Strings", "Choir", "Glass"]), voices=4, detune=0.2, level=0.5)
        p.filt(0, type="LP 12", cutoff=rr(r, 1500, 4000, 0)); p.env(0, att=rr(r, 0.2, 0.8, 2), dec=1.0, sus=0.9, rel=1.2)
        reverb(p, r, "Hall", mix=(0.2, 0.35))
    elif kind == "guitar":
        p.sample(pick(r, ["Classical Guitar", "Clean Electric Guitar", "Twang Guitar", "Archtop Guitar"]), level=0.85)
        p.env(0, att=0.0, dec=2.0, sus=1.0, rel=0.3); reverb(p, r, "Room", mix=(0.1, 0.18))
    else:   # pluck
        p.shape(0, "saw", voices=3, detune=0.15, level=0.6)
        p.filt(0, type="LP 24", cutoff=rr(r, 400, 900, 0), res=0.2, env=0.5); p.env(1, dec=0.25, sus=0.0)
        p.env(0, att=0.0, dec=0.5, sus=0.0, rel=0.3); delay(p, r); reverb(p, r, "Hall", mix=(0.1, 0.2))


@arch(S, "Chords", ["Progression Keys", "Song Starter", "Chord Machine", "Progression", "Writer's Chords"], role="keys")
def _(p, r, i):
    kind = pick(r, ["piano", "ep", "pad", "organ", "guitar"])
    chord_sound(p, r, kind)
    p.chord_gen(mode="Progression", mood=pick(r, MOODS), scale=pick(r, ["Major", "Minor", "Dorian", "Mixolydian"]), count=pick(r, [4, 4, 4, 8]),
                len=pick(r, ["1 Bar", "2 Bars"]), size=pick(r, ["Triads", "Sevenths", "Mixed", "Ninths"]), voicing=pick(r, ["Open", "Close", "Spread"]),
                rhythm=pick(r, ["Sustain", "Sustain", "1/4 Pulse", "Offbeats", "Syncopated"]), human=rr(r, 0.1, 0.3, 2), complex=rr(r, 0.2, 0.7, 2))
    p.add_tags("Chords", kind.title())
    macros(p)


@arch(S, "Chords", ["One-Finger Chords", "Chord Memory", "Chord Stab", "Minor Stab", "House Chord"], role="keys")
def _(p, r, i):
    kind = pick(r, ["stab", "organ", "pluck", "ep"])
    chord_sound(p, r, kind)
    p.chord_gen(mode="Chord per Key", scale=pick(r, ["Minor", "Major", "Dorian"]), size=pick(r, ["Sevenths", "Ninths", "Triads", "Sus"]),
                voicing=pick(r, ["Open", "Close", "Drop 2"]), bass=pick(r, [0, 1]))
    p.add_tags("Chords")
    macros(p)


@arch(S, "Chords", ["Strummed Chords", "Rhythm Chords", "Pulse Chords", "Gated Chords", "Groove Chords"], role="keys")
def _(p, r, i):
    kind = pick(r, ["guitar", "piano", "pluck", "ep", "stab"])
    chord_sound(p, r, kind)
    p.chord_gen(mode="Progression", mood=pick(r, MOODS), scale=pick(r, ["Major", "Minor", "Minor Pentatonic", "Dorian"]), count=4,
                len=pick(r, ["1 Bar", "1/2"]), size=pick(r, ["Triads", "Sevenths", "Power", "Sus"]), rhythm=pick(r, ["1/8 Pulse", "Syncopated", "Charleston", "Stabs", "Offbeats"]),
                strum=rr(r, 0.1, 0.5, 2) if kind == "guitar" else rr(r, 0.0, 0.15, 2), gate=rr(r, 0.4, 0.8, 2), swing=rr(r, 0.0, 0.4, 2), human=0.2)
    p.add_tags("Chords", "Rhythmic")
    macros(p)
