"""BASS: synth, electric, acoustic, reese, hard, soft, sub, acid, 808."""
from core import *
from registry import arch

B = "Bass"


def mono(p, r, glide=(0.02, 0.08), legato=True):
    p.set(voiceMode="Legato" if legato else "Mono", polyphony=1, glide=rr(r, *glide, 3), glideLegato=1 if legato else 0)


def bass_macros(p, timbre="Osc A WT Pos"):
    macros(p, timbre=timbre, extra=[("Macro 3", "Sub Level", 0.4), ("Macro 4", "Filter 1 Drive", 0.4)])


# ------------------------------------------------------------------------------------------- Synth Bass
@arch(B, "Synth Bass", ["Ladder Bass", "Mono Bass", "Analog Bass", "Moog Bass", "Minimoog Bass", "Taurus Bass"], role="bass")
def _(p, r, i):
    mono(p, r)
    p.shape(0, "saw", level=0.7)
    p.shape(1, pick(r, ["saw", "square", "saw"]), level=0.55, fine=rr(r, 4, 12, 1), oct=pick(r, [0, -1]))
    if r.random() < 0.5: p.shape(2, "square", oct=-1, level=0.35)
    p.filt(0, type=pick(r, ["LP Ladder", "LP 24", "Diode LP"]), cutoff=rr(r, 250, 900, 0), res=rr(r, 0.1, 0.4, 2), env=rr(r, 0.25, 0.5, 2), drive=rr(r, 0.1, 0.4, 2), key=0.3)
    p.env(1, att=0.0, dec=rr(r, 0.15, 0.5, 2), sus=rr(r, 0.0, 0.3, 2), rel=0.1)
    p.env(0, att=0.002, dec=0.8, sus=rr(r, 0.7, 1.0, 2), rel=rr(r, 0.06, 0.15, 2))
    p.mod("Velocity", "Filter 1 Env Amt", 0.2)
    bass_macros(p, "Osc B Fine")


@arch(B, "Synth Bass", ["FM Bass", "DX Bass", "Rubber FM", "Slap FM", "Wire Bass", "Chrome Bass"])
def _(p, r, i):
    mono(p, r, (0.0, 0.04))
    p.osc(0, wt=pick(r, ["FM Bass", "FM 2:1", "FM Growl", "FM Feedback"]), pos=rr(r, 0.1, 0.6, 2), level=0.8)
    p.osc(1, wt="Sine", level=0.0, oct=pick(r, [0, 1, 1, 2]))
    p.osc(0, fmSrc="Osc B", fmAmt=rr(r, 0.1, 0.3, 2))
    p.env(1, att=0.0, dec=rr(r, 0.1, 0.45, 2), sus=rr(r, 0.0, 0.2, 2))
    p.mod("Env 2", "Osc A FM", rr(r, 0.2, 0.45, 2)).mod("Env 2", "Osc A WT Pos", rr(r, 0.1, 0.3, 2))
    p.mod("Velocity", "Osc A FM", 0.15)
    p.sub(shape=pick(r, ["Sine", "Driven Sine", "Soft Triangle"]), oct=-1, level=rr(r, 0.3, 0.5, 2), route="Direct")
    p.filt(0, type="LP 24", cutoff=rr(r, 2000, 6000, 0), res=0.1)
    p.env(0, att=0.001, dec=rr(r, 0.4, 1.0, 2), sus=rr(r, 0.4, 0.9, 2), rel=0.08)
    bass_macros(p, "Osc A FM")


@arch(B, "Synth Bass", ["PWM Bass", "Pulse Bass", "Square Bass", "Juno Bass", "Hollow Bass"])
def _(p, r, i):
    mono(p, r, (0.0, 0.05), legato=r.random() < 0.5)
    p.osc(0, wt=pick(r, ["Square PWM", "Analog PWM", "Warm Square"]), pos=rr(r, 0.1, 0.5, 2), level=0.75)
    p.lfo(0, shape="Triangle", rate=rr(r, 0.2, 1.2, 2), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", rr(r, 0.1, 0.3, 2))
    p.sub(shape="Square", oct=-1, level=rr(r, 0.25, 0.45, 2), route="Filter 1")
    p.filt(0, type=pick(r, ["LP 24", "LP 12", "MS-20 LP"]), cutoff=rr(r, 400, 1400, 0), res=rr(r, 0.1, 0.35, 2), env=rr(r, 0.2, 0.45, 2))
    p.env(1, dec=rr(r, 0.2, 0.6, 2), sus=rr(r, 0.1, 0.4, 2))
    p.env(0, att=0.002, dec=1.0, sus=0.9, rel=0.1)
    if r.random() < 0.5: chorus(p, r, mix=(0.15, 0.3))
    bass_macros(p)


@arch(B, "Synth Bass", ["Wavetable Bass", "Morph Bass", "Digital Bass", "Phase Bass", "Fold Bass"])
def _(p, r, i):
    mono(p, r, (0.0, 0.05))
    p.osc(0, wt=pick(r, ["Wavefold", "PD Saw", "PD Reso", "Digital Bits", "Harmonic Sweep", "Saw Stack", "Spectral Comb", "Fold Sine", "Double Saw"]),
          pos=rr(r, 0.0, 0.4, 2), level=0.75, voices=pick(r, [1, 2, 3]), detune=rr(r, 0.05, 0.15, 2))
    p.env(2, att=0.0, dec=rr(r, 0.2, 0.8, 2), sus=rr(r, 0.1, 0.5, 2), rel=0.1)
    p.mod("Env 3", "Osc A WT Pos", rr(r, 0.25, 0.6, 2))
    if r.random() < 0.5: p.osc(0, warp=pick(r, ["Bend +", "Fold", "Asym", "Mirror"]), warpAmt=rr(r, 0.1, 0.4, 2))
    p.sub(shape=pick(r, ["Sine", "Rounded Square", "Half Sine"]), oct=-1, level=rr(r, 0.3, 0.5, 2), route="Direct")
    p.filt(0, type=pick(r, ["LP 24", "LP 12", "BP 12"]), cutoff=rr(r, 900, 4000, 0), res=rr(r, 0.05, 0.3, 2), env=rr(r, 0.0, 0.3, 2))
    p.env(1, dec=0.3, sus=0.2)
    p.env(0, att=0.002, dec=1.0, sus=0.85, rel=0.1)
    bass_macros(p)


@arch(B, "Synth Bass", ["Funk Bass", "Bounce Bass", "Pluck Bass", "Disco Bass", "Snap Bass", "Electro Bass"])
def _(p, r, i):
    mono(p, r, (0.0, 0.02), legato=False)
    p.shape(0, pick(r, ["saw", "square"]), level=0.8)
    if r.random() < 0.6: p.shape(1, "saw", oct=-1, fine=rr(r, 3, 8, 1), level=0.45)
    p.filt(0, type=pick(r, ["LP 24", "LP Ladder", "MS-20 LP"]), cutoff=rr(r, 120, 300, 0), res=rr(r, 0.2, 0.55, 2), env=rr(r, 0.4, 0.65, 2))
    p.env(1, att=0.0, dec=rr(r, 0.08, 0.25, 2), sus=0.0)
    p.env(0, att=0.001, dec=rr(r, 0.25, 0.6, 2), sus=rr(r, 0.0, 0.3, 2), rel=0.06)
    p.mod("Velocity", "Filter 1 Env Amt", 0.25)
    if r.random() < 0.5: p.fx("comp", thresh=-20, ratio=4, makeup=4)
    bass_macros(p, "Filter 1 Reso")


# ------------------------------------------------------------------------------------------- Electric Bass (sampled)
def ebass(p, r, inst, tone=(3000, 9000), rel=0.12):
    mono(p, r, (0.0, 0.02), legato=False)
    p.set(polyphony=2)
    p.sample(inst, level=0.9, vel=rr(r, 0.6, 0.9, 2))
    p.filt(0, type="LP 12", cutoff=rr(r, *tone, 0), res=0.05)
    velocity_tone(p, 0.15)
    p.env(0, att=0.0, dec=1.0, sus=1.0, rel=rel)


@arch(B, "Electric Bass", ["Finger Bass", "Jazz Bass", "Session Bass", "Studio Bass", "Precision Bass", "Motown Bass"], role="real")
def _(p, r, i):
    ebass(p, r, "Finger Bass")
    p.fx("comp", thresh=rr(r, -24, -16, 1), ratio=rr(r, 3, 5, 1), att=rr(r, 5, 20, 1), rel=120, makeup=rr(r, 3, 6, 1))
    if r.random() < 0.5: p.fx("eq", lowf=rr(r, 70, 110, 0), lowg=rr(r, 1, 3, 1), midf=rr(r, 600, 900, 0), midg=rr(r, -3, 2, 1), highf=4000, highg=rr(r, -2, 2, 1))
    macros(p, extra=[("Macro 2", "Filter 1 Reso", 0.3)])


@arch(B, "Electric Bass", ["Pick Bass", "Rock Bass", "Punk Bass", "Indie Bass", "Plectrum Bass"], role="real")
def _(p, r, i):
    ebass(p, r, "Pick Bass", (4000, 12000))
    p.fx("dist", type=pick(r, ["Tube", "Overdrive", "Soft Clip"]), drive=rr(r, 0.15, 0.4, 2), tone=rr(r, 0.4, 0.7, 2), mix=rr(r, 0.3, 0.7, 2))
    p.fx("comp", thresh=-18, ratio=4, makeup=4)
    macros(p, extra=[("Macro 2", "Filter 1 Drive", 0.3)])


@arch(B, "Electric Bass", ["Growl Bass", "Grit Bass", "Amp Bass", "Fuzz Bass", "Garage Bass"], role="real")
def _(p, r, i):
    ebass(p, r, "Growl Bass", (2500, 8000))
    if r.random() < 0.6: p.fx("dist", type=pick(r, ["Tube", "Diode", "Overdrive"]), drive=rr(r, 0.2, 0.5, 2), mix=rr(r, 0.3, 0.6, 2))
    p.fx("comp", thresh=-20, ratio=5, makeup=5)
    macros(p)


@arch(B, "Electric Bass", ["Fretless", "Fretless Slide", "Singing Fretless", "Smooth Fretless"], role="real")
def _(p, r, i):
    ebass(p, r, "Finger Bass", (1800, 4000), 0.2)
    p.set(voiceMode="Legato", glide=rr(r, 0.06, 0.15, 3), glideLegato=1, polyphony=1)
    chorus(p, r, mix=(0.15, 0.3), depth=(0.2, 0.4))
    reverb(p, r, "Room", mix=(0.06, 0.12))
    macros(p)


@arch(B, "Electric Bass", ["Synth-Layer Bass", "Hybrid Bass", "Bass + Sub", "Thick Bass"], role="real")
def _(p, r, i):
    ebass(p, r, pick(r, ["Finger Bass", "Pick Bass"]))
    p.sub(shape=pick(r, ["Sine", "Soft Triangle"]), oct=-1, level=rr(r, 0.25, 0.45, 2), route="Direct")
    p.fx("comp", thresh=-20, ratio=4, makeup=4)
    macros(p, extra=[("Macro 3", "Sub Level", 0.4)])


# ------------------------------------------------------------------------------------------- Acoustic Bass
@arch(B, "Acoustic Bass", ["Upright Bass", "Jazz Upright", "Walking Bass", "Club Upright", "Double Bass Pizz"], role="real")
def _(p, r, i):
    mono(p, r, (0.0, 0.0), legato=False); p.set(polyphony=2)
    p.sample("Upright Bass", level=0.9, vel=rr(r, 0.6, 0.9, 2))
    p.filt(0, type="LP 12", cutoff=rr(r, 2500, 9000, 0), res=0.05)
    velocity_tone(p, 0.2)
    p.env(0, att=0.0, dec=1.0, sus=1.0, rel=rr(r, 0.1, 0.25, 2))
    reverb(p, r, "Room", mix=(0.06, 0.14), size=(0.2, 0.45), decay=(0.6, 1.4))
    if r.random() < 0.5: p.fx("comp", thresh=-22, ratio=3, makeup=3)
    macros(p)


@arch(B, "Acoustic Bass", ["Bowed Bass", "Arco Bass", "Contrabass", "Low Arco", "Concert Bass"], role="real")
def _(p, r, i):
    p.set(polyphony=4)
    p.sample("Contrabass", level=0.9)
    p.filt(0, type="LP 12", cutoff=rr(r, 3000, 10000, 0))
    p.env(0, att=rr(r, 0.05, 0.25, 2), dec=1.0, sus=1.0, rel=rr(r, 0.3, 0.7, 2))
    vibrato(p, r, cents=(4, 10))
    reverb(p, r, "Hall", mix=(0.12, 0.22))
    macros(p)


@arch(B, "Acoustic Bass", ["Pizzicato Bass", "Orchestra Pizz Bass", "Plucked Bass", "Thumb Bass"], role="real")
def _(p, r, i):
    mono(p, r, (0.0, 0.0), legato=False); p.set(polyphony=3)
    p.sample(pick(r, ["Upright Bass", "Pizzicato Strings"]), level=0.9, oct=0)
    p.sub(shape="Sine", oct=-1, level=rr(r, 0.1, 0.25, 2), route="Direct")
    p.env(0, att=0.0, dec=rr(r, 0.4, 0.9, 2), sus=0.0, rel=0.15)
    reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p)


# ------------------------------------------------------------------------------------------- Reese Bass
@arch(B, "Reese Bass", ["Reese", "Classic Reese", "Jungle Reese", "DnB Reese", "Old Skool Reese"], role="bass")
def _(p, r, i):
    mono(p, r, (0.02, 0.05))
    for o in (0, 1):
        p.osc(o, wt=pick(r, ["Analog Saw", "Saw", "Soft Saw"]), voices=pick(r, [2, 3, 4]), detune=rr(r, 0.12, 0.3, 2), width=rr(r, 0.4, 0.9, 2),
              level=0.6, fine=rr(r, -14, 14, 1))
    p.sub(shape="Sine", oct=-1, level=rr(r, 0.4, 0.6, 2), route="Direct")
    p.filt(0, type=pick(r, ["LP 24", "LP Ladder"]), cutoff=rr(r, 300, 900, 0), res=rr(r, 0.1, 0.35, 2), drive=rr(r, 0.2, 0.5, 2))
    p.lfo(1, shape="Sine", rate=rr(r, 0.1, 0.5, 2), mode="Free")
    p.mod("LFO 2", "Filter 1 Cutoff", rr(r, 0.04, 0.12, 3))
    if r.random() < 0.5: p.fx("pha", rate=rr(r, 0.08, 0.3, 2), depth=rr(r, 0.4, 0.7, 2), mix=rr(r, 0.25, 0.45, 2))
    if r.random() < 0.6: p.fx("dist", type=pick(r, ["Soft Clip", "Tube", "Overdrive"]), drive=rr(r, 0.2, 0.5, 2), mix=0.7)
    bass_macros(p, "Osc A Detune")


@arch(B, "Reese Bass", ["Neuro Reese", "Comb Reese", "Machine Reese", "Steel Reese", "Formant Reese"], role="bass")
def _(p, r, i):
    mono(p, r, (0.02, 0.04))
    p.osc(0, wt=pick(r, ["Saw Stack", "Phaser Saw", "Double Saw"]), voices=3, detune=rr(r, 0.15, 0.3, 2), level=0.65)
    p.osc(1, wt=pick(r, ["Spectral Comb", "Harmonic Sweep", "Growl"]), pos=rr(r, 0.2, 0.6, 2), level=0.45, fine=rr(r, 5, 15, 1))
    p.sub(shape="Sine", oct=-1, level=0.5, route="Direct")
    p.filt(0, type=pick(r, ["Comb +", "Comb -", "Formant", "Dual BP"]), cutoff=rr(r, 300, 1500, 0), res=rr(r, 0.3, 0.6, 2), mix=rr(r, 0.5, 0.9, 2))
    p.filt(1, type="LP 24", cutoff=rr(r, 1500, 5000, 0), res=0.1)
    p.lfo(0, shape=pick(r, ["Sine", "Triangle", "Smooth Random"]), div=pick(r, ["1 Bar", "1/2", "2 Bars"]), mode="Free")
    p.mod("LFO 1", "Filter 1 Cutoff", rr(r, 0.1, 0.25, 2)).mod("LFO 1", "Osc B WT Pos", rr(r, 0.2, 0.4, 2))
    p.fx("dist", type=pick(r, ["Fold", "Diode", "Overdrive"]), drive=rr(r, 0.3, 0.55, 2), mix=0.8)
    p.fx("comp", thresh=-20, ratio=5, makeup=4)
    bass_macros(p, "Osc B WT Pos")


@arch(B, "Reese Bass", ["Liquid Reese", "Soft Reese", "Rolling Reese", "Dub Reese", "Velvet Reese"], role="bass")
def _(p, r, i):
    mono(p, r, (0.03, 0.08))
    for o in (0, 1): p.osc(o, wt="Soft Saw", voices=2, detune=rr(r, 0.1, 0.2, 2), level=0.55, fine=rr(r, -9, 9, 1))
    p.sub(shape=pick(r, ["Sine", "Soft Triangle"]), oct=-1, level=0.55, route="Direct")
    p.filt(0, type="LP 24", cutoff=rr(r, 250, 600, 0), res=rr(r, 0.05, 0.2, 2))
    p.lfo(1, shape="Sine", rate=rr(r, 0.08, 0.25, 2), mode="Free")
    p.mod("LFO 2", "Osc A Fine", rr(r, 4, 9, 1) * CENT)
    chorus(p, r, mix=(0.15, 0.3))
    bass_macros(p, "Osc A Detune")


# ------------------------------------------------------------------------------------------- Hard Bass
@arch(B, "Hard Bass", ["Growl", "Beast Growl", "Talking Bass", "Snarl", "Mouth Bass", "Yoi Bass"], role="bass")
def _(p, r, i):
    mono(p, r, (0.01, 0.03))
    p.osc(0, wt=pick(r, ["Growl", "FM Growl", "Talking", "Throat", "Vowel A-E"]), pos=rr(r, 0.2, 0.5, 2), voices=2, detune=0.08,
          warp=pick(r, ["Fold", "Bend +", "Off", "Asym"]), warpAmt=rr(r, 0.1, 0.4, 2), level=0.8)
    p.osc(1, wt=pick(r, ["FM Metal", "Digital Bits", "Saw"]), pos=0.3, level=0.3, oct=-1)
    p.sub(shape="Sine", oct=-1, level=0.5, route="Direct")
    p.filt(0, type=pick(r, ["Formant", "BP 12", "Peak", "Comb +"]), cutoff=rr(r, 400, 1600, 0), res=rr(r, 0.4, 0.7, 2))
    p.lfo(0, shape=pick(r, ["Sine", "Triangle", "Wobble 2x", "Wobble 3x", "Smooth Random", "Shark Fin"]), div=pick(r, ["1/8", "1/16", "1/8 T", "1/4", "1/8 D"]))
    p.mod("LFO 1", "Osc A WT Pos", rr(r, 0.35, 0.6, 2)).mod("LFO 1", "Filter 1 Cutoff", rr(r, 0.2, 0.4, 2))
    p.fx("dist", type=pick(r, ["Fold", "Diode", "Overdrive", "Hard Clip"]), drive=rr(r, 0.45, 0.75, 2), mix=0.9)
    p.fx("comp", thresh=-22, ratio=6, makeup=5)
    bass_macros(p)


@arch(B, "Hard Bass", ["Wobble", "Dubstep Wobble", "Brostep Wob", "Riddim", "Halftime Wob", "Wub"], role="bass")
def _(p, r, i):
    mono(p, r, (0.01, 0.03))
    p.osc(0, wt=pick(r, ["Saw", "Square PWM", "Growl", "Saw Stack"]), voices=2, detune=0.1, level=0.8)
    p.sub(shape="Sine", oct=-1, level=0.55, route="Direct")
    p.filt(0, type=pick(r, ["LP 24", "LP Ladder", "MS-20 LP", "Diode LP"]), cutoff=rr(r, 150, 350, 0), res=rr(r, 0.35, 0.65, 2), drive=0.4)
    p.lfo(0, shape=pick(r, ["Sine", "Triangle", "Saw Down", "Exp Down", "Wobble 2x", "Bounce"]), div=pick(r, ["1/4", "1/8", "1/8 T", "1/4 T", "1/16", "1/8 D"]), mode="Retrig")
    p.mod("LFO 1", "Filter 1 Cutoff", rr(r, 0.35, 0.55, 2)).mod("LFO 1", "Osc A WT Pos", 0.3)
    p.fx("dist", type=pick(r, ["Soft Clip", "Tube", "Overdrive", "Fold"]), drive=rr(r, 0.35, 0.65, 2), mix=0.85)
    p.fx("comp", thresh=-20, ratio=4, makeup=4)
    bass_macros(p, "LFO 1 Rate")


@arch(B, "Hard Bass", ["Distorted Bass", "Industrial Bass", "Saw Grinder", "Hardstyle Bass", "Rave Hoover", "Mid Bass"], role="bass")
def _(p, r, i):
    mono(p, r, (0.0, 0.03))
    p.osc(0, wt=pick(r, ["Saw", "Hard Sync", "Square Sync", "XOR", "Stepped Saw"]), pos=rr(r, 0.1, 0.6, 2), voices=pick(r, [1, 3, 5]), detune=rr(r, 0.1, 0.3, 2), level=0.75)
    p.osc(1, wt="Saw", oct=-1, level=0.45, fine=7)
    p.sub(shape=pick(r, ["Sine", "Driven Sine", "808 Sub"]), oct=-1, level=0.5, route="Direct")
    p.filt(0, type=pick(r, ["LP 24", "Diode LP", "MS-20 LP"]), cutoff=rr(r, 600, 2500, 0), res=rr(r, 0.2, 0.5, 2), env=rr(r, 0.1, 0.35, 2), drive=rr(r, 0.4, 0.8, 2))
    p.env(1, dec=0.25, sus=0.3)
    p.fx("dist", type=pick(r, ["Hard Clip", "Fold", "Diode", "Bitcrush"]), drive=rr(r, 0.5, 0.8, 2), mix=0.85)
    p.fx("comp", thresh=-24, ratio=8, makeup=6)
    bass_macros(p, "Filter 1 Drive")


# ------------------------------------------------------------------------------------------- Soft Bass
@arch(B, "Soft Bass", ["Soft Bass", "Round Bass", "Velvet Bass", "Warm Bass", "Pillow Bass", "Mellow Bass"], role="bass")
def _(p, r, i):
    mono(p, r, (0.02, 0.08))
    p.shape(0, pick(r, ["tri", "sine", "tri"]), level=0.8)
    p.shape(1, pick(r, ["saw", "square"]), level=rr(r, 0.1, 0.25, 2), fine=rr(r, 3, 8, 1))
    p.filt(0, type=pick(r, ["LP 12", "LP 24", "LP 6"]), cutoff=rr(r, 300, 900, 0), res=rr(r, 0.0, 0.15, 2), env=rr(r, 0.05, 0.2, 2))
    p.env(1, dec=0.4, sus=0.3)
    p.env(0, att=rr(r, 0.005, 0.03, 3), dec=1.0, sus=rr(r, 0.8, 1.0, 2), rel=rr(r, 0.1, 0.3, 2))
    if r.random() < 0.4: p.fx("tape", wow=0.08, flutter=0.06, sat=0.3, mix=1.0)
    bass_macros(p, "Osc B Level")


@arch(B, "Soft Bass", ["Deep House Bass", "Organ Bass", "Garage Bass", "Chord Bass", "Dub Bass"], role="bass")
def _(p, r, i):
    mono(p, r, (0.0, 0.03), legato=False)
    p.osc(0, wt=pick(r, ["Organ", "Octaves", "Even Harmonics", "Half Sine", "Trapezoid"]), pos=rr(r, 0.0, 0.5, 2), level=0.8)
    p.sub(shape=pick(r, ["Sine", "Organ Sub"]), oct=-1, level=rr(r, 0.3, 0.5, 2), route="Direct")
    p.filt(0, type="LP 24", cutoff=rr(r, 500, 1500, 0), res=rr(r, 0.05, 0.25, 2), env=rr(r, 0.15, 0.3, 2))
    p.env(1, dec=rr(r, 0.15, 0.4, 2), sus=0.2)
    p.env(0, att=0.002, dec=rr(r, 0.4, 0.9, 2), sus=rr(r, 0.3, 0.8, 2), rel=0.12)
    bass_macros(p)


@arch(B, "Soft Bass", ["Felt Bass", "Muted Upright", "Lo-Fi Bass", "Bedroom Bass", "Sleepy Bass"], role="real")
def _(p, r, i):
    mono(p, r, (0.0, 0.0), legato=False); p.set(polyphony=2)
    p.sample(pick(r, ["Upright Bass", "Finger Bass"]), level=0.9, vel=0.6)
    p.filt(0, type="LP 24", cutoff=rr(r, 500, 1300, 0), res=0.05)
    p.env(0, att=0.004, dec=rr(r, 0.5, 1.2, 2), sus=rr(r, 0.0, 0.5, 2), rel=0.15)
    p.sub(shape="Sine", oct=-1, level=rr(r, 0.1, 0.25, 2), route="Direct")
    macros(p)


# ------------------------------------------------------------------------------------------- Sub Bass
@arch(B, "Sub Bass", ["Sub", "Pure Sub", "Deep Sub", "Floor Sub", "Rumble", "Pressure"], role="bass", chars=["clean", "vintage", "dark", "lofi", "digital", "clean", "dark", "vintage"])
def _(p, r, i):
    mono(p, r, (0.02, 0.07))
    p.osc(0, wt=pick(r, ["Sine", "Sub Bass", "Half Sine", "Triangle"]), pos=rr(r, 0.0, 0.4, 2), level=0.85)
    p.filt(0, type="LP 24", cutoff=rr(r, 200, 600, 0), res=0.05)
    p.env(0, att=rr(r, 0.002, 0.02, 3), dec=1.0, sus=1.0, rel=rr(r, 0.06, 0.2, 2))
    if r.random() < 0.4: p.fx("dist", type=pick(r, ["Soft Clip", "Tube"]), drive=rr(r, 0.1, 0.3, 2), mix=0.5)
    macros(p, extra=[("Macro 2", "Osc A WT Pos", 0.4)])


@arch(B, "Sub Bass", ["Saturated Sub", "Harmonic Sub", "Octave Sub", "Fifth Sub", "Driven Sub"], role="bass")
def _(p, r, i):
    mono(p, r, (0.02, 0.06))
    p.osc_off(0)
    p.sub(shape=pick(r, ["Driven Sine", "Sine + Octave", "Sine + Twelfth", "808 Sub", "Full-wave Sine", "Rounded Square", "Warm Saw"]), oct=0, level=0.9, route="Filter 1")
    p.filt(0, type="LP 24", cutoff=rr(r, 300, 1200, 0), res=0.05, drive=rr(r, 0.1, 0.4, 2))
    p.env(0, att=0.003, dec=1.0, sus=1.0, rel=0.1)
    macros(p, extra=[("Macro 2", "Filter 1 Drive", 0.4)])


# ------------------------------------------------------------------------------------------- Acid Bass
@arch(B, "Acid Bass", ["303", "Acid Line", "Squelch", "Acid Bass", "Rave Acid", "Chicago Acid"], role="bass")
def _(p, r, i):
    mono(p, r, (0.04, 0.09))
    p.shape(0, "saw" if r.random() < 0.6 else "square", level=0.8)
    p.filt(0, type=pick(r, ["LP Ladder", "Diode LP", "MS-20 LP", "LP 24"]), cutoff=rr(r, 250, 700, 0), res=rr(r, 0.65, 0.88, 2), drive=rr(r, 0.2, 0.5, 2), env=rr(r, 0.35, 0.6, 2))
    p.env(1, att=0.0, dec=rr(r, 0.12, 0.35, 2), sus=0.0)
    p.env(0, att=0.001, dec=0.6, sus=rr(r, 0.4, 0.9, 2), rel=0.06)
    p.mod("Velocity", "Filter 1 Env Amt", 0.3).mod("Velocity", "Filter 1 Cutoff", 0.1)
    p.fx("dist", type=pick(r, ["Soft Clip", "Overdrive", "Tube", "Diode"]), drive=rr(r, 0.3, 0.6, 2), mix=0.65)
    if r.random() < 0.5: delay(p, r, div=pick(r, ["1/8 D", "1/16 D", "1/8"]), mix=(0.1, 0.18))
    macros(p, extra=[("Macro 2", "Filter 1 Reso", 0.3), ("Macro 3", "Filter 1 Env Amt", 0.3)])


@arch(B, "Acid Bass", ["Acid Sequence", "Acid Groove", "303 Loop", "Warehouse Acid", "Hypnotic Acid"], role="bass")
def _(p, r, i):
    mono(p, r, (0.05, 0.1))
    p.shape(0, pick(r, ["saw", "square"]), level=0.8)
    p.filt(0, type=pick(r, ["LP Ladder", "Diode LP"]), cutoff=rr(r, 300, 650, 0), res=rr(r, 0.7, 0.88, 2), drive=0.4, env=rr(r, 0.4, 0.6, 2))
    p.env(1, dec=rr(r, 0.1, 0.25, 2), sus=0.0)
    p.env(0, att=0.001, dec=0.4, sus=0.6, rel=0.05)
    pat = []
    for s in range(16):
        if r.random() < 0.2: pat.append(None); continue
        d = pick(r, [0, 0, 0, 12, 7, 3, -2, 5, 10, 0])
        acc = 1.0 if r.random() < 0.3 else 0.6
        tie = 1 if r.random() < 0.15 else 0
        pat.append((d, acc, 0.6, tie))
    p.arpeggio(pattern=pat, mode="Sequencer", rate="1/16", steps=16, gate=rr(r, 0.4, 0.7, 2), scale="Chromatic", latch=0)
    p.mod("Velocity", "Filter 1 Env Amt", 0.35)
    p.fx("dist", type=pick(r, ["Soft Clip", "Overdrive", "Tube"]), drive=rr(r, 0.3, 0.6, 2), mix=0.6)
    delay(p, r, div=pick(r, ["1/8 D", "1/16 D"]), mix=(0.08, 0.15))
    macros(p, extra=[("Macro 2", "Filter 1 Reso", 0.3)])
    p.add_tags("Sequence")


# ------------------------------------------------------------------------------------------- 808
@arch(B, "808", ["808", "Trap 808", "Long 808", "Boom 808", "Classic 808", "Deep 808"], role="bass")
def _(p, r, i):
    mono(p, r, (0.04, 0.12))
    p.osc(0, wt=pick(r, ["Sine", "Sine", "Sub Bass", "Half Sine"]), level=0.9)
    p.sub(shape=pick(r, ["808 Sub", "Sine + Octave", "Driven Sine"]), oct=0, level=rr(r, 0.1, 0.3, 2), route="Direct")
    p.filt(0, on=0)
    p.env(1, att=0.0, dec=rr(r, 0.03, 0.08, 3), sus=0.0)
    p.mod("Env 2", "Osc A Pitch", rr(r, 0.12, 0.25, 2))
    p.env(0, att=0.001, dec=rr(r, 1.2, 4.0, 2), sus=0.0, rel=rr(r, 0.1, 0.3, 2), dCrv=0.5)
    p.fx("dist", type=pick(r, ["Soft Clip", "Tube", "Soft Clip"]), drive=rr(r, 0.1, 0.35, 2), mix=1.0)
    macros(p, tone=False, extra=[("Macro 2", "Sub Level", 0.4)])


@arch(B, "808", ["Distorted 808", "Dirty 808", "Drill 808", "Rage 808", "Clipped 808"], role="bass")
def _(p, r, i):
    mono(p, r, (0.05, 0.12))
    p.osc(0, wt=pick(r, ["Sine", "Sub Bass", "Half Sine"]), level=0.9)
    p.env(1, att=0.0, dec=rr(r, 0.03, 0.07, 3), sus=0.0)
    p.mod("Env 2", "Osc A Pitch", rr(r, 0.15, 0.3, 2))
    p.env(0, att=0.001, dec=rr(r, 1.0, 3.0, 2), sus=rr(r, 0.0, 0.3, 2), rel=0.2)
    p.filt(0, type="LP 24", cutoff=rr(r, 1500, 5000, 0), res=0.05)
    p.fx("dist", type=pick(r, ["Hard Clip", "Overdrive", "Fold", "Diode"]), drive=rr(r, 0.45, 0.8, 2), tone=rr(r, 0.4, 0.7, 2), mix=1.0)
    p.fx("comp", thresh=-16, ratio=4, makeup=2)
    macros(p, extra=[("Macro 2", "Filter 1 Drive", 0.4)])


@arch(B, "808", ["Slide 808", "Glide 808", "Portamento 808", "Bending 808"], role="bass")
def _(p, r, i):
    mono(p, r, (0.12, 0.25))
    p.osc(0, wt="Sine", level=0.9)
    p.sub(shape="Sine + Octave", oct=0, level=rr(r, 0.1, 0.25, 2), route="Direct")
    p.env(1, att=0.0, dec=0.05, sus=0.0)
    p.mod("Env 2", "Osc A Pitch", rr(r, 0.1, 0.2, 2))
    p.env(0, att=0.001, dec=rr(r, 2.0, 5.0, 2), sus=rr(r, 0.2, 0.5, 2), rel=0.2)
    p.filt(0, type="LP 24", cutoff=rr(r, 800, 3000, 0))
    p.fx("dist", type="Tube", drive=rr(r, 0.2, 0.45, 2), mix=1.0)
    macros(p)


@arch(B, "808", ["Reese 808", "Hybrid 808", "Sub Kick 808", "Punch 808", "Tone 808"], role="bass")
def _(p, r, i):
    mono(p, r, (0.04, 0.1))
    p.osc(0, wt="Sine", level=0.85)
    p.osc(1, wt=pick(r, ["Analog Saw", "Square", "FM Bass"]), level=rr(r, 0.15, 0.3, 2), voices=2, detune=0.12)
    p.noise(type=pick(r, ["White", "Pink"]), level=rr(r, 0.05, 0.15, 2), route="Direct")
    p.env(1, att=0.0, dec=0.05, sus=0.0)
    p.env(2, att=0.0, dec=0.02, sus=0.0)
    p.mod("Env 2", "Osc A Pitch", rr(r, 0.15, 0.25, 2)).mod("Env 3", "Noise Level", -0.9)
    p.mod("Env 2", "Osc B Pitch", rr(r, 0.15, 0.25, 2))
    p.filt(0, type="LP 24", cutoff=rr(r, 400, 1200, 0), res=0.1)
    p.env(0, att=0.001, dec=rr(r, 1.0, 2.5, 2), sus=0.0, rel=0.15)
    p.fx("dist", type=pick(r, ["Soft Clip", "Tube", "Overdrive"]), drive=rr(r, 0.25, 0.5, 2), mix=1.0)
    macros(p)
