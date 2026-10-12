"""KEYBOARD, ORGANS, BELLS."""
from core import *
from registry import arch

K, O, BL = "Keyboard", "Organs", "Bells"


def piano(p, r, inst, bright=(8000, 18000), rel=(0.3, 0.6), vel=(0.7, 0.95)):
    p.set(polyphony=32)
    p.sample(inst, level=0.9, vel=rr(r, *vel, 2))
    p.filt(0, type="LP 12", cutoff=rr(r, *bright, 0), res=0.05)
    velocity_tone(p, 0.18)
    p.env(0, att=0.0, dec=2.0, sus=1.0, rel=rr(r, *rel, 2))


REAL = ["clean", "clean", "bright", "dark", "vintage", "wide", "lofi", "fx"]

# ------------------------------------------------------------------------------------------- Acoustic Piano
@arch(K, "Acoustic Piano", ["Upright Piano", "Home Piano", "Studio Upright", "Parlor Piano", "Songwriter Piano"], role="real", chars=REAL)
def _(p, r, i):
    piano(p, r, "Upright Piano")
    reverb(p, r, "Room", mix=(0.08, 0.16), size=(0.25, 0.5), decay=(0.8, 1.6))
    macros(p)


@arch(K, "Acoustic Piano", ["Vintage Piano", "Saloon Piano", "Old Piano", "Bar Piano", "Antique Piano"], role="real", chars=REAL)
def _(p, r, i):
    piano(p, r, "Vintage Piano", (5000, 12000))
    reverb(p, r, "Room", mix=(0.1, 0.18))
    macros(p)


@arch(K, "Acoustic Piano", ["Honky Tonk", "Detuned Piano", "Western Piano", "Ragtime Piano"], role="real", chars=["vintage", "lofi", "clean", "bright"])
def _(p, r, i):
    piano(p, r, pick(r, ["Upright Piano", "Vintage Piano"]))
    p.fx("cho", depth=rr(r, 0.4, 0.7, 2), rate=rr(r, 0.2, 0.6, 2), delay=rr(r, 5, 10, 1), mix=0.5)
    reverb(p, r, "Room", mix=(0.08, 0.15))
    macros(p)


@arch(K, "Acoustic Piano", ["Felt Piano", "Muted Piano", "Soft Piano", "Night Piano", "Intimate Piano"], role="real", chars=["clean", "dark", "lofi", "vintage", "wide"])
def _(p, r, i):
    piano(p, r, pick(r, ["Upright Piano", "Grand Piano", "Concert Grand"]), (900, 2500), vel=(0.4, 0.7))
    p.set(f1_type=choice_index("f1_type", "LP 24"))
    p.fx("tape", wow=0.1, flutter=0.08, sat=0.25, tone=0.5, mix=1.0)
    reverb(p, r, "Hall", mix=(0.15, 0.25))
    macros(p)


@arch(K, "Acoustic Piano", ["Piano Pad", "Piano Strings", "Layered Piano", "Dream Piano", "Ballad Layer"], role="real", chars=["clean", "wide", "bright", "dark", "fx"])
def _(p, r, i):
    piano(p, r, pick(r, ["Upright Piano", "Grand Piano", "Concert Grand", "Vintage Piano"]))
    p.osc(0, wt=pick(r, ["Strings", "Soft Saw", "Choir", "Glass"]), voices=4, detune=0.2, level=rr(r, 0.15, 0.3, 2), route="Filter 2")
    p.filt(1, type="LP 12", cutoff=rr(r, 1500, 4000, 0))
    p.env(0, att=0.0, dec=3.0, sus=0.8, rel=1.0)
    reverb(p, r, "Hall", mix=(0.2, 0.32))
    macros(p, extra=[("Macro 2", "Osc A Level", 0.3)])


# ------------------------------------------------------------------------------------------- Grand Piano
@arch(K, "Grand Piano", ["Concert Grand", "Steinway Style", "Recital Grand", "Hall Grand", "Classical Grand"], role="real", chars=REAL)
def _(p, r, i):
    piano(p, r, "Concert Grand")
    reverb(p, r, "Hall", mix=(0.12, 0.22), size=(0.6, 0.9), decay=(2, 3.5))
    macros(p)


@arch(K, "Grand Piano", ["Grand Piano", "Studio Grand", "Salamander Grand", "Jazz Grand", "Session Grand"], role="real", chars=REAL)
def _(p, r, i):
    piano(p, r, "Grand Piano")
    reverb(p, r, pick(r, ["Room", "Hall"]), mix=(0.08, 0.18))
    if r.random() < 0.4: p.fx("comp", thresh=-20, ratio=2.5, makeup=2)
    macros(p)


@arch(K, "Grand Piano", ["Pop Grand", "Bright Grand", "Rock Grand", "Stage Grand", "Radio Grand"], role="real", chars=["bright", "clean", "digital", "wide"])
def _(p, r, i):
    piano(p, r, pick(r, ["Grand Piano", "Concert Grand"]), (12000, 18000))
    p.fx("eq", lowf=120, lowg=-2, midf=400, midg=-2, highf=6000, highg=rr(r, 2, 4, 1))
    p.fx("comp", thresh=-22, ratio=3, makeup=3)
    reverb(p, r, "Room", mix=(0.08, 0.15))
    macros(p)


@arch(K, "Grand Piano", ["Cinematic Grand", "Ambient Grand", "Score Grand", "Echo Grand", "Cathedral Grand"], role="real", chars=["wide", "dark", "fx", "clean", "bright"])
def _(p, r, i):
    piano(p, r, pick(r, ["Concert Grand", "Grand Piano"]))
    reverb(p, r, pick(r, ["Hall", "Shimmer"]), mix=(0.3, 0.45), decay=(5, 10), size=(0.8, 1.0))
    delay(p, r, div="1/4 D", mix=(0.08, 0.15))
    macros(p)


# ------------------------------------------------------------------------------------------- Electric Piano
@arch(K, "Electric Piano", ["Wurli", "Wurlitzer", "Soul Wurli", "Wurli Ballad", "Bark Wurli"], role="real", chars=REAL)
def _(p, r, i):
    piano(p, r, "Wurlitzer", (5000, 12000))
    if r.random() < 0.5:
        p.lfo(0, shape="Sine", rate=rr(r, 4.5, 6.5, 2), mode="Free"); p.mod("LFO 1", "Amp", rr(r, 0.1, 0.25, 2))
    if r.random() < 0.4: p.fx("dist", type="Tube", drive=rr(r, 0.15, 0.4, 2), mix=0.5)
    reverb(p, r, "Room", mix=(0.08, 0.15))
    macros(p, extra=[("Macro 2", "Amp", 0.0)])


@arch(K, "Electric Piano", ["Pianet", "Pianet Funk", "Reed Piano", "Pianet Soul", "Pluck EP"], role="real", chars=REAL)
def _(p, r, i):
    piano(p, r, "Pianet", (4000, 10000))
    if r.random() < 0.6: p.fx("pha", rate=rr(r, 0.2, 0.6, 2), depth=0.5, mix=0.35)
    reverb(p, r, "Room", mix=(0.06, 0.14))
    macros(p)


@arch(K, "Electric Piano", ["CP80", "Electric Grand", "Yamaha EG", "80s Electric Grand", "Ballad CP"], role="real", chars=REAL)
def _(p, r, i):
    piano(p, r, "Electric Grand")
    chorus(p, r, mix=(0.2, 0.4))
    reverb(p, r, "Hall", mix=(0.1, 0.2))
    macros(p)


@arch(K, "Electric Piano", ["FM E-Piano", "DX Piano", "80s EP", "TX Piano", "Glass EP"], role="real", chars=REAL)
def _(p, r, i):
    piano(p, r, "FM E-Piano")
    chorus(p, r, mix=(0.25, 0.45))
    reverb(p, r, "Hall", mix=(0.1, 0.2))
    macros(p)


@arch(K, "Electric Piano", ["Tine EP", "Suitcase EP", "Rhodes Style", "Stage EP", "Dyno EP"], role="keys")
def _(p, r, i):
    p.osc(0, wt="FM E-Piano", pos=rr(r, 0.1, 0.4, 2), level=0.7)
    p.osc(1, wt="Sine", level=0.4)
    p.osc(2, wt="FM Bell", pos=0.2, oct=2, level=0.0)
    p.env(2, att=0.0, dec=rr(r, 0.15, 0.4, 2), sus=0.0)
    p.mod("Env 3", "Osc C Level", rr(r, 0.12, 0.25, 2)).mod("Velocity", "Osc A WT Pos", 0.3).mod("Velocity", "Osc C Level", 0.15)
    p.env(0, att=0.0, dec=rr(r, 2.0, 4.0, 2), sus=0.0, rel=0.5)
    p.lfo(0, shape="Sine", rate=rr(r, 3.5, 5.5, 2), mode="Free")
    p.mod("LFO 1", "Osc A Pan", rr(r, 0.3, 0.7, 2)).mod("LFO 1", "Osc B Pan", -0.5)
    p.filt(0, type="LP 12", cutoff=rr(r, 3000, 8000, 0))
    reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p, timbre="Osc A WT Pos")
    p.add_tags("Synth")


# ------------------------------------------------------------------------------------------- Electric Organ
def leslie(p, r):
    p.fx("cho", depth=rr(r, 0.5, 0.8, 2), rate=rr(r, 4.5, 6.5, 2), delay=8, mix=rr(r, 0.35, 0.55, 2))
    p.lfo(3, shape="Sine", rate=rr(r, 5.5, 6.8, 2), mode="Free")
    p.mod("LFO 4", "Amp", 0.08)


@arch(O, "Electric Organ", ["Tonewheel", "B3 Organ", "Gospel Organ", "Jazz Organ", "Drawbar Organ", "Soul Organ"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Organ", "Octaves & Fifths", "Octaves"]), pos=rr(r, 0.1, 0.9, 2), level=0.6)
    p.osc(1, wt="Sine", oct=1, semi=7, level=rr(r, 0.0, 0.3, 2))
    p.osc(2, wt="Sine", oct=pick(r, [1, 2]), level=0.0)
    p.env(2, att=0.0, dec=rr(r, 0.15, 0.35, 2), sus=0.0)
    p.mod("Env 3", "Osc C Level", rr(r, 0.2, 0.4, 2))      # percussion
    p.noise(type="White", level=0.08, route="Direct")
    p.env(3, att=0.0, dec=0.012, sus=0.0)
    p.mod("Env 4", "Noise Level", 0.3)                      # key click
    p.set(noise_level=0.0)
    p.filt(0, on=0)
    p.env(0, att=0.002, dec=0.3, sus=1.0, rel=0.06)
    leslie(p, r)
    if r.random() < 0.5: p.fx("dist", type="Tube", drive=rr(r, 0.15, 0.4, 2), mix=0.6)
    reverb(p, r, "Room", mix=(0.06, 0.14))
    macros(p, tone=False, timbre="Osc A WT Pos", extra=[("Macro 3", "Osc B Level", 0.4)])


@arch(O, "Electric Organ", ["Combo Organ", "Farfisa Style", "Garage Organ", "Vox Organ", "60s Organ"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Odd Harmonics", "Square", "Warm Square", "Analog PWM"]), pos=rr(r, 0.2, 0.7, 2), level=0.55)
    p.osc(1, wt="Square", oct=1, level=0.25)
    p.filt(0, type=pick(r, ["LP 12", "BP 12"]), cutoff=rr(r, 2000, 5000, 0), res=0.15)
    p.env(0, att=0.002, dec=0.3, sus=1.0, rel=0.05)
    vibrato(p, r, rate=(6, 7), cents=(10, 20), delay=(0.0, 0.1))
    if r.random() < 0.5: p.fx("dist", type="Overdrive", drive=0.3, mix=0.5)
    reverb(p, r, "Room", mix=(0.08, 0.16))
    macros(p)


@arch(O, "Electric Organ", ["Rock Organ", "Overdrive Organ", "Purple Organ", "Screaming Organ", "Prog Organ"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Organ", "Octaves & Fifths"]), pos=rr(r, 0.5, 1.0, 2), level=0.6)
    p.filt(0, on=0)
    p.env(0, att=0.002, dec=0.3, sus=1.0, rel=0.06)
    leslie(p, r)
    p.fx("dist", type=pick(r, ["Tube", "Overdrive", "Diode"]), drive=rr(r, 0.4, 0.7, 2), tone=0.55, mix=0.9)
    reverb(p, r, "Room", mix=(0.08, 0.15))
    macros(p, tone=False, timbre="Osc A WT Pos")


@arch(O, "Electric Organ", ["House Organ", "Organ Stab", "M1 Organ", "Garage Stab", "Club Organ"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Octaves", "Organ", "Even Harmonics"]), pos=rr(r, 0.3, 0.8, 2), level=0.6)
    p.filt(0, type="LP 24", cutoff=rr(r, 1500, 4000, 0), res=0.15, env=0.25)
    p.env(1, dec=0.3, sus=0.2)
    p.env(0, att=0.0, dec=rr(r, 0.3, 0.7, 2), sus=0.2, rel=0.15)
    p.fx("comp", thresh=-18, ratio=4, makeup=3)
    reverb(p, r, "Room", mix=(0.1, 0.18))
    macros(p)


# ------------------------------------------------------------------------------------------- Pipe / Reed / Church
@arch(O, "Pipe Organ", ["Flute Pipes", "Chamber Organ", "Positive Organ", "Soft Pipes", "Chapel Pipes"], role="real", chars=REAL)
def _(p, r, i):
    p.set(polyphony=24)
    p.sample(pick(r, ["Soft Pipe Organ", "Renaissance Organ"]), level=0.85)
    p.env(0, att=0.02, dec=1.0, sus=1.0, rel=rr(r, 0.3, 0.6, 2))
    reverb(p, r, "Hall", mix=(0.2, 0.35), size=(0.7, 1.0), decay=(3, 6))
    macros(p)


@arch(O, "Pipe Organ", ["Synth Pipe Organ", "Pipe Drone", "Pipe Ensemble", "Bourdon", "Principal"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Octaves", "Organ", "Octaves & Fifths"]), pos=rr(r, 0.2, 0.9, 2), level=0.55)
    p.osc(1, wt="Sine", oct=-1, level=0.35)
    p.noise(type="Breath", level=0.03, route="Filter 1")
    p.filt(0, type="LP 12", cutoff=rr(r, 3000, 7000, 0))
    p.env(0, att=0.06, dec=1.0, sus=1.0, rel=0.5)
    reverb(p, r, "Hall", mix=(0.25, 0.4), decay=(4, 8), size=(0.8, 1.0))
    macros(p, timbre="Osc A WT Pos")


@arch(O, "Reed Organ", ["Harmonium", "Pump Organ", "Parlour Reed", "Indian Harmonium", "Mission Organ"], role="real", chars=REAL)
def _(p, r, i):
    p.set(polyphony=16)
    p.sample(pick(r, ["Accordion", "Harmonica"]), level=0.8, oct=pick(r, [0, -1]))
    p.env(0, att=rr(r, 0.06, 0.15, 2), dec=1.0, sus=1.0, rel=0.25)
    p.fx("cho", depth=0.3, rate=0.5, mix=0.3)
    reverb(p, r, "Room", mix=(0.1, 0.2))
    macros(p)


@arch(O, "Reed Organ", ["Synth Harmonium", "Reed Drone", "Melodeon", "Reed Pad Organ", "Bellows"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Reed", "Odd Harmonics", "Square PWM"]), pos=rr(r, 0.2, 0.7, 2), level=0.5)
    p.osc(1, wt="Reed", pos=0.4, fine=rr(r, 8, 14, 1), level=0.4)
    p.filt(0, type="LP 12", cutoff=rr(r, 2000, 5000, 0), res=0.1)
    p.env(0, att=0.08, dec=1.0, sus=1.0, rel=0.25)
    p.lfo(0, shape="Sine", rate=rr(r, 0.2, 0.6, 2), mode="Free"); p.mod("LFO 1", "Amp", 0.06)
    reverb(p, r, "Room", mix=(0.12, 0.2))
    macros(p, timbre="Osc A WT Pos")


@arch(O, "Church Organ", ["Church Organ", "Cathedral Organ", "Full Organ", "Plenum", "Grand Organ"], role="real", chars=["clean", "dark", "wide", "bright", "clean", "fx", "vintage", "dark"])
def _(p, r, i):
    p.set(polyphony=24)
    p.sample("Church Organ", level=0.85)
    if r.random() < 0.5: p.osc(0, wt="Octaves", pos=0.8, oct=-1, level=0.25)
    p.env(0, att=0.03, dec=1.0, sus=1.0, rel=rr(r, 0.5, 1.0, 2))
    reverb(p, r, "Hall", mix=(0.3, 0.45), size=(0.85, 1.0), decay=(5, 10), pre=(20, 50))
    macros(p)


@arch(O, "Church Organ", ["Requiem Organ", "Toccata", "Gothic Organ", "Mass Organ", "Abbey Organ"], role="keys")
def _(p, r, i):
    p.sample(pick(r, ["Church Organ", "Soft Pipe Organ"]), level=0.75, osc_off=False)
    p.osc(0, wt="Organ", pos=1.0, level=0.3)
    p.osc(1, wt="Octaves & Fifths", oct=-1, level=0.3)
    p.env(0, att=0.04, dec=1.0, sus=1.0, rel=1.0)
    reverb(p, r, "Hall", mix=(0.35, 0.5), size=(0.9, 1.0), decay=(7, 12))
    macros(p)


# ------------------------------------------------------------------------------------------- BELLS
def mallet_sample(p, r, inst, oct=0, rev=("Hall", (0.12, 0.25))):
    p.set(polyphony=24)
    p.sample(inst, level=0.9, oct=oct, vel=rr(r, 0.6, 0.9, 2))
    p.env(0, att=0.0, dec=2.0, sus=1.0, rel=rr(r, 0.3, 1.0, 2))
    velocity_tone(p, 0.12)
    reverb(p, r, rev[0], mix=rev[1])


@arch(BL, "Bells", ["FM Bells", "Digital Bells", "Crystal Bells", "Glass Bells", "Ice Bells", "Fairy Bells"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM Bell", "FM Chorus Bell", "FM 1:9 Glass", "Glass", "Prime Harmonics"]), pos=rr(r, 0.2, 0.8, 2), level=0.7)
    p.osc(1, wt="Sine", oct=pick(r, [1, 2]), level=rr(r, 0.1, 0.3, 2), semi=pick(r, [0, 7, 0]))
    p.env(0, att=0.0, dec=rr(r, 1.5, 4.0, 2), sus=0.0, rel=rr(r, 1.0, 2.5, 2))
    p.mod("Velocity", "Osc A WT Pos", 0.2)
    reverb(p, r, pick(r, ["Hall", "Shimmer"]), mix=(0.2, 0.35)); delay(p, r, mode="Ping-Pong", mix=(0.08, 0.16))
    macros(p, timbre="Osc A WT Pos")


@arch(BL, "Bells", ["Glockenspiel", "Orchestral Glock", "Glock", "Music Box", "Toy Bells"], role="real", chars=["clean", "bright", "wide", "lofi", "fx", "vintage"])
def _(p, r, i):
    mallet_sample(p, r, "Glockenspiel", oct=1)   # orchestral habit: the glock sounds above the written note
    macros(p)


@arch(BL, "Bells", ["Hand Bells", "Handbell Choir", "Chapel Bells", "Bell Choir", "Christmas Bells"], role="real", chars=["clean", "bright", "wide", "dark", "fx"])
def _(p, r, i):
    mallet_sample(p, r, "Hand Chimes")
    macros(p)


@arch(BL, "Tubular Bells", ["Tubular Bells", "Orchestral Chimes", "Concert Chimes", "Tubular", "Bell Tower"], role="real", chars=["clean", "wide", "dark", "bright", "fx"])
def _(p, r, i):
    mallet_sample(p, r, "Tubular Bells", rev=("Hall", (0.2, 0.35)))
    macros(p)


@arch(BL, "Tubular Bells", ["Synth Tubular", "FM Tubes", "Metal Tubes", "Tube Bells"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM Bell", "FM Metal", "FM 3:1"]), pos=rr(r, 0.3, 0.7, 2), level=0.7)
    p.osc(1, wt="Sine", semi=pick(r, [0, 12, 19]), level=0.2)
    p.env(0, att=0.0, dec=rr(r, 3.0, 6.0, 2), sus=0.0, rel=2.0)
    reverb(p, r, "Hall", mix=(0.25, 0.4))
    macros(p, timbre="Osc A WT Pos")


@arch(BL, "Church Bells", ["Church Bells", "Cathedral Bells", "Bell Tower", "Village Bells", "Belfry"], role="real", chars=["clean", "dark", "wide", "fx", "lofi"])
def _(p, r, i):
    p.set(polyphony=16)
    p.sample("Tubular Bells", level=0.85, oct=pick(r, [-1, -2]))
    p.osc(0, wt="FM Bell", pos=rr(r, 0.4, 0.8, 2), oct=-1, level=rr(r, 0.15, 0.3, 2))
    p.env(0, att=0.0, dec=6.0, sus=0.0, rel=4.0)
    reverb(p, r, "Hall", mix=(0.35, 0.5), size=(0.9, 1.0), decay=(6, 12), pre=(30, 80))
    macros(p)
    p.add_tags("Emulated")


@arch(BL, "Church Bells", ["Funeral Bell", "Doom Bell", "Great Bell", "Iron Bell", "Monastery Bell"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM Bell", "FM Metal", "Ring Mod"]), pos=rr(r, 0.4, 0.9, 2), oct=-1, level=0.6)
    p.osc(1, wt="Sine", oct=-2, level=0.4)
    p.osc(2, wt="Sine", semi=pick(r, [3, 4, 7]), oct=0, level=0.15)
    p.env(0, att=0.0, dec=8.0, sus=0.0, rel=5.0)
    reverb(p, r, "Hall", mix=(0.35, 0.55), size=(0.9, 1.0), decay=(8, 14))
    macros(p, timbre="Osc A WT Pos")


@arch(BL, "Celesta", ["Celesta", "Celeste", "Sugar Plum", "Fairy Celesta", "Nutcracker Celesta"], role="real", chars=["clean", "bright", "wide", "dark", "fx", "lofi"])
def _(p, r, i):
    mallet_sample(p, r, "Glockenspiel")
    p.osc(0, wt="Sine", level=0.25)
    p.filt(0, type="LP 12", cutoff=rr(r, 4000, 8000, 0))
    p.env(0, att=0.0, dec=1.5, sus=0.0, rel=0.8)
    macros(p)
    p.add_tags("Emulated")


@arch(BL, "Celesta", ["Synth Celesta", "Toy Celesta", "Felt Celesta", "Dreamy Celesta"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM 1:9 Glass", "FM Bell", "Glass"]), pos=0.3, level=0.5)
    p.osc(1, wt="Sine", oct=1, level=0.5)
    p.env(0, att=0.0, dec=rr(r, 0.8, 1.6, 2), sus=0.0, rel=0.6)
    p.filt(0, type="LP 12", cutoff=6000)
    reverb(p, r, "Hall", mix=(0.15, 0.3))
    macros(p)


@arch(BL, "Xylophone", ["Xylophone", "Concert Xylo", "Xylo", "Bright Xylophone", "Hard Mallet Xylo"], role="real", chars=["clean", "bright", "wide", "fx", "vintage"])
def _(p, r, i):
    mallet_sample(p, r, "Xylophone", oct=1, rev=("Room", (0.08, 0.16)))   # sounds an octave above the written note
    macros(p)


@arch(BL, "Xylophone", ["Balafon", "African Xylophone", "Wood Balafon", "Gourd Xylophone"], role="real", chars=["clean", "dark", "wide", "fx"])
def _(p, r, i):
    mallet_sample(p, r, "Balafon", rev=("Room", (0.08, 0.16)))
    macros(p)


@arch(BL, "Xylophone", ["Synth Xylo", "Plastic Xylo", "Bit Xylo", "Wooden Synth"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM 3:1", "Half Sine", "Odd Harmonics"]), pos=0.4, level=0.7)
    p.noise(type="White", level=0.2, route="Filter 1")
    p.env(2, dec=0.01, sus=0.0); p.mod("Env 3", "Noise Level", 0.5); p.set(noise_level=0.0)
    p.filt(0, type="BP 12", cutoff=3000, res=0.2, key=1.0)
    p.env(0, att=0.0, dec=rr(r, 0.2, 0.4, 2), sus=0.0, rel=0.2)
    reverb(p, r, "Room", mix=(0.08, 0.15))
    macros(p)


@arch(BL, "Marimba", ["Marimba", "Concert Marimba", "Rosewood Marimba", "Soft Marimba", "Bass Marimba"], role="real", chars=["clean", "dark", "wide", "bright", "fx", "lofi", "vintage"])
def _(p, r, i):
    mallet_sample(p, r, "Marimba", oct=pick(r, [0, 0, -1]), rev=("Room", (0.08, 0.18)))
    macros(p)


@arch(BL, "Marimba", ["Synth Marimba", "FM Marimba", "Kalimba Marimba", "Tropical Marimba"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Sine", "Half Sine"]), level=0.7)
    p.osc(1, wt="Sine", oct=2, semi=pick(r, [0, 7]), level=0.0)
    p.env(2, dec=0.06, sus=0.0); p.mod("Env 3", "Osc B Level", 0.4)
    p.env(0, att=0.0, dec=rr(r, 0.3, 0.7, 2), sus=0.0, rel=0.3)
    reverb(p, r, "Room", mix=(0.1, 0.18)); delay(p, r, mix=(0.06, 0.12))
    macros(p)


@arch(BL, "Steel Pan", ["Steel Pan", "Steel Drum", "Caribbean Pan", "Tenor Pan", "Island Pan"], role="real", chars=["clean", "bright", "wide", "fx", "vintage", "lofi"])
def _(p, r, i):
    mallet_sample(p, r, "Steel Pan", rev=("Room", (0.1, 0.18)))
    macros(p)


@arch(BL, "Steel Pan", ["Synth Steel Pan", "FM Pan", "Calypso Synth", "Ring Pan"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM 2:1", "Ring Mod", "FM 3:1"]), pos=rr(r, 0.3, 0.6, 2), level=0.65)
    p.osc(1, wt="Sine", oct=1, level=0.25)
    p.env(0, att=0.0, dec=rr(r, 0.6, 1.2, 2), sus=0.0, rel=0.5)
    p.mod("Velocity", "Osc A WT Pos", 0.2)
    reverb(p, r, "Room", mix=(0.1, 0.18))
    macros(p, timbre="Osc A WT Pos")


@arch(BL, "Mallet", ["Vibraphone", "Vibes", "Jazz Vibes", "Motor Vibes", "Cool Vibes"], role="real", chars=["clean", "wide", "dark", "vintage", "fx", "bright"])
def _(p, r, i):
    mallet_sample(p, r, "Vibraphone")
    if r.random() < 0.6:
        p.lfo(0, shape="Sine", rate=rr(r, 3.0, 6.0, 2), mode="Free"); p.mod("LFO 1", "Amp", rr(r, 0.1, 0.25, 2))
    macros(p)


@arch(BL, "Mallet", ["Kalimba", "Thumb Piano", "Mbira", "African Kalimba", "Kalimba Lullaby"], role="real", chars=["clean", "wide", "lofi", "fx", "dark", "bright"])
def _(p, r, i):
    mallet_sample(p, r, "Kalimba", rev=("Room", (0.1, 0.2)))
    macros(p)


@arch(BL, "Mallet", ["Synth Mallet", "Soft Mallet", "Wood Mallet", "Bamboo Mallet", "Dream Mallet"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["Sine", "Triangle", "Glass", "Octaves"]), pos=0.3, level=0.7)
    p.osc(1, wt="Sine", oct=2, level=0.0)
    p.env(2, dec=rr(r, 0.04, 0.12, 3), sus=0.0); p.mod("Env 3", "Osc B Level", 0.35)
    p.filt(0, type="LP 12", cutoff=rr(r, 2000, 6000, 0))
    p.env(0, att=0.0, dec=rr(r, 0.5, 1.5, 2), sus=0.0, rel=0.5)
    reverb(p, r, "Hall", mix=(0.12, 0.25)); delay(p, r, mix=(0.06, 0.14))
    macros(p)


@arch(BL, "Chimes", ["Hand Chimes", "Chime Choir", "Bright Chimes", "Holy Chimes", "Temple Chimes"], role="real", chars=["clean", "wide", "bright", "fx", "dark"])
def _(p, r, i):
    mallet_sample(p, r, "Hand Chimes", rev=("Hall", (0.2, 0.35)))
    macros(p)


@arch(BL, "Chimes", ["Wine Glass", "Glass Harp", "Crystal Glass", "Singing Glass"], role="real", chars=["clean", "wide", "fx", "dark"])
def _(p, r, i):
    p.set(polyphony=12)
    p.sample("Wine Glasses", level=0.85)
    p.env(0, att=0.2, dec=1.0, sus=1.0, rel=1.5)
    reverb(p, r, "Hall", mix=(0.25, 0.4))
    macros(p)


@arch(BL, "Chimes", ["Wind Chimes", "Garden Chimes", "Breeze Chimes", "Porch Chimes"], role="keys")
def _(p, r, i):
    p.osc(0, wt=pick(r, ["FM 1:9 Glass", "Glass", "FM Bell"]), pos=0.5, level=0.6)
    p.env(0, att=0.0, dec=rr(r, 1.5, 3.0, 2), sus=0.0, rel=1.5)
    p.arpeggio(mode="Random", rate=pick(r, ["1/16", "1/8 T", "1/16 T"]), oct=pick(r, [2, 3]), gate=0.3, scale=pick(r, ["Major Pentatonic", "Whole Tone", "Lydian"]))
    p.mod("Random", "Amp", -0.4)
    reverb(p, r, "Hall", mix=(0.3, 0.45))
    macros(p)
