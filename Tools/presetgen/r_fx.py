"""FX: sound fx, texture, drones, risers, downlifter, hit, foley."""
from core import *
from registry import arch

F = "FX"
NOISES = ["White", "Pink", "Brown", "Crackle", "Digital", "Blue", "Vinyl", "Tape Hiss", "Breath", "Wind", "Ocean", "Rain", "Fire", "Bit-crushed", "Static", "Geiger"]


# ------------------------------------------------------------------------------------------- Sound FX
@arch(F, "Sound FX", ["Laser", "Zap", "Pew", "Blaster", "Ray Gun", "Photon"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw", "Square", "FM Feedback", "Hard Sync"]), pos=0.5, level=0.6)
    p.env(1, att=0.0, dec=rr(r, 0.1, 0.4, 2), sus=0.0)
    p.mod("Env 2", "Osc A Pitch", rr(r, 0.4, 0.9, 2))
    p.filt(0, type="LP 24", cutoff=6000, res=0.3)
    p.env(0, att=0.0, dec=rr(r, 0.2, 0.5, 2), sus=0.0, rel=0.2)
    delay(p, r, mode="Ping-Pong", mix=(0.15, 0.3))
    macros(p)


@arch(F, "Sound FX", ["UFO", "Alien Signal", "Saucer", "Mothership", "Close Encounter"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Sine", "Ring Mod", "FM 2:1"]), pos=0.4, level=0.6)
    p.lfo(0, shape=pick(r, ["Sine", "Triangle", "Square"]), rate=rr(r, 3, 12, 1), mode="Free")
    p.lfo(1, shape="Sine", rate=rr(r, 0.1, 0.5, 2), mode="Free")
    p.mod("LFO 1", "Osc A Pitch", rr(r, 0.05, 0.2, 2)).mod("LFO 2", "LFO 1 Rate", 0.3)
    p.env(0, att=0.2, dec=1.0, sus=1.0, rel=1.0)
    p.fx("shf", mode="Freq Shift", freq=rr(r, -200, 200, 0), fb=0.3, mix=0.3)
    reverb(p, r, "Hall", mix=(0.25, 0.4)); macros(p)


@arch(F, "Sound FX", ["Alarm", "Siren", "Klaxon", "Warning", "Red Alert"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Square", "Saw", "Triangle"]), level=0.5)
    p.lfo(0, shape=pick(r, ["Square", "Triangle", "Saw Up"]), rate=rr(r, 0.8, 3.0, 2), mode="Free")
    p.mod("LFO 1", "Osc A Pitch", rr(r, 0.08, 0.2, 2))
    p.filt(0, type="BP 12", cutoff=2000, res=0.2)
    p.env(0, att=0.01, dec=1.0, sus=1.0, rel=0.3)
    reverb(p, r, "Room", mix=(0.15, 0.25)); macros(p)


@arch(F, "Sound FX", ["Robot Talk", "Computer Babble", "Data Stream", "Modem", "Glitch Bot"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Robot Voice", "Digital Bits", "Glitch Steps", "XOR"]), pos=0.0, level=0.6)
    p.lfo(0, shape="S&H", rate=rr(r, 8, 20, 1), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", 0.8).mod("LFO 1", "Osc A Pitch", rr(r, 0.05, 0.25, 2))
    p.filt(0, type="Formant", cutoff=1200, res=0.3, mix=0.7)
    p.env(0, att=0.0, dec=1.0, sus=1.0, rel=0.2)
    p.analog(bits=rr(r, 0.3, 0.6, 2))
    macros(p)


# ------------------------------------------------------------------------------------------- Texture
@arch(F, "Texture", ["Rain Texture", "Storm", "Ocean Waves", "Wind Field", "Fireplace", "Night Air"], role="fx")
def _(p, r, i):
    p.osc_off(0)
    p.noise(type=pick(r, ["Rain", "Ocean", "Wind", "Fire", "Breath", "Vinyl"]), level=0.8, color=rr(r, -0.4, 0.4, 2), route="Filter 1")
    p.filt(0, type=pick(r, ["LP 12", "BP 12", "LP 24"]), cutoff=rr(r, 1500, 6000, 0), res=0.1)
    p.lfo(0, shape="Smooth Random", rate=rr(r, 0.05, 0.3, 2), mode="Free")
    p.mod("LFO 1", "Filter 1 Cutoff", 0.2).mod("LFO 1", "Noise Level", 0.15)
    p.env(0, att=rr(r, 0.5, 2.0, 2), dec=1.0, sus=1.0, rel=2.0)
    reverb(p, r, "Hall", mix=(0.2, 0.35)); macros(p)


@arch(F, "Texture", ["Granular Texture", "Shimmer Grains", "Crystal Dust", "Particle Field", "Frozen Air"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Glass", "Noise Spectra", "Whisper", "Spectral Comb"]), pos=rr(r, 0.1, 0.9, 2), voices=4, detune=0.3, level=0.5)
    p.env(0, att=rr(r, 0.5, 2.0, 2), dec=1.0, sus=1.0, rel=3.0)
    p.fx("grn", size=rr(r, 20, 120, 0), density=rr(r, 25, 55, 1), spray=rr(r, 0.5, 1.0, 2), pitch=pick(r, [12, 7, 0, 19, -12]), prand=rr(r, 0.2, 0.6, 2), rev=0.3, mix=0.7)
    reverb(p, r, "Shimmer", mix=(0.3, 0.45)); macros(p)


@arch(F, "Texture", ["Vinyl Crackle", "Old Record", "Radio Static", "Tape Noise", "Broken Signal"], role="fx", chars=["lofi", "vintage", "dark", "clean"])
def _(p, r, i):
    p.osc_off(0)
    p.noise(type=pick(r, ["Vinyl", "Crackle", "Static", "Tape Hiss", "Geiger"]), level=1.0, route="Filter 1")
    p.osc(0, wt=pick(r, ["Noise Spectra", "Digital Noise"]), pos=rr(r, 0.2, 0.8, 2), level=rr(r, 0.08, 0.18, 2))   # a bed of hiss under the clicks
    p.filt(0, type="BP 12", cutoff=rr(r, 1500, 4000, 0), res=0.1)
    p.fx("comp", thresh=-30, ratio=4, makeup=14)
    p.env(0, att=0.05, dec=1.0, sus=1.0, rel=0.5)
    macros(p)


# ------------------------------------------------------------------------------------------- Drones
@arch(F, "Drones", ["Dark Drone", "Abyss Drone", "Void", "Underworld", "Cave Drone", "Doom Drone"], role="fx", chars=["dark", "fx", "wide", "clean", "lofi", "dark"])
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw", "Growl", "Spectral Comb", "Noise Spectra", "Throat"]), pos=rr(r, 0.2, 0.7, 2), voices=5, detune=0.2, level=0.5, oct=-1)
    p.osc(1, wt="Sine", oct=-2, level=0.4)
    p.filt(0, type=pick(r, ["LP 24", "Comb -", "BP 12"]), cutoff=rr(r, 200, 700, 0), res=0.35)
    p.lfo(0, shape="Smooth Random", rate=rr(r, 0.03, 0.15, 2), mode="Free")
    p.mod("LFO 1", "Filter 1 Cutoff", 0.2).mod("LFO 1", "Osc A WT Pos", 0.3)
    p.env(0, att=rr(r, 1.0, 4.0, 2), dec=1.0, sus=1.0, rel=4.0)
    reverb(p, r, "Hall", mix=(0.3, 0.5), decay=(6, 14)); macros(p)


@arch(F, "Drones", ["Bright Drone", "Light Drone", "Celestial Drone", "Choir Drone", "Overtone Drone"], role="fx", chars=["bright", "wide", "clean", "fx"])
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Octaves & Fifths", "Harmonic Pairs", "Choir", "Glass", "Octaves"]), pos=rr(r, 0.2, 0.8, 2), voices=6, detune=0.15, level=0.5)
    p.lfo(0, shape="Triangle", rate=rr(r, 0.02, 0.08, 2), mode="Free"); p.mod("LFO 1", "Osc A WT Pos", 0.4)
    p.env(0, att=rr(r, 1.0, 3.0, 2), dec=1.0, sus=1.0, rel=4.0)
    reverb(p, r, "Shimmer", mix=(0.3, 0.5), decay=(6, 14)); macros(p)


@arch(F, "Drones", ["Machine Drone", "Engine Room", "Reactor Hum", "Turbine", "Power Grid"], role="fx", chars=["dark", "digital", "fx", "lofi"])
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Square", "Saw", "Ring Mod", "XOR"]), pos=0.4, level=0.5, oct=-1)
    p.noise(type="Brown", level=0.3, route="Filter 1")
    p.analog(hum=rr(r, 0.3, 0.7, 2), noise=0.2)
    p.filt(0, type="LP 24", cutoff=rr(r, 300, 900, 0), res=0.4)
    p.lfo(0, shape="Sine", rate=rr(r, 0.2, 2.0, 2), mode="Free"); p.mod("LFO 1", "Filter 1 Cutoff", 0.15)
    p.env(0, att=1.0, dec=1.0, sus=1.0, rel=2.0)
    reverb(p, r, "Room", mix=(0.15, 0.3)); macros(p)


# ------------------------------------------------------------------------------------------- Risers / Downlifters
@arch(F, "Risers", ["Noise Riser", "White Riser", "Build Up", "Uplifter", "Sweep Up", "Tension Riser"], role="fx")
def _(p, r, i):
    p.osc_off(0)
    p.noise(type=pick(r, ["White", "Pink", "Wind", "Digital"]), level=0.8, route="Filter 1")
    p.filt(0, type=pick(r, ["BP 12", "HP 12", "LP 24"]), cutoff=200, res=rr(r, 0.2, 0.5, 2))
    t = rr(r, 4.0, 10.0, 1)
    p.env(2, att=t, dec=0.1, sus=1.0, rel=0.2, aCrv=rr(r, -0.5, 0.3, 2))
    p.mod("Env 3", "Filter 1 Cutoff", 0.85)
    p.env(0, att=t * 0.6, dec=1.0, sus=1.0, rel=0.5)
    reverb(p, r, "Hall", mix=(0.2, 0.35)); delay(p, r, mix=(0.1, 0.2))
    macros(p)


@arch(F, "Risers", ["Pitch Riser", "Siren Riser", "Saw Riser", "Supersaw Rise", "Lift Off"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw Stack", "Saw", "Square PWM"]), voices=7, detune=0.3, level=0.5)
    t = rr(r, 4.0, 8.0, 1)
    p.env(2, att=t, dec=0.1, sus=1.0, rel=0.2)
    p.mod("Env 3", "Osc A Pitch", rr(r, 0.25, 0.5, 2)).mod("Env 3", "Filter 1 Cutoff", 0.5)
    p.filt(0, type="LP 24", cutoff=500, res=0.2)
    p.env(0, att=t * 0.5, dec=1.0, sus=1.0, rel=0.4)
    p.set(motion_amt=rr(r, 0.0, 0.5, 2), motion_shape="Trance Gate", motion_rate="1/16")
    reverb(p, r, "Hall", mix=(0.2, 0.35)); macros(p)


@arch(F, "Downlifter", ["Downlifter", "Fall", "Drop Sweep", "Sink", "Descent", "Power Down"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw", "Saw Stack", "Square"]), voices=5, detune=0.25, level=0.4)
    p.noise(type=pick(r, ["White", "Pink"]), level=0.5, route="Filter 1")
    t = rr(r, 2.0, 5.0, 1)
    p.env(2, att=0.0, dec=t, sus=0.0, rel=0.2)
    p.mod("Env 3", "Osc A Pitch", rr(r, 0.25, 0.5, 2)).mod("Env 3", "Filter 1 Cutoff", 0.6)
    p.filt(0, type="LP 24", cutoff=400, res=0.2)
    p.env(0, att=0.0, dec=t, sus=0.0, rel=0.5)
    reverb(p, r, "Hall", mix=(0.25, 0.4)); macros(p)


@arch(F, "Downlifter", ["Tape Stop", "Slow Down", "Vinyl Stop", "Power Off", "Wind Down"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw", "Analog Saw"]), voices=3, detune=0.2, level=0.5)
    p.env(2, att=0.0, dec=rr(r, 0.8, 2.0, 2), sus=0.0)
    p.mod("Env 3", "Osc A Pitch", rr(r, 0.3, 0.5, 2))
    p.env(0, att=0.0, dec=2.0, sus=0.0, rel=0.3)
    p.set(transpose=12)
    p.fx("tape", wow=0.4, flutter=0.3, sat=0.4, mix=1.0)
    macros(p)


# ------------------------------------------------------------------------------------------- Hit
@arch(F, "Hit", ["Impact", "Boom", "Cinematic Hit", "Trailer Hit", "Earthquake", "Thunder Hit"], role="fx")
def _(p, r, i):
    p.osc(0, wt="Sine", level=0.9, oct=-1)
    p.env(1, dec=rr(r, 0.1, 0.3, 2), sus=0.0); p.mod("Env 2", "Osc A Pitch", rr(r, 0.2, 0.4, 2))
    p.noise(type=pick(r, ["White", "Brown", "Pink"]), level=0.6, route="Filter 1")
    p.env(2, dec=rr(r, 0.2, 0.6, 2), sus=0.0); p.mod("Env 3", "Noise Level", 0.5); p.set(noise_level=0.1)
    p.filt(0, type="LP 24", cutoff=rr(r, 1500, 5000, 0))
    p.env(0, att=0.0, dec=rr(r, 1.5, 4.0, 2), sus=0.0, rel=1.5)
    p.fx("dist", type=pick(r, ["Soft Clip", "Tube"]), drive=rr(r, 0.2, 0.5, 2), mix=0.7)
    reverb(p, r, "Hall", mix=(0.3, 0.5), size=(0.85, 1.0), decay=(4, 9))
    macros(p)


@arch(F, "Hit", ["Braam", "Horn Blast", "Inception Hit", "Monster Brass", "Doom Braam"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Brass", "Saw Stack", "Growl"]), voices=7, detune=0.3, level=0.5, oct=-1)
    p.osc(1, wt="Saw", oct=-2, level=0.4)
    p.filt(0, type="LP 24", cutoff=rr(r, 400, 900, 0), res=0.3, env=0.4)
    p.env(1, att=0.05, dec=1.5, sus=0.3)
    p.env(0, att=0.02, dec=3.0, sus=0.0, rel=2.0)
    p.fx("dist", type="Overdrive", drive=0.5, mix=0.7)
    reverb(p, r, "Hall", mix=(0.3, 0.45), size=(0.9, 1.0))
    macros(p)


@arch(F, "Hit", ["Orchestra Hit", "Stab Hit", "Ork Hit", "80s Ork", "Fairlight Hit"], role="fx")
def _(p, r, i):
    p.sample(pick(r, ["Violins", "Trumpet", "Trombone", "Cellos", "French Horn"]), level=0.7, osc_off=False)
    p.osc(0, wt=pick(r, ["Brass", "Strings", "Saw Stack"]), voices=5, detune=0.2, level=0.35)
    p.chord_gen(mode="Chord per Key", size="Triads", voicing="Spread", bass=1)
    p.env(0, att=0.0, dec=rr(r, 0.4, 0.8, 2), sus=0.0, rel=0.4)
    p.analog(bits=rr(r, 0.0, 0.3, 2))
    reverb(p, r, "Hall", mix=(0.2, 0.35)); macros(p)


# ------------------------------------------------------------------------------------------- Foley
@arch(F, "Foley", ["Footsteps", "Gravel Step", "Wood Floor", "Snow Step", "Leaves"], role="fx")
def _(p, r, i):
    p.osc_off(0)
    p.noise(type=pick(r, ["Brown", "Pink", "Crackle"]), level=0.8, route="Filter 1")
    p.filt(0, type=pick(r, ["LP 24", "BP 12"]), cutoff=rr(r, 400, 2000, 0), res=0.1)
    p.env(0, att=0.002, dec=rr(r, 0.08, 0.2, 2), sus=0.0, rel=0.05)
    p.mod("Random", "Filter 1 Cutoff", 0.15)
    reverb(p, r, "Room", mix=(0.08, 0.16)); macros(p)


@arch(F, "Foley", ["Water Drop", "Drip", "Bubble", "Droplet", "Faucet"], role="fx")
def _(p, r, i):
    p.osc(0, wt="Sine", level=0.7)
    p.env(1, dec=rr(r, 0.03, 0.08, 3), sus=0.0); p.mod("Env 2", "Osc A Pitch", -rr(r, 0.2, 0.4, 2))
    p.env(0, att=0.0, dec=rr(r, 0.08, 0.2, 2), sus=0.0, rel=0.05)
    p.mod("Random", "Osc A Pitch", 0.05)
    reverb(p, r, "Room", mix=(0.15, 0.3)); macros(p, tone=False)


@arch(F, "Foley", ["Clock Tick", "Metronome", "Typewriter", "Ticking", "Clockwork"], role="fx")
def _(p, r, i):
    p.sample("Hand Percussion", level=0.9)
    p.arpeggio(mode="As Played", rate=pick(r, ["1/4", "1/8", "1/8 T"]), gate=0.5)
    p.env(0, att=0.0, dec=0.3, sus=0.0, rel=0.05)
    reverb(p, r, "Room", mix=(0.08, 0.15)); macros(p, tone=False)
    p.add_tags("Hold C#1 or D3")


@arch(F, "Foley", ["Paper", "Cloth Rustle", "Brush", "Sand", "Crinkle"], role="fx")
def _(p, r, i):
    p.osc_off(0)
    p.noise(type=pick(r, ["White", "Crackle", "Digital"]), level=0.7, route="Filter 1")
    p.filt(0, type="BP 12", cutoff=rr(r, 2000, 6000, 0), res=0.2)
    p.lfo(0, shape="S&H", rate=rr(r, 10, 25, 1), mode="Free"); p.mod("LFO 1", "Amp", 0.6)
    p.env(0, att=0.02, dec=0.4, sus=0.6, rel=0.2)
    macros(p)
