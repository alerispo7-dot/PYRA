"""GUITAR: acoustic, electric, distorted, harps, clav, harpsichord, ethnic (ukulele, banjo, mandolin, sitar, lute, lyre, balalaika, dulcimer)."""
from core import *
from registry import arch

G = "Guitar"
REAL = ["clean", "clean", "wide", "vintage", "bright", "dark", "lofi", "fx"]


def gtr(p, r, inst, tone=(5000, 14000), rel=(0.2, 0.5), oct=0):
    p.set(polyphony=12)
    p.sample(inst, level=0.9, vel=rr(r, 0.6, 0.9, 2), oct=oct)
    p.filt(0, type="LP 12", cutoff=rr(r, *tone, 0), res=0.05)
    velocity_tone(p, 0.15)
    p.env(0, att=0.0, dec=2.0, sus=1.0, rel=rr(r, *rel, 2))


def strum(p, r, size=None):
    p.chord_gen(mode="Chord per Key", scale=pick(r, ["Major", "Minor"]), size=size or pick(r, ["Triads", "Sus", "Sevenths"]),
                voicing=pick(r, ["Open", "Spread"]), strum=rr(r, 0.25, 0.6, 2), bass=1)
    p.add_tags("Strummed")


# ------------------------------------------------------------------------------------------- Acoustic Guitar
@arch(G, "Acoustic Guitar", ["Classical Guitar", "Nylon Guitar", "Spanish Guitar", "Flamenco Nylon", "Bossa Nylon"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Classical Guitar")
    reverb(p, r, "Room", mix=(0.08, 0.16), size=(0.3, 0.55))
    if i % 3 == 2: strum(p, r)
    macros(p)


@arch(G, "Acoustic Guitar", ["Archtop Guitar", "Jazz Archtop", "Hollow Body", "Swing Guitar", "Parlor Archtop"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Archtop Guitar", (3500, 9000))
    reverb(p, r, "Room", mix=(0.08, 0.15))
    if i % 3 == 1: strum(p, r, "Sevenths")
    macros(p)


@arch(G, "Acoustic Guitar", ["Folk Guitar", "Campfire Guitar", "Strummed Acoustic", "Fingerstyle", "Travel Guitar"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, pick(r, ["Strumstick", "Classical Guitar", "Archtop Guitar"]))
    p.fx("eq", lowf=150, lowg=-2, highf=6000, highg=rr(r, 1, 3, 1))
    if r.random() < 0.6: strum(p, r, "Triads")
    reverb(p, r, "Room", mix=(0.1, 0.18))
    macros(p)


# ------------------------------------------------------------------------------------------- Electric Guitar
@arch(G, "Electric Guitar", ["Clean Strat", "Clean Electric", "Chorus Guitar", "Pop Clean", "Glass Clean"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Clean Electric Guitar")
    if r.random() < 0.6: chorus(p, r, mix=(0.25, 0.45))
    if r.random() < 0.5: delay(p, r, div="1/8 D", mix=(0.1, 0.18))
    reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p)


@arch(G, "Electric Guitar", ["Twang Guitar", "Surf Guitar", "Country Twang", "Tele Twang", "Spaghetti Western"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Twang Guitar")
    if r.random() < 0.5:
        p.lfo(0, shape="Sine", rate=rr(r, 4.0, 7.0, 2), mode="Free"); p.mod("LFO 1", "Amp", rr(r, 0.15, 0.3, 2))
        p.add_tags("Tremolo")
    reverb(p, r, pick(r, ["Room", "Hall"]), mix=(0.12, 0.25))
    if r.random() < 0.5: delay(p, r, mode="Tape Echo", div="1/8 D", mix=(0.12, 0.2))
    macros(p)


@arch(G, "Electric Guitar", ["Funk Guitar", "Muted Guitar", "Chicken Pick", "Palm Mute", "Disco Guitar"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, pick(r, ["Clean Electric Guitar", "Twang Guitar"]), rel=(0.04, 0.08))
    p.env(0, att=0.0, dec=rr(r, 0.12, 0.3, 2), sus=0.0, rel=0.05)
    if r.random() < 0.5:
        p.filt(0, type="BP 12", cutoff=rr(r, 800, 1600, 0), res=0.35, env=0.4); p.env(1, dec=0.15, sus=0.0)
    p.fx("comp", thresh=-20, ratio=4, makeup=4)
    macros(p)


@arch(G, "Electric Guitar", ["Jazz Guitar", "Warm Jazz", "Smooth Jazz", "Neck Pickup", "Club Jazz"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, pick(r, ["Archtop Guitar", "Clean Electric Guitar"]), (1800, 4000))
    reverb(p, r, "Room", mix=(0.1, 0.18))
    macros(p)


# ------------------------------------------------------------------------------------------- Distorted Guitar
@arch(G, "Distorted Guitar", ["Overdrive Guitar", "Crunch Guitar", "Rock Guitar", "Blues Drive", "Classic Rock"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Overdrive Guitar", (4000, 9000))
    if r.random() < 0.6: p.fx("dist", type=pick(r, ["Tube", "Overdrive"]), drive=rr(r, 0.2, 0.45, 2), mix=0.6)
    reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p, extra=[("Macro 2", "Filter 1 Drive", 0.3)])


@arch(G, "Distorted Guitar", ["Power Chords", "Metal Chug", "Riff Machine", "Stadium Power", "Punk Power"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, pick(r, ["Overdrive Guitar", "Twang Guitar", "Clean Electric Guitar"]), (3500, 8000))
    p.fx("dist", type=pick(r, ["Overdrive", "Hard Clip", "Tube", "Diode"]), drive=rr(r, 0.5, 0.8, 2), tone=rr(r, 0.4, 0.7, 2), mix=1.0)
    p.chord_gen(mode="Chord per Key", size="Power", voicing="Close", strum=rr(r, 0.0, 0.1, 2), bass=0)
    p.fx("eq", lowf=100, lowg=-3, midf=rr(r, 600, 1200, 0), midg=rr(r, -3, 3, 1), highf=7000, highg=-4)
    macros(p)
    p.add_tags("Power Chords")


@arch(G, "Distorted Guitar", ["Synth Guitar", "Fuzz Lead", "Screaming Guitar", "Feedback Lead", "Distortion Synth"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.04, glideLegato=1)
    p.osc(0, wt=pick(r, ["PD Reso", "Saw", "Harmonic Sweep"]), pos=0.3, level=0.7)
    p.osc(1, wt="Square", oct=-1, level=0.3, fine=5)
    p.filt(0, type="Peak", cutoff=rr(r, 1500, 3000, 0), res=0.4)
    p.fx("dist", type=pick(r, ["Overdrive", "Hard Clip", "Diode"]), drive=rr(r, 0.6, 0.9, 2), tone=0.5, mix=1.0)
    p.fx("eq", highf=6000, highg=-6, lowf=100, lowg=-3)
    vibrato(p, r, cents=(15, 30), delay=(0.4, 0.8))
    reverb(p, r, "Room", mix=(0.1, 0.18)); delay(p, r)
    macros(p)
    p.add_tags("Synth")


# ------------------------------------------------------------------------------------------- Harps
@arch(G, "Harps", ["Concert Harp", "Orchestral Harp", "Harp", "Angel Harp", "Gliss Harp"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Harp", rel=(0.5, 1.0))
    if i % 3 == 2:
        p.arpeggio(mode="Up", rate=pick(r, ["1/32", "1/16 T"]), oct=3, gate=0.9, scale=pick(r, ["Major", "Lydian", "Major Pentatonic"]))
        p.add_tags("Glissando")
    reverb(p, r, "Hall", mix=(0.18, 0.3))
    macros(p)


@arch(G, "Harps", ["Folk Harp", "Celtic Harp", "Irish Harp", "Lap Harp", "Bardic Harp"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Folk Harp", rel=(0.4, 0.8))
    reverb(p, r, "Hall", mix=(0.15, 0.28))
    macros(p)


@arch(G, "Harps", ["Synth Harp", "Crystal Harp", "Dream Harp", "Space Harp"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Glass", "Harmonic Sweep", "PD Reso"]), pos=0.4, level=0.6)
    p.filt(0, type="LP 12", cutoff=rr(r, 1500, 4000, 0), env=0.4, key=0.6)
    p.env(1, dec=0.3, sus=0.0)
    p.env(0, att=0.0, dec=rr(r, 1.0, 2.5, 2), sus=0.0, rel=1.0)
    reverb(p, r, pick(r, ["Hall", "Shimmer"]), mix=(0.25, 0.4)); delay(p, r, mode="Ping-Pong")
    macros(p)


# ------------------------------------------------------------------------------------------- Clav
@arch(G, "Clav", ["Clavinet", "Funk Clav", "Clavi", "Stevie Clav", "Clav Groove"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Clavi", rel=(0.05, 0.12))
    if r.random() < 0.5:
        p.filt(0, type="BP 12", cutoff=rr(r, 700, 1400, 0), res=0.35, env=0.35); p.env(1, dec=0.2, sus=0.1)
        p.add_tags("Wah")
    if r.random() < 0.5: p.fx("pha", rate=rr(r, 0.3, 0.8, 2), depth=0.5, mix=0.35)
    macros(p)


@arch(G, "Clav", ["Synth Clav", "Pulse Clav", "Wah Clav", "Envelope Clav", "Auto-Wah Clav"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Square PWM", "Analog PWM"]), pos=rr(r, 0.75, 0.95, 2), level=0.7)
    p.filt(0, type=pick(r, ["BP 12", "Peak", "LP 24"]), cutoff=rr(r, 800, 1800, 0), res=rr(r, 0.35, 0.6, 2), env=0.45)
    p.env(1, dec=0.15, sus=0.05)
    p.env(0, att=0.0, dec=rr(r, 0.4, 0.8, 2), sus=0.0, rel=0.06)
    p.mod("Velocity", "Filter 1 Env Amt", 0.3)
    p.fx("comp", thresh=-20, ratio=4, makeup=3)
    macros(p)


# ------------------------------------------------------------------------------------------- Harpsichord
@arch(G, "Harpsichord", ["Harpsichord", "Baroque Harpsichord", "Flemish Harpsichord", "Bach Harpsichord", "Cembalo"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Harpsichord", rel=(0.2, 0.35))
    reverb(p, r, pick(r, ["Room", "Hall"]), mix=(0.1, 0.2))
    if r.random() < 0.3: p.set(smp_oct=1)
    macros(p)


@arch(G, "Harpsichord", ["Synth Harpsichord", "Lute Stop", "Buff Stop", "Plucked Keys", "Space Harpsichord"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Square PWM", "PD Saw", "Analog PWM"]), pos=rr(r, 0.8, 0.95, 2), level=0.6)
    p.osc(1, wt="Saw", oct=1, level=0.3)
    p.filt(0, type="HP 12", cutoff=200)
    p.env(0, att=0.0, dec=rr(r, 0.8, 1.5, 2), sus=0.0, rel=0.2)
    reverb(p, r, "Room", mix=(0.1, 0.18))
    macros(p, tone=False)


# ------------------------------------------------------------------------------------------- Ethnic Guitar
@arch(G, "Ethnic Guitar", ["Ukulele", "Hawaiian Uke", "Beach Ukulele", "Island Uke", "Strummed Uke"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Ukulele")
    if i % 2 == 1: strum(p, r, "Triads")
    reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p)


@arch(G, "Ethnic Guitar", ["Banjo", "Bluegrass Banjo", "Country Banjo", "Frailing Banjo", "Banjo Roll"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Banjo")
    if i % 3 == 2:
        p.arpeggio(mode="Up-Down", rate="1/16", oct=1, gate=0.7)
        p.chord_gen(mode="Chord per Key", size="Triads", voicing="Open"); p.add_tags("Roll")
    reverb(p, r, "Room", mix=(0.06, 0.14))
    macros(p)


@arch(G, "Ethnic Guitar", ["Mandolin", "Mandolin Tremolo", "Neapolitan Mandolin", "Folk Mandolin", "Bouzouki"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Strumstick", (6000, 14000), oct=pick(r, [0, 1]))
    if i % 2 == 0:
        p.arpeggio(mode="As Played", rate=pick(r, ["1/32", "1/16 T"]), gate=0.6, oct=1)
        p.add_tags("Tremolo")
    chorus(p, r, mix=(0.1, 0.2), depth=(0.1, 0.25))
    reverb(p, r, "Room", mix=(0.08, 0.15))
    macros(p)
    p.add_tags("Emulated")


@arch(G, "Ethnic Guitar", ["Balalaika", "Russian Balalaika", "Domra", "Steppe Strings"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, pick(r, ["Strumstick", "Ukulele"]), (5000, 12000), oct=pick(r, [0, 1]))
    p.arpeggio(mode="As Played", rate="1/32", gate=0.5)
    reverb(p, r, "Room", mix=(0.1, 0.18))
    macros(p)
    p.add_tags("Tremolo", "Emulated")


@arch(G, "Ethnic Guitar", ["Sitar", "Sitar Drone", "Raga Strings", "Indian Sitar", "Sympathetic Sitar"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, pick(r, ["Zither", "Dan Tranh", "Strumstick"]), (5000, 11000))
    p.fx("res", chord=pick(r, ["Fifth", "Octaves", "Single"]), note=pick(r, [48, 50, 52, 55]), decay=rr(r, 0.7, 0.92, 2), bright=rr(r, 0.6, 0.85, 2), mix=rr(r, 0.3, 0.5, 2))
    p.fx("dist", type="Fold", drive=rr(r, 0.1, 0.25, 2), mix=0.3)   # the buzz of the jawari bridge
    p.env(2, att=0.0, dec=0.08, sus=0.0); p.mod("Env 3", "Filter 1 Cutoff", 0.15)
    p.set(bendUp=2, glide=rr(r, 0.04, 0.1, 3))
    reverb(p, r, "Hall", mix=(0.15, 0.25))
    macros(p)
    p.add_tags("Emulated")


@arch(G, "Ethnic Guitar", ["Lute", "Renaissance Lute", "Oud", "Baroque Lute", "Minstrel Lute"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, pick(r, ["Classical Guitar", "Folk Harp", "Medieval Lyre"]), (2500, 5000))
    p.fx("eq", lowf=200, lowg=-3, midf=1200, midg=2, highf=6000, highg=-3)
    reverb(p, r, "Room", mix=(0.12, 0.2))
    macros(p)
    p.add_tags("Emulated")


@arch(G, "Ethnic Guitar", ["Medieval Lyre", "Lyre", "Cithara", "Greek Lyre", "Bard Lyre"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Medieval Lyre", rel=(0.4, 0.8))
    reverb(p, r, "Hall", mix=(0.15, 0.28))
    macros(p)


@arch(G, "Ethnic Guitar", ["Dulcimer", "Zither", "Hammered Dulcimer", "Citera", "Psaltery"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Zither", rel=(0.4, 0.8))
    if r.random() < 0.4: chorus(p, r, mix=(0.15, 0.3))
    reverb(p, r, "Room", mix=(0.1, 0.2))
    macros(p)


@arch(G, "Ethnic Guitar", ["Koto", "Dan Tranh", "Guzheng", "Zen Koto", "Pentatonic Koto"], role="real", chars=REAL)
def _(p, r, i):
    gtr(p, r, "Dan Tranh", rel=(0.4, 0.8))
    p.set(bendUp=2)
    reverb(p, r, "Hall", mix=(0.15, 0.25))
    macros(p)
