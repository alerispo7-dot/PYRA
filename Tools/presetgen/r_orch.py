"""STRINGS, BRASS, WIND, VOX."""
from core import *
from registry import arch

ST, BR, WD, VX = "Strings", "Brass", "Winds", "Vox"
REAL = ["clean", "clean", "wide", "dark", "bright", "vintage", "lofi", "fx"]


def bowed(p, r, inst, att=(0.08, 0.3), rel=(0.3, 0.8), oct=0, poly=16):
    p.set(polyphony=poly)
    p.sample(inst, level=0.85, oct=oct, vel=rr(r, 0.4, 0.8, 2))
    p.filt(0, type="LP 12", cutoff=rr(r, 5000, 14000, 0), res=0.05)
    velocity_tone(p, 0.15)
    p.env(0, att=rr(r, *att, 2), dec=1.0, sus=1.0, rel=rr(r, *rel, 2))
    p.mod("Mod Wheel", "Filter 1 Cutoff", 0.2)


def hall(p, r, mix=(0.15, 0.3)):
    reverb(p, r, "Hall", mix=mix, size=(0.6, 0.95), decay=(2.0, 4.5), pre=(10, 35))


def solo_expr(p, r, legato=True):
    if legato: p.set(voiceMode="Legato", polyphony=1, glide=rr(r, 0.03, 0.08, 3), glideLegato=1)
    vibrato(p, r, cents=(6, 14), delay=(0.3, 0.7))


# ------------------------------------------------------------------------------------------- STRINGS
@arch(ST, "Solo Strings", ["Solo Violin", "Concert Violin", "Romantic Violin", "Fiddle", "Chamber Violin"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, "Solo Violin", poly=4)
    if i % 2 == 0: solo_expr(p, r)
    hall(p, r)
    macros(p)


@arch(ST, "Solo Strings", ["Erhu", "Chinese Fiddle", "Two-String Fiddle", "Silk Erhu", "Mountain Erhu"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, "Erhu", poly=4)
    solo_expr(p, r)
    reverb(p, r, "Hall", mix=(0.12, 0.22))
    macros(p)


@arch(ST, "Solo Strings", ["Synth Violin", "Lead Strings", "Bowed Synth", "Gypsy Synth"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.05, glideLegato=1)
    p.osc(0, wt=pick(r, ["Strings", "Saw", "PD Saw"]), pos=0.4, level=0.7)
    p.noise(type="Pink", level=0.04, route="Filter 1")
    p.filt(0, type="LP 24", cutoff=rr(r, 2000, 4500, 0), res=0.2, key=0.5)
    p.filt(1, type="Peak", cutoff=rr(r, 1500, 3000, 0), res=0.4)
    p.env(0, att=0.12, dec=0.5, sus=0.9, rel=0.4)
    vibrato(p, r, cents=(10, 20))
    hall(p, r)
    macros(p)


@arch(ST, "Ensemble Strings", ["Violin Section", "Violins", "Strings Ensemble", "Chamber Strings", "Lush Violins"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, "Violins")
    hall(p, r); macros(p)


@arch(ST, "Ensemble Strings", ["Viola Section", "Violas", "Alto Strings", "Warm Section", "Mid Strings"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, "Violas")
    hall(p, r); macros(p)


@arch(ST, "Ensemble Strings", ["Pizzicato", "Pizz Section", "Plucked Strings", "Pizz Ensemble", "Staccato Pizz"], role="real", chars=REAL)
def _(p, r, i):
    p.set(polyphony=16)
    p.sample("Pizzicato Strings", level=0.9)
    p.env(0, att=0.0, dec=1.5, sus=1.0, rel=0.25)
    velocity_tone(p, 0.15)
    hall(p, r, (0.12, 0.22)); macros(p)


@arch(ST, "Ensemble Strings", ["String Machine", "Synth Strings", "Solina", "Disco Strings", "80s Strings"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw", "Analog Saw", "Strings"]), voices=pick(r, [3, 5]), detune=0.2, level=0.55)
    p.osc(1, wt="Saw", oct=1, level=0.3, fine=6)
    p.filt(0, type="LP 12", cutoff=rr(r, 2500, 6000, 0))
    p.filt(1, type="HP 12", cutoff=200)
    p.env(0, att=rr(r, 0.05, 0.3, 2), dec=1.0, sus=0.9, rel=0.6)
    p.fx("cho", depth=0.8, rate=0.6, mix=0.55); hall(p, r)
    macros(p)


@arch(ST, "Orchestral Strings", ["Cinematic Strings", "Epic Strings", "Score Strings", "Film Strings", "Symphonic Strings"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, pick(r, ["Violins", "Violas", "Cellos"]), att=(0.2, 0.6))
    p.osc(0, wt="Strings", voices=5, detune=0.2, level=0.2, oct=pick(r, [0, -1]))
    reverb(p, r, "Hall", mix=(0.25, 0.4), size=(0.8, 1.0), decay=(3, 6))
    macros(p, extra=[("Macro 2", "Osc A Level", 0.3)])


@arch(ST, "Orchestral Strings", ["Tremolo Strings", "Tension Strings", "Horror Tremolo", "Suspense Strings", "Thriller Strings"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, pick(r, ["Violins", "Violas", "Cellos"]))
    p.lfo(0, shape=pick(r, ["Triangle", "Sine"]), rate=rr(r, 9, 14, 2), mode="Free")
    p.mod("LFO 1", "Amp", rr(r, 0.3, 0.5, 2))
    hall(p, r, (0.2, 0.35)); macros(p)
    p.add_tags("Tremolo")


@arch(ST, "Orchestral Strings", ["Swell Strings", "Crescendo Strings", "Rising Strings", "Slow Strings", "Adagio"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, pick(r, ["Violins", "Cellos", "Violas"]), att=(0.8, 2.0), rel=(1.0, 2.0))
    hall(p, r, (0.25, 0.4)); macros(p)


@arch(ST, "Orchestral Strings", ["Contrabass Section", "Low Strings", "Basses", "Deep Strings", "Bass Section"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, "Contrabass")
    hall(p, r); macros(p)


@arch(ST, "Solo Cello", ["Solo Cello", "Romantic Cello", "Concert Cello", "Elegy Cello", "Sad Cello"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, "Solo Cello", poly=4)
    if i % 2 == 0: solo_expr(p, r)
    hall(p, r); macros(p)


@arch(ST, "Solo Cello", ["Cello Pizzicato", "Plucked Cello", "Jazz Cello Pizz", "Cello Pluck"], role="real", chars=REAL)
def _(p, r, i):
    # a real pizzicato: the plucked double bass up in its cello register, or the section pizz down low
    p.set(polyphony=6)
    p.sample(pick(r, ["Upright Bass", "Pizzicato Strings"]), level=0.9, vel=rr(r, 0.5, 0.8, 2))
    p.filt(0, type="LP 12", cutoff=rr(r, 2500, 6000, 0), res=0.05)
    p.env(0, att=0.0, dec=rr(r, 0.5, 1.0, 2), sus=0.0, rel=0.25)
    reverb(p, r, "Room", mix=(0.1, 0.2)); macros(p)
    p.add_tags("Emulated")


@arch(ST, "Solo Cello", ["Synth Cello", "Cello Lead", "Dark Bow", "Gothic Cello"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.05, glideLegato=1)
    p.osc(0, wt=pick(r, ["Strings", "PD Saw", "Saw"]), pos=0.5, level=0.7, oct=-1)
    p.filt(0, type="LP 24", cutoff=rr(r, 1200, 2500, 0), res=0.2)
    p.filt(1, type="Peak", cutoff=rr(r, 400, 800, 0), res=0.3)
    p.env(0, att=0.15, dec=0.5, sus=0.9, rel=0.5)
    vibrato(p, r)
    hall(p, r); macros(p)


@arch(ST, "Ensemble Cello", ["Cello Section", "Cellos", "Warm Cellos", "Low Section", "Cello Ensemble"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, "Cellos")
    hall(p, r); macros(p)


@arch(ST, "Ensemble Cello", ["Cellos & Basses", "Low End Strings", "Cinematic Lows", "Dark Cellos", "Doom Strings"], role="real", chars=REAL)
def _(p, r, i):
    bowed(p, r, "Cellos", oct=pick(r, [0, -1]))
    p.sub(shape="Sine", oct=-1, level=rr(r, 0.15, 0.3, 2))
    reverb(p, r, "Hall", mix=(0.25, 0.4), size=(0.8, 1.0))
    macros(p)


# ------------------------------------------------------------------------------------------- BRASS
def blow(p, r, inst, att=(0.02, 0.08), rel=(0.2, 0.4), poly=8, oct=0):
    p.set(polyphony=poly)
    p.sample(inst, level=0.85, vel=rr(r, 0.5, 0.85, 2), oct=oct)
    p.filt(0, type="LP 12", cutoff=rr(r, 3000, 10000, 0), res=0.05)
    p.mod("Velocity", "Filter 1 Cutoff", 0.2).mod("Mod Wheel", "Filter 1 Cutoff", 0.25)
    p.env(0, att=rr(r, *att, 3), dec=1.0, sus=1.0, rel=rr(r, *rel, 2))


BRASS_INST = ["Trumpet", "French Horn", "Trombone", "Tuba", "Muted Trumpet"]


@arch(BR, "Solo Brass", ["Solo Brass", "Brass Solo", "Fanfare Solo", "Lone Brass", "Concert Brass"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, BRASS_INST[i % len(BRASS_INST)], poly=2)
    solo_expr(p, r)
    hall(p, r); macros(p)


@arch(BR, "Ensemble Brass", ["Brass Section", "Brass Ensemble", "Horn Section", "Big Band Brass", "Soul Brass"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, pick(r, ["Trumpet", "Trombone", "French Horn"]))
    p.osc(0, wt="Brass", voices=3, detune=0.15, level=0.2)
    p.fx("cho", depth=0.25, rate=0.4, mix=0.3)
    hall(p, r); macros(p, extra=[("Macro 2", "Osc A Level", 0.3)])


@arch(BR, "Ensemble Brass", ["Brass Stabs", "Funk Brass", "Brass Hits", "Horn Stabs", "Ska Brass"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, pick(r, ["Trumpet", "Trombone"]), att=(0.0, 0.01), rel=(0.08, 0.15))
    p.env(0, att=0.0, dec=rr(r, 0.2, 0.4, 2), sus=0.2, rel=0.1)
    p.chord_gen(mode="Chord per Key", size=pick(r, ["Triads", "Sevenths"]), voicing="Close", bass=0)
    p.fx("comp", thresh=-18, ratio=4, makeup=3)
    reverb(p, r, "Room", mix=(0.08, 0.16)); macros(p)


@arch(BR, "Orchestral Brass", ["Epic Horns", "Heroic Brass", "Cinematic Brass", "Battle Horns", "Trailer Brass"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, pick(r, ["French Horn", "Trombone", "Tuba"]), att=(0.05, 0.2))
    p.osc(0, wt="Brass", voices=5, detune=0.2, level=0.2, oct=-1)
    reverb(p, r, "Hall", mix=(0.25, 0.4), size=(0.8, 1.0), decay=(3, 6))
    macros(p)


@arch(BR, "Orchestral Brass", ["Brass Swell", "Brass Crescendo", "Low Brass", "Brass Choir", "Chorale Brass"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, pick(r, ["Trombone", "French Horn", "Tuba"]), att=(0.4, 1.0), rel=(0.5, 1.0))
    hall(p, r, (0.25, 0.4)); macros(p)


@arch(BR, "Synth Brass", ["Synth Brass", "80s Brass", "OB Brass", "Jump Brass", "Analog Brass", "CS Brass"], role="lead")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw", "Analog Saw", "Brass"]), voices=pick(r, [2, 3]), detune=0.12, level=0.6)
    p.osc(1, wt="Saw", fine=rr(r, 5, 10, 1), level=0.45)
    p.filt(0, type=pick(r, ["LP 24", "LP Ladder"]), cutoff=rr(r, 500, 1200, 0), res=0.15, env=rr(r, 0.35, 0.55, 2), key=0.4)
    p.env(1, att=rr(r, 0.04, 0.12, 3), dec=rr(r, 0.3, 0.7, 2), sus=0.5)
    p.env(0, att=rr(r, 0.01, 0.06, 3), dec=0.6, sus=0.85, rel=0.3)
    p.mod("Velocity", "Filter 1 Env Amt", 0.2)
    chorus(p, r, mix=(0.2, 0.35)); reverb(p, r, "Hall", mix=(0.12, 0.22))
    macros(p)


@arch(BR, "Synth Brass", ["Brass Stab Synth", "Poly Brass", "Power Brass", "Brass Pad", "Detuned Brass"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Saw Stack", "Double Saw"]), voices=5, detune=0.25, level=0.5)
    p.osc(1, wt=pick(r, ["Analog PWM", "Square PWM"]), pos=0.5, voices=3, detune=0.2, level=0.35, oct=-1)
    p.filt(0, type="LP 24", cutoff=rr(r, 800, 1600, 0), res=0.2, env=0.45)
    p.env(1, att=0.05, dec=0.5, sus=0.4)
    p.env(0, att=0.02, dec=0.5, sus=0.8, rel=0.4)
    reverb(p, r, "Hall", mix=(0.15, 0.25))
    macros(p)


@arch(BR, "Trombone", ["Trombone", "Tenor Trombone", "Slide Trombone", "Jazz Trombone", "Mellow Bone"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Trombone", poly=4)
    if i % 2 == 0:
        p.set(voiceMode="Legato", polyphony=1, glide=rr(r, 0.08, 0.2, 3), glideLegato=1); p.add_tags("Slide")
    hall(p, r); macros(p)


@arch(BR, "Saxophone", ["Alto Sax", "Smooth Alto", "Jazz Alto", "Soul Alto", "Bebop Alto"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Alto Sax", poly=2)
    solo_expr(p, r)
    reverb(p, r, pick(r, ["Room", "Hall"]), mix=(0.1, 0.2)); macros(p)


@arch(BR, "Saxophone", ["Tenor Sax", "Late Night Tenor", "Honky Tenor", "Blue Tenor", "Ballad Tenor"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Tenor Sax", poly=2)
    solo_expr(p, r)
    if r.random() < 0.3: p.fx("dist", type="Tube", drive=0.25, mix=0.4)
    reverb(p, r, pick(r, ["Room", "Hall"]), mix=(0.1, 0.2)); macros(p)


@arch(BR, "Saxophone", ["Baritone Sax", "Bari Sax", "Low Sax", "Funk Bari", "Big Bari"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Baritone Sax", poly=2)
    reverb(p, r, "Room", mix=(0.08, 0.16)); macros(p)


@arch(BR, "Horn", ["French Horn", "Concert Horn", "Noble Horn", "Pastoral Horn", "Hunting Horn"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "French Horn", att=(0.05, 0.15))
    hall(p, r, (0.2, 0.35)); macros(p)


@arch(BR, "Trumpet", ["Trumpet", "Classical Trumpet", "Jazz Trumpet", "Bright Trumpet", "Mariachi Trumpet"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Trumpet", poly=3)
    if i % 2 == 0: solo_expr(p, r)
    hall(p, r); macros(p)


@arch(BR, "Trumpet", ["Muted Trumpet", "Harmon Mute", "Cool Trumpet", "Smoky Trumpet", "Noir Trumpet"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Muted Trumpet", poly=3)
    solo_expr(p, r)
    reverb(p, r, "Room", mix=(0.12, 0.22)); macros(p)


@arch(BR, "Tuba", ["Tuba", "Concert Tuba", "Oompah Tuba", "Low Tuba", "March Tuba"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Tuba", poly=3)
    hall(p, r, (0.12, 0.22)); macros(p)


# ------------------------------------------------------------------------------------------- WIND
@arch(WD, "Assorted Winds", ["Pan Flute", "Andes Flute", "Pan Pipes", "Zampona", "Breathy Pipes"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.03, glideLegato=1)
    p.osc(0, wt=pick(r, ["Sine", "Half Sine", "Triangle"]), level=0.7)
    p.noise(type="Breath", level=0.12, route="Filter 1")
    p.env(2, dec=0.15, sus=0.2); p.mod("Env 3", "Noise Level", 0.3)
    p.filt(0, type="BP 12", cutoff=rr(r, 1500, 3000, 0), res=0.1, key=0.7, mix=0.6)
    p.env(0, att=0.04, dec=0.4, sus=0.85, rel=0.3)
    vibrato(p, r, cents=(6, 12))
    reverb(p, r, "Hall", mix=(0.2, 0.35)); macros(p, extra=[("Macro 2", "Noise Level", 0.3)])


@arch(WD, "Assorted Winds", ["Ocarina", "Clay Ocarina", "Zelda Ocarina", "Vessel Flute", "Soft Ocarina"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Ocarina", poly=3)
    vibrato(p, r, cents=(5, 10))
    reverb(p, r, "Hall", mix=(0.15, 0.3)); macros(p)


@arch(WD, "Assorted Winds", ["Bagpipe", "Highland Pipes", "Celtic Pipes", "War Pipes", "Gaita"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Bagpipe", poly=4)
    reverb(p, r, "Hall", mix=(0.15, 0.25)); macros(p)


@arch(WD, "Assorted Winds", ["Didgeridoo", "Outback Drone", "Didge", "Earth Drone", "Dreamtime"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.05, glideLegato=1)
    p.osc(0, wt=pick(r, ["Saw", "Throat", "Growl"]), pos=rr(r, 0.1, 0.4, 2), level=0.7, oct=-1)
    p.noise(type="Breath", level=0.06, route="Filter 1")
    p.filt(0, type="Formant", cutoff=rr(r, 300, 700, 0), res=0.5, mix=0.8)
    p.lfo(0, shape=pick(r, ["Sine", "Smooth Random", "Wobble 2x"]), rate=rr(r, 0.6, 2.5, 2), mode="Free")
    p.mod("LFO 1", "Filter 1 Cutoff", rr(r, 0.12, 0.25, 2)).mod("LFO 1", "Osc A WT Pos", 0.2)
    p.env(0, att=0.08, dec=0.5, sus=1.0, rel=0.4)
    reverb(p, r, "Room", mix=(0.1, 0.2)); macros(p)
    p.add_tags("Synth")


@arch(WD, "Flute", ["Flute", "Concert Flute", "Orchestral Flute", "Silver Flute", "Pastoral Flute"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Flute", poly=4)
    if i % 2 == 0: solo_expr(p, r)
    hall(p, r); macros(p)


@arch(WD, "Flute", ["Piccolo", "Bright Piccolo", "Fife", "March Piccolo"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Piccolo", poly=3)
    hall(p, r); macros(p)


@arch(WD, "Flute", ["Synth Flute", "Breath Flute", "Shakuhachi Style", "Bamboo Flute", "Air Flute"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.04, glideLegato=1)
    p.osc(0, wt=pick(r, ["Sine", "Half Sine"]), level=0.65)
    p.noise(type="Breath", level=0.15, route="Filter 1")
    p.filt(0, type="LP 12", cutoff=rr(r, 2500, 5000, 0), key=0.6)
    p.env(2, att=0.0, dec=0.2, sus=0.3); p.mod("Env 3", "Noise Level", 0.4)
    p.env(0, att=rr(r, 0.05, 0.12, 2), dec=0.4, sus=0.9, rel=0.3)
    vibrato(p, r, cents=(8, 16), delay=(0.4, 0.8))
    reverb(p, r, "Hall", mix=(0.2, 0.35)); macros(p)


@arch(WD, "Recorder", ["Recorder", "Alto Recorder", "Baroque Recorder", "School Recorder", "Consort Recorder"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Recorder", poly=4)
    reverb(p, r, "Room", mix=(0.12, 0.22)); macros(p)


@arch(WD, "Recorder", ["Tin Whistle", "Penny Whistle", "Irish Whistle", "Folk Whistle"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, pick(r, ["Recorder", "Piccolo"]), poly=2, oct=pick(r, [0, 1]))
    p.set(voiceMode="Legato", polyphony=1, glide=0.02, glideLegato=1)
    vibrato(p, r, cents=(5, 12), delay=(0.3, 0.5))
    reverb(p, r, "Room", mix=(0.12, 0.22)); macros(p)


@arch(WD, "Clarinet", ["Clarinet", "Concert Clarinet", "Jazz Clarinet", "Klezmer Clarinet", "Woody Clarinet"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Clarinet", poly=4)
    if i % 2 == 0: solo_expr(p, r)
    hall(p, r); macros(p)


@arch(WD, "Clarinet", ["Synth Clarinet", "Square Reed", "Hollow Reed", "Chalumeau"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.03, glideLegato=1)
    p.osc(0, wt=pick(r, ["Square", "Odd Harmonics", "Reed"]), pos=0.3, level=0.6)
    p.filt(0, type="LP 24", cutoff=rr(r, 1500, 3000, 0), res=0.15, key=0.6)
    p.noise(type="Breath", level=0.04, route="Filter 1")
    p.env(0, att=0.05, dec=0.4, sus=0.9, rel=0.2)
    vibrato(p, r, cents=(4, 10))
    reverb(p, r, "Room", mix=(0.12, 0.22)); macros(p)


@arch(WD, "Oboe", ["Oboe", "Orchestral Oboe", "Pastoral Oboe", "Reed Oboe", "Lyric Oboe"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Oboe", poly=4)
    if i % 2 == 0: solo_expr(p, r)
    hall(p, r); macros(p)


@arch(WD, "Oboe", ["Synth Oboe", "Nasal Reed", "Shawm", "Krumhorn Style"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.03, glideLegato=1)
    p.osc(0, wt=pick(r, ["Square PWM", "Reed", "Analog PWM"]), pos=rr(r, 0.75, 0.95, 2), level=0.6)
    p.filt(0, type="BP 12", cutoff=rr(r, 1200, 2500, 0), res=0.3, key=0.5, mix=0.7)
    p.env(0, att=0.04, dec=0.4, sus=0.9, rel=0.2)
    vibrato(p, r, cents=(5, 12))
    reverb(p, r, "Room", mix=(0.12, 0.22)); macros(p)


@arch(WD, "Bassoon", ["Bassoon", "Orchestral Bassoon", "Comic Bassoon", "Dark Bassoon", "Low Reed"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Bassoon", poly=4)
    hall(p, r); macros(p)


@arch(WD, "Accordion", ["Accordion", "Musette", "Paris Accordion", "Button Accordion", "Tango Bandoneon", "Folk Accordion"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Accordion", poly=12)
    if i % 2 == 0:
        p.fx("cho", depth=rr(r, 0.3, 0.5, 2), rate=rr(r, 4, 7, 2), delay=6, mix=0.35); p.add_tags("Musette")
    reverb(p, r, "Room", mix=(0.08, 0.16)); macros(p)


@arch(WD, "Accordion", ["Synth Accordion", "Squeezebox", "Reed Box", "Bal Musette"], role="keys")
def _(p, r, i):
    p.osc(0, wt="Reed", pos=rr(r, 0.3, 0.7, 2), level=0.5)
    p.osc(1, wt="Reed", pos=0.5, fine=rr(r, 10, 18, 1), level=0.4)
    p.osc(2, wt="Reed", pos=0.5, fine=-rr(r, 10, 18, 1), level=0.3)
    p.filt(0, type="LP 12", cutoff=rr(r, 2500, 5000, 0))
    p.env(0, att=0.05, dec=0.5, sus=1.0, rel=0.15)
    reverb(p, r, "Room", mix=(0.1, 0.18)); macros(p, timbre="Osc A WT Pos")


@arch(WD, "Harmonica", ["Harmonica", "Blues Harp", "Country Harmonica", "Folk Harp", "Mouth Harp"], role="real", chars=REAL)
def _(p, r, i):
    blow(p, r, "Harmonica", poly=6)
    if i % 2 == 0:
        p.fx("dist", type=pick(r, ["Tube", "Overdrive"]), drive=rr(r, 0.25, 0.5, 2), mix=0.6)
        p.fx("eq", lowf=300, lowg=-6, midf=1500, midg=4, highf=5000, highg=-6); p.add_tags("Bullet Mic")
    reverb(p, r, "Room", mix=(0.1, 0.2)); macros(p)


@arch(WD, "Harmonica", ["Synth Harmonica", "Reed Lead", "Harmonica Lead", "Train Harp"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.03, glideLegato=1)
    p.osc(0, wt="Reed", pos=rr(r, 0.4, 0.8, 2), level=0.65)
    p.filt(0, type="BP 12", cutoff=rr(r, 1200, 2200, 0), res=0.2, mix=0.7)
    p.env(0, att=0.03, dec=0.4, sus=0.9, rel=0.15)
    vibrato(p, r, rate=(5, 7), cents=(15, 30), delay=(0.2, 0.4))
    reverb(p, r, "Room", mix=(0.1, 0.18)); macros(p)


# ------------------------------------------------------------------------------------------- VOX
def voice(p, r, poly=8, att=(0.08, 0.3), oct=0):
    p.set(polyphony=poly)
    p.sample("Voice Aah", level=0.85, oct=oct)
    p.filt(0, type="LP 12", cutoff=rr(r, 4000, 10000, 0))
    p.env(0, att=rr(r, *att, 2), dec=1.0, sus=1.0, rel=rr(r, 0.4, 1.0, 2))
    p.mod("Mod Wheel", "Filter 1 Cutoff", 0.2)


@arch(VX, "Solo Vox", ["Solo Voice", "Aah Voice", "Siren Voice", "Ethereal Voice", "Diva Aah"], role="real", chars=REAL)
def _(p, r, i):
    voice(p, r, poly=4)
    if i % 2 == 0: solo_expr(p, r)
    reverb(p, r, "Hall", mix=(0.2, 0.35)); macros(p)


@arch(VX, "Solo Vox", ["Synth Voice", "Vocoder Lead", "Robot Voice", "Talkbox Voice", "Formant Voice"], role="lead")
def _(p, r, i):
    p.set(voiceMode="Legato", polyphony=1, glide=0.05, glideLegato=1)
    p.osc(0, wt=pick(r, ["Vowels", "Robot Voice", "Talking", "Vowel A-E", "Throat"]), pos=rr(r, 0.1, 0.6, 2), level=0.7, voices=2, detune=0.06)
    p.filt(0, type="Formant", cutoff=rr(r, 700, 2000, 0), res=0.4, mix=0.7)
    p.lfo(0, shape="Smooth Random", rate=rr(r, 0.5, 2.0, 2), mode="Free")
    p.mod("LFO 1", "Osc A WT Pos", 0.3)
    p.env(0, att=0.03, dec=0.5, sus=0.85, rel=0.3)
    reverb(p, r, "Hall", mix=(0.15, 0.25)); macros(p, timbre="Osc A WT Pos")


@arch(VX, "Ensemble Choir", ["Choir Aahs", "Chamber Choir", "Voices", "Angel Choir", "Gospel Choir", "Soft Choir"], role="real", chars=REAL)
def _(p, r, i):
    voice(p, r, poly=16, att=(0.15, 0.5))
    p.osc(0, wt="Choir", pos=rr(r, 0.2, 0.6, 2), voices=6, detune=0.25, level=0.25)
    p.fx("cho", depth=0.6, rate=0.35, delay=15, mix=0.45)
    reverb(p, r, "Hall", mix=(0.25, 0.4)); macros(p, extra=[("Macro 2", "Osc A Level", 0.3)])
    p.add_tags("Emulated")


@arch(VX, "Ensemble Choir", ["Synth Choir", "Choir Pad", "Mellotron Choir", "Digital Choir", "Vox Humana"], role="pad")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Choir", "Vowels", "Vowel I-O-U", "Whisper"]), pos=rr(r, 0.2, 0.7, 2), voices=6, detune=0.25, level=0.55)
    p.filt(0, type="Formant", cutoff=rr(r, 800, 1600, 0), res=0.3, mix=0.5)
    p.env(0, att=rr(r, 0.2, 0.6, 2), dec=1.0, sus=0.9, rel=1.0)
    p.fx("cho", depth=0.5, rate=0.4, mix=0.4)
    reverb(p, r, "Hall", mix=(0.25, 0.4)); macros(p, timbre="Osc A WT Pos")


@arch(VX, "Orchestral Choir", ["Epic Choir", "Requiem Choir", "Cathedral Choir", "Battle Choir", "Doom Choir"], role="real", chars=REAL)
def _(p, r, i):
    voice(p, r, poly=16, att=(0.3, 0.8), oct=pick(r, [0, -1]))
    p.osc(0, wt="Choir", pos=0.4, voices=8, detune=0.3, level=0.3, oct=-1)
    p.sub(shape="Sine", oct=-1, level=0.15)
    reverb(p, r, "Hall", mix=(0.35, 0.5), size=(0.9, 1.0), decay=(5, 10))
    macros(p, extra=[("Macro 2", "Osc A Level", 0.3)])
    p.add_tags("Emulated")


@arch(VX, "Orchestral Choir", ["Choir & Strings", "Choir & Organ", "Sacred Choir", "Hymn Choir", "Oratorio"], role="real", chars=REAL)
def _(p, r, i):
    voice(p, r, poly=16, att=(0.2, 0.6))
    p.osc(0, wt=pick(r, ["Strings", "Organ", "Octaves"]), pos=0.5, voices=4, detune=0.2, level=0.25)
    reverb(p, r, "Hall", mix=(0.3, 0.45), size=(0.85, 1.0))
    macros(p, extra=[("Macro 2", "Osc A Level", 0.3)])


@arch(VX, "Scat", ["Doo Scat", "Bah Scat", "Dah Vox", "Jazz Scat", "Bop Vox"], role="real", chars=REAL)
def _(p, r, i):
    voice(p, r, poly=6, att=(0.0, 0.02))
    p.env(0, att=0.005, dec=rr(r, 0.18, 0.4, 2), sus=rr(r, 0.3, 0.6, 2), rel=0.12)
    p.filt(0, type="Formant", cutoff=rr(r, 500, 1200, 0), res=0.4, env=0.3, mix=0.55)
    p.env(1, dec=0.1, sus=0.0)
    if i % 2 == 1:
        p.arpeggio(mode=pick(r, ["Sequencer", "Up-Down", "Random"]), rate=pick(r, ["1/8", "1/16", "1/8 T"]), gate=0.5, scale=pick(r, ["Minor Pentatonic", "Blues", "Dorian"]),
                   pattern=[pick(r, [0, 2, 4, 7, -3, 5]) for _ in range(8)], steps=8, swing=rr(r, 0.2, 0.5, 2))
    reverb(p, r, "Room", mix=(0.1, 0.2)); macros(p)
    p.add_tags("Emulated")


@arch(VX, "Scat", ["Vowel Chop", "Vocal Chops", "Chopped Vox", "Stutter Vox", "Glitch Vox"], role="fx")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Vowels", "Vowel A-E", "Talking", "Choir"]), pos=0.0, level=0.7)
    p.env(0, att=0.0, dec=rr(r, 0.1, 0.25, 2), sus=0.0, rel=0.1)
    p.lfo(0, shape=pick(r, ["Stairs 8", "S&H", "Stairs Up-Down"]), div=pick(r, ["1/8", "1/16"]), mode="Retrig")
    p.mod("LFO 1", "Osc A WT Pos", 0.8)
    p.filt(0, type="Formant", cutoff=1200, res=0.4, mix=0.6)
    p.arpeggio(mode=pick(r, ["Up", "Random", "Up-Down"]), rate=pick(r, ["1/16", "1/8"]), oct=2, gate=0.4, scale="Minor Pentatonic")
    reverb(p, r, "Hall", mix=(0.15, 0.25)); delay(p, r)
    macros(p, timbre="Osc A WT Pos")
