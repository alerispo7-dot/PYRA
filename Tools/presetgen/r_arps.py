"""ARPS (with the sequences): simple, complex, phrase, accent, pulsating."""
from core import *
from registry import arch

A = "Arps"
SCALES = ["Minor", "Major", "Dorian", "Minor Pentatonic", "Major Pentatonic", "Phrygian", "Lydian", "Harmonic Minor", "Mixolydian", "Hirajoshi"]


def arp_sound(p, r, kind=None):
    """A sound that arpeggiates well. Returns the kind (it goes in the name)."""
    kind = kind or pick(r, ["pluck", "bell", "saw", "square", "fm", "ep", "mallet", "harp", "glass", "acid", "kalimba", "guitar"])
    if kind == "pluck":
        p.shape(0, "saw", voices=pick(r, [1, 3]), detune=0.15, level=0.65)
        p.filt(0, type="LP 24", cutoff=rr(r, 400, 1000, 0), res=rr(r, 0.15, 0.4, 2), env=0.5); p.env(1, dec=rr(r, 0.1, 0.25, 2), sus=0.0)
        p.env(0, att=0.0, dec=0.3, sus=0.0, rel=0.2)
    elif kind == "bell":
        p.osc(0, wt=pick(r, ["FM Bell", "FM Chorus Bell", "Glass"]), pos=rr(r, 0.2, 0.6, 2), level=0.65)
        p.env(0, att=0.0, dec=rr(r, 0.4, 1.0, 2), sus=0.0, rel=0.5)
    elif kind == "saw":
        p.osc(0, wt=pick(r, ["Saw Stack", "Analog Saw"]), voices=5, detune=0.25, level=0.55)
        p.filt(0, type="LP 24", cutoff=rr(r, 1500, 5000, 0), res=0.15, env=0.25); p.env(1, dec=0.2, sus=0.2)
        p.env(0, att=0.0, dec=0.3, sus=0.4, rel=0.15)
    elif kind == "square":
        p.osc(0, wt=pick(r, ["Square", "Square PWM", "Chip"]), pos=rr(r, 0.2, 0.6, 2), level=0.55)
        p.filt(0, type="LP 12", cutoff=rr(r, 2000, 6000, 0)); p.env(0, att=0.0, dec=0.25, sus=0.3, rel=0.1)
    elif kind == "fm":
        p.osc(0, wt=pick(r, ["FM 2:1", "FM 3:1", "FM E-Piano", "FM Feedback"]), pos=rr(r, 0.2, 0.6, 2), level=0.6)
        p.env(2, dec=0.15, sus=0.0); p.mod("Env 3", "Osc A WT Pos", 0.3)
        p.env(0, att=0.0, dec=0.4, sus=0.0, rel=0.2)
    elif kind == "ep":
        p.sample(pick(r, ["Wurlitzer", "Electric Grand", "FM E-Piano", "Pianet"]), level=0.85); p.env(0, att=0.0, dec=1.0, sus=1.0, rel=0.2)
    elif kind == "mallet":
        p.sample(pick(r, ["Marimba", "Vibraphone", "Xylophone", "Glockenspiel", "Balafon"]), level=0.85); p.env(0, att=0.0, dec=1.0, sus=1.0, rel=0.3)
    elif kind == "harp":
        p.sample(pick(r, ["Harp", "Folk Harp", "Medieval Lyre", "Dan Tranh"]), level=0.85); p.env(0, att=0.0, dec=1.0, sus=1.0, rel=0.4)
    elif kind == "kalimba":
        p.sample(pick(r, ["Kalimba", "Steel Pan", "Hang Drum", "Hand Chimes"]), level=0.85); p.env(0, att=0.0, dec=1.0, sus=1.0, rel=0.4)
    elif kind == "guitar":
        p.sample(pick(r, ["Classical Guitar", "Clean Electric Guitar", "Twang Guitar", "Banjo", "Ukulele"]), level=0.85); p.env(0, att=0.0, dec=1.0, sus=1.0, rel=0.2)
    elif kind == "glass":
        p.osc(0, wt=pick(r, ["Glass", "FM 1:9 Glass", "Octaves"]), level=0.6); p.osc(1, wt="Sine", oct=1, level=0.2)
        p.env(0, att=0.0, dec=0.5, sus=0.0, rel=0.4)
    else:   # acid
        p.shape(0, pick(r, ["saw", "square"]), level=0.75)
        p.filt(0, type=pick(r, ["LP Ladder", "Diode LP"]), cutoff=rr(r, 300, 700, 0), res=rr(r, 0.65, 0.85, 2), env=0.5, drive=0.3)
        p.env(1, dec=0.15, sus=0.0); p.env(0, att=0.0, dec=0.3, sus=0.5, rel=0.08)
        p.fx("dist", type="Soft Clip", drive=0.3, mix=0.5)
    return kind


NAMES = {"pluck": "Pluck", "bell": "Bell", "saw": "Saw", "square": "Square", "fm": "FM", "ep": "EP", "mallet": "Mallet", "harp": "Harp",
         "kalimba": "Kalimba", "guitar": "Guitar", "glass": "Glass", "acid": "Acid"}


def arp_space(p, r):
    delay(p, r, div=pick(r, ["1/8 D", "1/4 D", "1/16 D", "1/8"]), mix=(0.1, 0.22))
    reverb(p, r, "Hall", mix=(0.1, 0.22))


# ------------------------------------------------------------------------------------------- Simple
@arch(A, "Simple Arpeggios", ["Up Arp", "Classic Arp", "Arp Up", "Rising Arp", "Simple Arp", "Starter Arp"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r)
    p.arpeggio(mode=pick(r, ["Up", "Up", "Down", "Up-Down", "Down-Up"]), rate=pick(r, ["1/16", "1/8", "1/16", "1/8 T"]), oct=pick(r, [1, 2, 2, 3]), gate=rr(r, 0.4, 0.8, 2))
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Arp")


@arch(A, "Simple Arpeggios", ["Chord Arp", "Broken Chord", "Arpeggiated Chord", "One Finger Arp", "Auto Arp"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r)
    p.chord_gen(mode="Chord per Key", scale=pick(r, ["Minor", "Major"]), size=pick(r, ["Triads", "Sevenths", "Sus"]), voicing="Open", bass=0)
    p.arpeggio(mode=pick(r, ["Up", "Up-Down", "As Played"]), rate=pick(r, ["1/16", "1/8"]), oct=pick(r, [1, 2]), gate=rr(r, 0.4, 0.7, 2))
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Chords")


# ------------------------------------------------------------------------------------------- Complex
@arch(A, "Complex Arpeggios", ["Random Arp", "Generative Arp", "Chance Arp", "Wandering Arp", "Dice Arp"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r)
    p.arpeggio(mode="Random", rate=pick(r, ["1/16", "1/8", "1/16 T"]), oct=pick(r, [2, 3, 4]), gate=rr(r, 0.3, 0.7, 2), scale=pick(r, SCALES), swing=rr(r, 0.0, 0.3, 2))
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Generative")


@arch(A, "Complex Arpeggios", ["Polyrhythm Arp", "Odd Meter Arp", "Shifting Arp", "Cross Arp", "5 Against 4"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r)
    n = pick(r, [5, 7, 9, 11, 13])
    pat = [pick(r, [0, 2, 4, 7, 9, 11, -3, 12, 5]) for _ in range(n)]
    p.arpeggio(pattern=pat, mode="Sequencer", rate=pick(r, ["1/16", "1/16 T", "1/8"]), steps=n, gate=rr(r, 0.4, 0.7, 2), scale=pick(r, SCALES))
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Polyrhythm")


@arch(A, "Complex Arpeggios", ["Cascade Arp", "Spiral Arp", "Octave Leap", "Mountain Arp", "Kaleidoscope"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r)
    p.chord_gen(mode="Progression", mood=pick(r, PARAMS["chord_mood"]["choices"][1:]), count=4, len=pick(r, ["1 Bar", "2 Bars"]),
                size=pick(r, ["Sevenths", "Ninths", "Mixed"]), voicing="Spread")
    p.arpeggio(mode=pick(r, ["Up-Down", "Down-Up", "Up", "Random"]), rate=pick(r, ["1/16", "1/16 T", "1/32"]), oct=pick(r, [2, 3]), gate=rr(r, 0.3, 0.6, 2))
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Progression")


# ------------------------------------------------------------------------------------------- Phrase
def phrase_pattern(r, n=16, rest=0.2, tie=0.12):
    motif = [pick(r, [0, 2, 3, 4, 7, 5, -2, 9, 12]) for _ in range(4)]
    pat = []
    for s in range(n):
        if r.random() < rest: pat.append(None); continue
        d = motif[s % 4] + (pick(r, [0, 0, 2, -2, 5]) if s >= n // 2 else 0)
        pat.append((d, rr(r, 0.55, 1.0, 2), rr(r, 0.5, 1.0, 2), 1 if r.random() < tie else 0))
    return pat


@arch(A, "Phrase", ["Melody Phrase", "Hook Phrase", "Riff", "Motif", "Topline Phrase", "Ostinato"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r)
    p.arpeggio(pattern=phrase_pattern(r), mode="Sequencer", rate=pick(r, ["1/16", "1/8", "1/16"]), steps=pick(r, [8, 16, 16]), gate=rr(r, 0.5, 0.9, 2),
               scale=pick(r, SCALES), swing=rr(r, 0.0, 0.35, 2))
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Sequence")


@arch(A, "Phrase", ["Bass Sequence", "Bassline Phrase", "Groove Line", "Rolling Bassline", "Synthwave Bass"], role="bass")
def _(p, r, i):
    p.set(voiceMode="Mono", polyphony=1)
    p.shape(0, pick(r, ["saw", "square"]), level=0.75)
    p.sub(shape="Sine", oct=-1, level=0.4, route="Direct")
    p.filt(0, type=pick(r, ["LP 24", "LP Ladder"]), cutoff=rr(r, 300, 800, 0), res=0.25, env=0.4); p.env(1, dec=0.15, sus=0.1)
    p.env(0, att=0.0, dec=0.3, sus=0.6, rel=0.06)
    pat = phrase_pattern(r, 16, rest=0.15, tie=0.08)
    p.arpeggio(pattern=[(s[0] - 12 if s and s[0] > 7 else (s[0] if s else 0), s[1], s[2], s[3]) if s else None for s in pat],
               mode="Sequencer", rate="1/16", steps=16, gate=rr(r, 0.4, 0.7, 2), scale=pick(r, ["Minor", "Dorian", "Phrygian", "Minor Pentatonic"]))
    delay(p, r, div="1/8 D", mix=(0.05, 0.12)); macros(p)
    p.add_tags("Bass", "Sequence")


@arch(A, "Phrase", ["Chord Phrase", "Progression Arp", "Song Arp", "Arp Story", "Changing Arp"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r)
    p.chord_gen(mode="Progression", mood=pick(r, PARAMS["chord_mood"]["choices"][1:]), count=pick(r, [4, 8]), len="1 Bar", size=pick(r, ["Triads", "Sevenths"]))
    p.arpeggio(mode=pick(r, ["Up", "As Played", "Up-Down"]), rate=pick(r, ["1/8", "1/16"]), oct=pick(r, [1, 2]), gate=rr(r, 0.5, 0.8, 2))
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Progression")


# ------------------------------------------------------------------------------------------- Accent
def accent_pattern(r, n=16):
    pat = []
    for s in range(n):
        acc = (s % 4 == 0) or r.random() < 0.2
        pat.append((pick(r, [0, 0, 7, 12, 3, 5]), 1.0 if acc else rr(r, 0.35, 0.55, 2), 0.9 if acc else 0.4, 0))
    return pat


@arch(A, "Accent", ["Accent Arp", "Punch Arp", "Stab Arp", "Dynamic Arp", "Hit Arp"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r, pick(r, ["pluck", "saw", "acid", "square", "fm"]))
    p.mod("Velocity", "Filter 1 Cutoff", rr(r, 0.25, 0.45, 2)).mod("Velocity", "Amp", 0.2)
    p.arpeggio(pattern=accent_pattern(r), mode="Sequencer", rate="1/16", steps=16, gate=0.6, scale=pick(r, SCALES), velamt=1.0)
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Accent")


@arch(A, "Accent", ["Swing Accent", "Shuffle Arp", "Funky Arp", "Groove Arp", "Bounce Arp"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r)
    p.mod("Velocity", "Filter 1 Cutoff", 0.3)
    p.arpeggio(pattern=accent_pattern(r, 8), mode="Sequencer", rate=pick(r, ["1/16", "1/8"]), steps=8, gate=rr(r, 0.4, 0.7, 2), scale=pick(r, SCALES), swing=rr(r, 0.35, 0.65, 2))
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Swing")


# ------------------------------------------------------------------------------------------- Pulsating
@arch(A, "Pulsating", ["Pulse Sequence", "Throb", "Repeat Pulse", "Machine Pulse", "Heartbeat Seq", "Pump Seq"], role="lead")
def _(p, r, i):
    k = arp_sound(p, r, pick(r, ["saw", "pluck", "square", "acid", "fm"]))
    pat = [(0, 1.0 if s % 4 == 0 else 0.7, 0.5, 0) for s in range(16)]
    p.arpeggio(pattern=pat, mode="Sequencer", rate=pick(r, ["1/16", "1/8", "1/16 T"]), steps=16, gate=rr(r, 0.3, 0.6, 2), scale="Chromatic")
    p.chord_gen(mode="Chord per Key", size=pick(r, ["Triads", "Power", "Sus"]), voicing="Close", bass=0)
    p.set(motion_amt=rr(r, 0.3, 0.6, 2), motion_shape=pick(r, ["Pump", "Filter Sweep"]), motion_rate=pick(r, ["1/4", "1 Bar", "1/2"]))
    arp_space(p, r); macros(p)
    p.add_tags(NAMES[k], "Pulse")


@arch(A, "Pulsating", ["Gated Sequence", "Trance Gate", "Chopped Seq", "Stutter Seq", "Glitch Pulse"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw Stack", "Analog Saw", "Soft Saw", "Choir"]), voices=5, detune=0.3, level=0.5)
    p.filt(0, type="LP 24", cutoff=rr(r, 2000, 6000, 0))
    p.env(0, att=0.01, dec=1.0, sus=1.0, rel=0.3)
    p.set(motion_amt=rr(r, 0.7, 1.0, 2), motion_shape=pick(r, ["Trance Gate", "Gate", "Stutter", "Random Steps"]), motion_rate=pick(r, ["1/16", "1/16 D", "1/8 T", "1/32"]))
    p.chord_gen(mode="Chord per Key", size=pick(r, ["Triads", "Sevenths", "Sus"]), voicing="Open", bass=0)
    if r.random() < 0.4: p.fx("glt", rate="1/16", chance=rr(r, 0.15, 0.35, 2), mix=0.8)
    arp_space(p, r); macros(p)
    p.add_tags("Gate")


@arch(A, "Pulsating", ["Pulse Bass Seq", "Driving Eighths", "Octave Pump", "Synthwave Pulse", "Night Drive"], role="bass")
def _(p, r, i):
    p.set(voiceMode="Mono", polyphony=1)
    p.osc(0, wt=pick(r, ["Analog Saw", "Saw"]), voices=2, detune=0.08, level=0.7)
    p.sub(shape="Square", oct=-1, level=0.35)
    p.filt(0, type="LP 24", cutoff=rr(r, 500, 1200, 0), res=0.2, env=0.3); p.env(1, dec=0.12, sus=0.1)
    p.env(0, att=0.0, dec=0.2, sus=0.5, rel=0.05)
    pat = [(0 if s % 2 == 0 else pick(r, [12, 0, 7]), 0.9 if s % 4 == 0 else 0.7, 0.6, 0) for s in range(16)]
    p.arpeggio(pattern=pat, mode="Sequencer", rate=pick(r, ["1/8", "1/16"]), steps=16, gate=0.55, scale="Chromatic")
    delay(p, r, div="1/8 D", mix=(0.05, 0.12)); macros(p)
    p.add_tags("Bass", "Pulse")
