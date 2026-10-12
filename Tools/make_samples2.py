#!/usr/bin/env python3
"""Builds the second wave of PYRA sample instruments (v0.9) from free libraries hosted on GitHub.

Every source is CC0 / public domain / Unlicense, except the Greg Sullivan e-pianos (CC-BY 3.0) and the
MTG solo saxophones (CC-BY 4.0), which are credited in CREDITS.txt and in SETTINGS > INFO.

For each instrument: reads the SFZ mapping (or the file names), picks the notes and velocity layers to
keep, sums the microphone layers that sound together, resamples to 44.1 kHz, trims, checks the tuning
(and the octave), levels every note, and either fades decaying sounds or bakes a seamless loop into
sustained ones. Writes 16-bit WAVs + instrument.json; PyraRenderTest --convert turns them into Ogg Vorbis.

    python3 Tools/make_samples2.py <sources_dir> <stage_dir> [ids...]
"""
import json, math, os, re, sys
import numpy as np
from scipy.io import wavfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sfzlib
from make_samples import envelope, estimate_cents, bake_loop

SR = sfzlib.SR


def detect_midi(x):
    """Absolute pitch (MIDI, fractional) of a recorded note with YIN, or None."""
    mono = x.mean(1)
    for start in (int(0.25 * SR), int(0.08 * SR), 0):
        seg = mono[start:start + int(0.3 * SR)]
        if len(seg) >= int(0.12 * SR): break
    W = len(seg) // 2
    taumax, taumin = min(int(SR / 25), W - 1), int(SR / 4200)
    if W < 256 or taumax <= taumin + 2: return None
    d = np.array([np.sum((seg[:W] - seg[t:t + W]) ** 2) for t in range(taumax)])
    cm = d.copy(); cm[1:] = d[1:] * np.arange(1, taumax) / np.maximum(np.cumsum(d[1:]), 1e-12); cm[0] = 1
    for t in range(max(taumin, 1), taumax - 1):
        if cm[t] < 0.12 and cm[t] <= cm[t + 1]:
            a, b, c = cm[t - 1], cm[t], cm[t + 1]
            den = a - 2 * b + c
            tau = t + (0.5 * (a - c) / den if abs(den) > 1e-12 else 0.0)
            return 69 + 12 * math.log2(SR / tau / 440)
    return None


def I(iid, name, cat, repo, src, notes=3, vels=(100,), include=None, exclude=r"(?i)noise|release|rel_|_rel|scrape|slide|harmonic|fx|thump",
      sustained=False, max_s=4.0, kit=False, mono=False, tune=True, gain_db=0.0, att=0.0, rel=0.3, keys=None, trigger="attack",
      lo=0, hi=127, transpose=0, c4=60, skip_start=0.0, fade_s=0.4):
    return dict(id=iid, name=name, cat=cat, repo=repo, src=src, notes=notes, vels=vels, include=include, exclude=exclude,
                sustained=sustained, max_s=max_s, kit=kit, mono=mono, tune=tune, gain_db=gain_db, att=att, rel=rel, keys=keys,
                trigger=trigger, lo=lo, hi=hi, transpose=transpose, c4=c4, skip_start=skip_start, fade_s=fade_s)


VCSL = "VCSL"
INSTRUMENTS = [
    # ---------------------------------------------------------------- keyboards
    I("concert_grand", "Concert Grand (Splendid)", "Keyboard", "SplendidGrandPiano", "Splendid Grand Piano.sfz", notes=4, vels=(60, 112), max_s=6.0, rel=0.5, exclude=r"(?i)rel|harm|pedal"),
    I("vintage_piano", "Vintage Piano", "Keyboard", "fp-old-piano-FB", "PianoFB small 20200401.sfz", notes=3, vels=(100,), max_s=6.0, rel=0.4),
    I("wurlitzer", "Wurlitzer EP200", "Keyboard", "GregSullivan.E-Pianos", "Wurlitzer EP200/Wurlitzer EP200.sfz", notes=3, vels=(50, 110), max_s=5.0, rel=0.25),
    I("pianet", "Pianet T", "Keyboard", "GregSullivan.E-Pianos", "Pianet T/Pianet T.sfz", notes=4, vels=(60, 110), max_s=4.0, rel=0.15),
    I("cp80", "Electric Grand CP80", "Keyboard", "GregSullivan.E-Pianos", "CP80/CP80.sfz", notes=5, vels=(60, 115), max_s=5.0, rel=0.4),
    I("fm_epiano", "FM E-Piano (TX81Z)", "Keyboard", VCSL, "names:Electrophones/TX81Z/FM Piano", notes=5, vels=(2, 3), max_s=4.0, rel=0.3, c4=60),
    I("harpsichord", "Harpsichord", "Guitar", VCSL, "names:Chordophones/Zithers/Harpsichord, Flemish/Sustains", notes=3, vels=(1,), max_s=4.0, rel=0.25, exclude=r"(?i)release"),
    I("clavisynth", "Clavi (TX81Z)", "Guitar", VCSL, "names:Electrophones/TX81Z/Clavisynth", notes=3, vels=(2, 3), max_s=2.5, rel=0.1),
    # ---------------------------------------------------------------- organs
    I("church_organ", "Church Organ (full)", "Organs", VCSL, "names:Aerophones/Edge-blown Aerophones/Pipe Organ/Loud", notes=3, sustained=True, max_s=5.0, att=0.05, rel=0.6),
    I("pipe_organ_soft", "Pipe Organ (soft)", "Organs", VCSL, "names:Aerophones/Edge-blown Aerophones/Pipe Organ/Quiet", notes=3, sustained=True, max_s=5.0, att=0.06, rel=0.5),
    I("renaissance_organ", "Renaissance Organ", "Organs", VCSL, "names:Aerophones/Edge-blown Aerophones/Renaissance Organ/8'", notes=3, sustained=True, max_s=4.0, att=0.04, rel=0.3),
    # ---------------------------------------------------------------- bells and mallets
    I("tubular_bells", "Tubular Bells", "Bells", "fp-tubular-bells1", "TubularBells small 20241130.sfz", notes=1, max_s=7.0, tune=False, rel=1.0),
    I("vibraphone", "Vibraphone", "Bells", VCSL, "names:Idiophones/Struck Idiophones/Vibraphone/Soft Mallets", notes=3, vels=(1, 2), max_s=5.0, rel=0.6),
    I("xylophone", "Xylophone", "Bells", VCSL, "names:Idiophones/Struck Idiophones/Xylophone/Medium Mallets", notes=3, vels=(2,), max_s=1.8, tune=False, rel=0.2, c4=72),   # VCSL names these an octave under the sounding pitch
    I("steel_pan", "Steel Pan", "Bells", "jlearman.SteelDrum", "jSteelDrum.sfz", notes=2, vels=(60, 115), max_s=3.0, tune=False, rel=0.4, exclude=r"(?i)-\d\.flac$"),
    I("hand_chimes", "Hand Chimes", "Bells", VCSL, "names:Idiophones/Struck Idiophones/Hand Chimes", notes=3, vels=(1,), max_s=4.0, tune=False, rel=0.8, c4=72),   # VCSL names these an octave under the sounding pitch
    I("kalimba", "Kalimba", "Bells", VCSL, "names:Idiophones/Plucked Idiophones/Kalimba, Kenya", notes=1, vels=(3,), max_s=2.5, tune=False, rel=0.3, c4=72),   # VCSL names these an octave under the sounding pitch
    I("balafon", "Balafon", "Bells", VCSL, "names:Idiophones/Struck Idiophones/Balafon/Hard Mallet", notes=2, vels=(1,), max_s=1.6, tune=False, rel=0.2, c4=72),   # VCSL names these an octave under the sounding pitch
    I("wine_glass", "Wine Glasses", "Bells", VCSL, "names:Idiophones/Friction Idiophones/Wine Glasses/Sustains", notes=1, sustained=True, max_s=4.0, tune=False, att=0.3, rel=0.8, c4=72),   # VCSL names these an octave under the sounding pitch
    I("hang_drum", "Hang Drum", "Drums", "fp-hang-D-minor", "Hang-D-minor 20220330.sfz", notes=1, max_s=4.0, tune=False, rel=0.8),
    # ---------------------------------------------------------------- guitars and plucked
    I("nylon_guitar", "Classical Guitar", "Guitar", "spanish-classical-guitar", "SpanishClassicalGuitar-20190618.sfz", notes=3, max_s=4.0, rel=0.4),
    I("archtop_guitar", "Archtop Guitar", "Guitar", "karoryfer.shinyguitar", "Programs/acoustic_one.sfz", notes=3, vels=(60, 110), max_s=3.5, rel=0.3),
    I("clean_guitar", "Electric Guitar (clean)", "Guitar", "fp-e-guitar-FSBS-clean", "EGuitarFSBS-clean bridge small 20260807.sfz", notes=3, max_s=4.0, rel=0.35),
    I("twang_guitar", "Electric Guitar (twang)", "Guitar", "karoryfer.black-and-green-guitars", "Programs/04-green_twang.sfz", notes=3, vels=(70, 115), max_s=3.0, rel=0.3),
    I("drive_guitar", "Electric Guitar (overdrive)", "Guitar", "fp-e-guitar-FSBS-dist2", "EGuitarFSBS-dist2 bridge 20220911.sfz", notes=3, vels=(110,), max_s=3.5, sustained=False, rel=0.25),
    I("ukulele", "Ukulele", "Guitar", "fp-ukulele1", "Ukulele 20260811.sfz", notes=2, max_s=2.5, rel=0.3),
    I("banjo", "Banjo", "Guitar", "ganjo", "ganjo.sfz", notes=2, max_s=2.5, rel=0.25),
    I("lyre", "Medieval Lyre", "Guitar", "cithara-barbarica", "Programs/02-cithara_barbarica_finger.sfz", notes=2, max_s=4.0, rel=0.5),
    I("zither", "Zither", "Guitar", "hungarian_zither", "Programs/hungarian_zither_mel_strings_poly.sfz", notes=2, max_s=4.0, rel=0.5),
    I("strumstick", "Strumstick", "Guitar", VCSL, "names:Chordophones/Composite Chordophones/Strumstick/Finger", notes=2, vels=(3,), max_s=3.0, rel=0.3),
    I("dan_tranh", "Dan Tranh (zither)", "Guitar", VCSL, "names:Chordophones/Zithers/Dan Tranh/Normal", notes=2, vels=(2,), max_s=3.5, rel=0.4),
    I("folk_harp", "Folk Harp", "Guitar", VCSL, "names:Chordophones/Composite Chordophones/Folk Harp", notes=3, vels=(2,), max_s=4.0, rel=0.6),
    # ---------------------------------------------------------------- basses
    I("finger_bass", "Electric Bass (finger)", "Bass", "fp-electric-bass-YR", "FingerBassYR 20190930.sfz", notes=1, max_s=3.0, rel=0.15),
    I("picked_bass", "Electric Bass (pick)", "Bass", "fp-electric-bass-YR", "PickedBassYR 20190930.sfz", notes=1, max_s=3.0, rel=0.15),
    I("growl_bass", "Electric Bass (growl)", "Bass", "karoryfer.growlybass", "growlybass_dirty.sfz", notes=3, vels=(100,), max_s=3.0, rel=0.15),
    I("upright_bass", "Upright Bass (pizz)", "Bass", "dsmolken.double-bass", "d_smolken_rubner_bass_pizz.sfz", notes=3, vels=(60, 110), max_s=3.0, rel=0.2),
    # ---------------------------------------------------------------- strings
    I("solo_cello", "Solo Cello", "Strings", "karoryfer-bigcat.cello", "Programs/01- Bowed (velocity layer).sfz", notes=4, vels=(70, 110), sustained=True, max_s=5.0, att=0.08, rel=0.4),
    I("erhu", "Erhu", "Strings", "aliexpress-erhu", "Programs/02-erhu_long.sfz", notes=2, sustained=True, max_s=4.0, att=0.06, rel=0.3, trigger="first"),
    # ---------------------------------------------------------------- brass and reeds
    I("alto_sax", "Alto Sax", "Brass", "MTG.SoloSax", "MTG Solo Saxophones/MTG Alto Sax.sfz", notes=3, vels=(60, 120), sustained=True, max_s=4.0, att=0.02, rel=0.2),
    I("tenor_sax", "Tenor Sax", "Brass", VCSL, "names:Aerophones/Reed Aerophones/Tenor Saxophone/Vibrato", notes=3, vels=(1,), sustained=True, max_s=4.0, att=0.02, rel=0.2),
    I("bari_sax", "Baritone Sax", "Brass", "MTG.SoloSax", "MTG Solo Saxophones/MTG Baritone Sax.sfz", notes=3, vels=(110,), sustained=True, max_s=4.0, att=0.02, rel=0.2),
    I("muted_trumpet", "Muted Trumpet", "Brass", "VSCO-2-CE", "names:Brass/Trumpet/straightM-sus", notes=3, vels=(1,), sustained=True, max_s=4.0, att=0.02, rel=0.2),
    # ---------------------------------------------------------------- winds
    I("recorder", "Recorder", "Wind", VCSL, "names:Aerophones/Edge-blown Aerophones/Baroque Alto Recorder/Sustain", notes=2, vels=(1,), sustained=True, max_s=3.5, att=0.03, rel=0.15),
    I("piccolo", "Piccolo", "Wind", "VSCO-2-CE", "names:Woodwinds/Piccolo/Sus", notes=3, vels=(1,), sustained=True, max_s=3.5, att=0.03, rel=0.15),
    I("ocarina", "Ocarina", "Wind", VCSL, "names:Aerophones/Edge-blown Aerophones/Ocarina, Typical/Sustains", notes=2, vels=(1,), sustained=True, max_s=3.5, att=0.03, rel=0.15),
    I("harmonica", "Harmonica", "Wind", VCSL, "names:Aerophones/Free Aerophones/Harmonica-Hohner-Super64/Sustains/Normal", notes=2, vels=(1,), sustained=True, max_s=3.5, att=0.02, rel=0.15),
    I("accordion", "Accordion", "Wind", "fp-button-accordion-HN", "PRESET Button Accordion HN tuned.sfz", notes=1, sustained=True, max_s=4.0, att=0.04, rel=0.15),
    I("bagpipe", "Bagpipe", "Wind", "fp-bagpipe", "Bagpipe small 20260806.sfz", notes=1, sustained=True, max_s=4.0, att=0.05, rel=0.3, exclude=r"(?i)drone"),
    # ---------------------------------------------------------------- voice
    I("voice_aah", "Voice Aah", "Vox", "legato_vocal_tutorial", "Programs/01-no_legato_polyphonic_only_no_vibrato_a.sfz", notes=2, sustained=True, max_s=4.0, att=0.08, rel=0.4),
    # ---------------------------------------------------------------- drums (one sound per key, kits follow GM)
    I("studio_kit", "Studio Drum Kit", "Drums", "virtuosity_drums", "Programs/01-basic-kit.sfz", kit=True, vels=(70, 118), max_s=3.0, tune=False, rel=0.3,
      keys=[35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 53, 55, 57, 59]),
    I("rusty_kit", "Vintage Drum Kit", "Drums", "karoryfer.big-rusty-drums", "Programs/02-basic.sfz", kit=True, vels=(70, 118), max_s=3.0, tune=False, rel=0.3,
      keys=[36, 37, 38, 40, 42, 43, 44, 45, 46, 47, 49, 50, 51, 53, 55]),
    I("jazz_kit", "Jazz Drum Kit", "Drums", "karoryfer.swirly-drums", "Programs/Basic_kit.sfz", kit=True, vels=(60, 110), max_s=3.0, tune=False, rel=0.3,
      keys=[36, 38, 39, 40, 41, 42, 44, 45, 46, 47, 49, 51]),
    I("world_perc", "World Percussion", "Drums", "fp-world-percussion", "WorldPercussion 20200905.sfz", kit=True, vels=(100,), max_s=2.0, tune=False, rel=0.3),
    I("drum_machine", "Vintage Drum Machine", "Drums", "fp-synthesizer-percussion", "SynthesizerPercussion-20220718.sfz", kit=True, vels=(100,), max_s=2.5, tune=False, rel=0.3),
]

# orchestral percussion: one VCSL / VSCO file per key (GM-style)
ORCH_PERC = [
    (35, "VCSL", "Membranophones/Struck Membranophones/Bass Drum 1", r"(?i)hit"), (36, "VCSL", "Membranophones/Struck Membranophones/Bass Drum 2", r""),
    (38, "VCSL", "Membranophones/Struck Membranophones/Snare Drum, Modern 1", r"(?i)hit|center|norm"), (40, "VCSL", "Membranophones/Struck Membranophones/Snare Drum, Rope Tension/Hi", r""),
    (41, "VCSL", "Membranophones/Struck Membranophones/Tom 1/Mallet", r""), (43, "VCSL", "Membranophones/Struck Membranophones/Tom 2/Mallet", r""),
    (45, "VCSL", "Membranophones/Struck Membranophones/Tom 1/Stick", r""), (47, "VCSL", "Membranophones/Struck Membranophones/Tom 2/Stick", r""),
    (49, "VCSL", "Idiophones/Struck Idiophones/Clash Cymbals 1", r""), (51, "VCSL", "Idiophones/Struck Idiophones/Suspended Cymbal 1", r""),
    (52, "VCSL", "Idiophones/Struck Idiophones/Gong 1", r""), (53, "VCSL", "Idiophones/Struck Idiophones/Gong 2", r""),
    (54, "VCSL", "Idiophones/Struck Idiophones/Tambourine 1", r""), (56, "VCSL", "Idiophones/Struck Idiophones/Triangles", r""),
    (57, "VCSL", "Idiophones/Struck Idiophones/Clash Cymbals 2", r""), (59, "VCSL", "Idiophones/Struck Idiophones/Suspended Cymbal 2", r""),
    (60, "VCSL", "Membranophones/Struck Membranophones/Timpani 1/Hit", r""), (62, "VCSL", "Idiophones/Struck Idiophones/Anvil", r""),
    (64, "VCSL", "Idiophones/Struck Idiophones/Mark Trees", r""), (65, "VCSL", "Idiophones/Struck Idiophones/Bell Tree", r""),
    (67, "VCSL", "Idiophones/Struck Idiophones/Sleigh Bells", r""), (69, "VCSL", "Idiophones/Struck Idiophones/Ratchet", r""),
    (71, "VCSL", "Idiophones/Struck Idiophones/Slapstick", r""), (72, "VCSL", "Idiophones/Struck Idiophones/Finger Cymbals", r""),
]
HAND_PERC = [
    (36, "VCSL", "Membranophones/Struck Membranophones/Frame Drum", r""), (37, "VCSL", "Idiophones/Struck Idiophones/Claves", r""),
    (38, "VCSL", "Idiophones/Struck Idiophones/Cajon", r"(?i)snare|hit"), (39, "VCSL", "Idiophones/Struck Idiophones/Claps", r""),
    (41, "VCSL", "Membranophones/Struck Membranophones/Darbuka", r"(?i)doum|dum"), (43, "VCSL", "Membranophones/Struck Membranophones/Darbuka", r"(?i)tek|tak"),
    (44, "VCSL", "Idiophones/Struck Idiophones/Shaker, Small", r""), (46, "VCSL", "Idiophones/Struck Idiophones/Shaker, Large", r""),
    (48, "VCSL", "Membranophones/Struck Membranophones/Bongos", r"(?i)hi"), (50, "VCSL", "Membranophones/Struck Membranophones/Bongos", r"(?i)lo"),
    (52, "VCSL", "Membranophones/Struck Membranophones/Conga", r"(?i)open|hitn"), (53, "VCSL", "Membranophones/Struck Membranophones/Conga", r"(?i)slap|hitfm"),
    (54, "VCSL", "Idiophones/Struck Idiophones/Tambourine 2", r""), (55, "VCSL", "Idiophones/Struck Idiophones/Cabasa", r""),
    (56, "VCSL", "Idiophones/Struck Idiophones/Cowbells", r""), (57, "VCSL", "Idiophones/Struck Idiophones/Agogo Bells", r"(?i)hi|high"),
    (58, "VCSL", "Idiophones/Struck Idiophones/Vibraslap", r""), (59, "VCSL", "Idiophones/Struck Idiophones/Agogo Bells", r"(?i)lo"),
    (60, "VCSL", "Idiophones/Struck Idiophones/Guiro", r""), (62, "VCSL", "Idiophones/Struck Idiophones/Woodblock", r""),
    (64, "VCSL", "Idiophones/Struck Idiophones/Slit Drum", r""), (65, "VCSL", "Idiophones/Struck Idiophones/Brake Drum", r""),
    (67, "VCSL", "Membranophones/Other Membranophones/Ocean Drum", r""),
]


# ----------------------------------------------------------------------------- picking the regions
_FILES = {}


def repo_files(repo):
    if repo not in _FILES:
        out = os.popen(f'git -C "{repo}" ls-tree -r --name-only HEAD').read().split("\n")
        _FILES[repo] = set(f for f in out if f)
    return _FILES[repo]


def resolve(repo, path, sfz):
    """Finds the real file for a sample path written in an SFZ (relative paths, missing default_path...)."""
    files = repo_files(repo)
    p = os.path.normpath(path).replace("\\", "/")
    if p in files: return p
    tail = p.split("/")
    for k in range(1, len(tail)):
        sub = "/".join(tail[k:])
        for pre in ("", "Samples/", os.path.dirname(sfz) + "/", os.path.dirname(os.path.dirname(sfz)) + "/Samples/"):
            c = os.path.normpath(pre + sub)
            if c in files: return c
    return None


def pick_regions(inst, src_dir):
    repo = os.path.join(src_dir, inst["repo"])
    if inst["src"].startswith("names:"):
        regs = sfzlib.regions_from_names(repo, inst["src"][6:])
        if not regs:   # one level deeper (articulation folders)
            regs = []
            for sub in os.popen(f'git -C "{repo}" ls-tree --name-only HEAD "{inst["src"][6:]}/"').read().split("\n"):
                if sub: regs += sfzlib.regions_from_names(repo, sub)
        for r in regs:
            if r["root"] is not None: r["root"] = r["lo"] = r["hi"] = r["root"] + (inst["c4"] - 60)
        named = True
    else:
        os.system(f'git -C "{repo}" checkout HEAD -- "*.sfz" "*.txt" > /dev/null 2>&1')
        regs = sfzlib.parse_sfz(repo, inst["src"])
        for r in regs: r["sample"] = resolve(repo, r["sample"], inst["src"])
        regs = [r for r in regs if r["sample"]]
        named = False
    regs = [r for r in regs if r["trigger"] == inst["trigger"] and r["root"] is not None]
    if inst["include"]: regs = [r for r in regs if re.search(inst["include"], os.path.basename(r["sample"]))]
    if inst["exclude"]: regs = [r for r in regs if not re.search(inst["exclude"], os.path.basename(r["sample"]))]
    # one round robin per note / velocity / microphone: the lowest sequence position and random range
    groups = {}
    for r in regs:
        key = (r["root"], r["lovel"], r["hivel"], os.path.dirname(r["sample"]) if not named else "")
        rank = (r["seq"], float(r["ops"].get("lorand", 0) or 0), r["sample"])
        if key not in groups or rank < groups[key][0]: groups[key] = (rank, r)
    regs = [g[1] for g in groups.values()]
    return regs, named


def plan(inst, regs, named):
    """-> list of (note, vel_label, [regions to sum])."""
    jobs = []
    if named:
        layers = sorted({r["lovel"] for r in regs})
        if not layers: return []
        want = [v for v in inst["vels"] if v in layers] or [layers[-1]]
        roots = sorted({r["root"] for r in regs if r["lovel"] in want})
    else:
        want = list(inst["vels"])
        roots = sorted({r["root"] for r in regs})
    if inst["kit"]:
        keys = inst["keys"] or sorted({k for r in regs for k in range(max(r["lo"], 0), min(r["hi"], 127) + 1)})
        for k in keys:
            for v in want:
                hit = [r for r in regs if r["lo"] <= k <= r["hi"] and r["lovel"] <= v <= r["hivel"]]
                if hit: jobs.append((k, v, hit))
        return jobs
    roots = [n for n in roots if inst["lo"] <= n <= inst["hi"]]
    chosen = []
    for n in roots:
        if not chosen or n - chosen[-1] >= inst["notes"]: chosen.append(n)
    for n in chosen:
        for v in want:
            if named: hit = [r for r in regs if r["root"] == n and r["lovel"] == v]
            else:
                hit, per_dir = [r for r in regs if r["root"] == n and r["lovel"] <= v <= r["hivel"]], {}
                for h in sorted(hit, key=lambda h: h["hivel"] - h["lovel"]): per_dir.setdefault(os.path.dirname(h["sample"]), h)
                hit = list(per_dir.values())
            if hit: jobs.append((n, v, hit))
    return jobs


def render(inst, src_dir, hits):
    repo = os.path.join(src_dir, inst["repo"])
    mix = None
    for r in hits:
        path = os.path.join(repo, r["sample"])
        if not os.path.exists(path):
            alt = [os.path.join(repo, os.path.dirname(inst["src"]), "..", r["sample"]), os.path.join(repo, r["sample"].split("/", 1)[-1])]
            path = next((a for a in alt if os.path.exists(a)), path)
        if not os.path.exists(path): continue
        x = sfzlib.decode(path)
        if r["offset"]: x = x[int(r["offset"] * SR / 44100):]
        g = 10 ** (r["volume"] / 20)
        if mix is None: mix = x * g
        else:
            n = max(len(mix), len(x))
            m2 = np.zeros((n, 2), np.float32); m2[:len(mix)] += mix; m2[:len(x)] += x * g; mix = m2
    return mix


def build(inst, src_dir, out_dir, report):
    regs, named = pick_regions(inst, src_dir)
    jobs = plan(inst, regs, named)
    if not jobs:
        report.append(f"!! {inst['id']}: no regions"); return None
    repo = os.path.join(src_dir, inst["repo"])
    need = sorted({r["sample"] for _, _, hits in jobs for r in hits})
    missing = sfzlib.fetch(repo, need)
    if missing:   # paths relative to the sfz folder
        alt = [os.path.normpath(os.path.join(os.path.dirname(inst["src"]), "..", m)) for m in missing]
        sfzlib.fetch(repo, alt)

    os.makedirs(out_dir, exist_ok=True)
    zones, vels = [], sorted({v for _, v, _ in jobs})
    layer_rms = {v: [] for v in vels}
    for note, v, hits in jobs:
        x = render(inst, src_dir, hits)
        if x is None or len(x) < 256: report.append(f"  ? {inst['id']} {note}/{v}: missing audio"); continue
        if inst["skip_start"]: x = x[int(inst["skip_start"] * SR):]
        env = envelope(x)
        pk = env.max()
        if pk < 1e-5: continue
        on = int(np.argmax(env > pk * 0.003))
        x = x[max(0, on - int(0.002 * SR)):]
        env = envelope(x)
        root = hits[0]["root"] if not inst["kit"] else note
        tune = float(hits[0]["tune"]) if not named else 0.0     # SFZ "tune" = the correction the sample needs
        if inst["tune"] and not inst["kit"]:
            cents = estimate_cents(x, root)                      # how far the recording is from its note
            if cents is not None and abs(cents) <= 130: tune = -round(cents, 1)
        if inst["sustained"]:
            tail = len(x) - int(0.2 * SR)
            loud = np.where(env > env.max() * 0.15)[0]
            if len(loud): tail = min(tail, loud[-1] - int(0.05 * SR))
            end = min(tail, int(inst["max_s"] * SR))
            looped = bake_loop(x, min(int(0.45 * SR), int(len(x) * 0.3)), end) if end > int(0.5 * SR) else None
            if looped is None: y, ls, le = x[:max(end, 1)].copy(), 0, 0
            else: y, ls, le = looped
        else:
            below = np.where(envelope(x, 0.05) > env.max() * 0.001)[0]
            end = min(int(inst["max_s"] * SR), (below[-1] if len(below) else len(x)) + 1)
            y = x[:end].copy()
            fl = min(int(inst["fade_s"] * SR), max(1, len(y) // 6))
            y[-fl:] *= np.linspace(1, 0, fl, dtype=np.float32)[:, None] ** 2
            ls = le = 0
        if inst["mono"]: y = np.repeat(y.mean(1, keepdims=True), 2, 1)
        rms = float(np.sqrt(np.mean(y[: min(len(y), int(0.5 * SR))] ** 2)) + 1e-9)
        layer_rms[v].append(rms)
        det = detect_midi(x) if inst["tune"] and not inst["kit"] else None
        zones.append({"file": f"{note}_{v}.ogg", "root": int(root), "key": note, "vel": v, "tune": round(tune, 1), "det": det,
                      "loopStart": int(ls), "loopEnd": int(le), "rms": rms, "_data": y})
    if not zones: report.append(f"!! {inst['id']}: nothing rendered"); return None
    # octave check: names / maps that count octaves differently (C3 = middle C, ...)
    deltas = [z["det"] - (z["root"] - z["tune"] / 100.0) for z in zones if z["det"] is not None]
    shift = 0
    if len(deltas) >= 2:
        med = float(np.median(deltas))
        if abs(med) > 6: shift = int(12 * round(med / 12))
    for z in zones:
        z["root"] += shift; z["key"] += shift
        z.pop("det", None)
    if shift: report.append(f"  ~ {inst['id']}: octave corrected by {shift:+d}")

    loudest = max(np.median(layer_rms[v]) for v in vels if layer_rms[v])
    layer_gain = {v: float(np.clip((np.median(layer_rms[v]) / loudest) ** 0.5, 0.35, 1.0)) if layer_rms[v] else 1.0 for v in vels}
    target = 0.12 * 10 ** (inst["gain_db"] / 20)
    for z in zones:
        if inst["kit"]:   # drums keep their natural balance (a hi-hat is quieter than a kick)
            kit_ref = loudest
            g = target / kit_ref * 1.6
        else:
            g = target / z["rms"] * layer_gain[z["vel"]]
        y = z.pop("_data") * g
        pk = np.abs(y).max()
        if pk > 0.98: y *= 0.98 / pk
        wavfile.write(os.path.join(out_dir, z["file"].replace(".ogg", ".wav")), SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
        del z["rms"]

    # key ranges: between neighbours (kits: every key is its own sound)
    if inst["kit"]:
        for z in zones: z["lo"] = z["hi"] = z["key"]
    else:
        keys = sorted({z["key"] for z in zones})
        rng = {}
        for i, k in enumerate(keys):
            lo = keys[i - 1] + (k - keys[i - 1]) // 2 + 1 if i > 0 else max(0, k - 12)
            hi = k + (keys[i + 1] - k) // 2 if i + 1 < len(keys) else min(127, k + 12)
            rng[k] = (lo, hi)
        for z in zones: z["lo"], z["hi"] = rng[z["key"]]
    edges = {v: (int(round(127 * k / len(vels))) + (1 if k else 0), int(round(127 * (k + 1) / len(vels)))) for k, v in enumerate(vels)}
    for z in zones:
        z["velLo"], z["velHi"] = edges[z["vel"]]
        if inst["kit"]: z["root"] = z["key"]
        del z["vel"], z["key"]

    meta = {"id": inst["id"], "name": inst["name"], "category": inst["cat"], "sampleRate": SR, "sustained": inst["sustained"],
            "attack": inst["att"], "release": inst["rel"], "zones": sorted(zones, key=lambda z: (z["lo"], z["velLo"]))}
    with open(os.path.join(out_dir, "instrument.json"), "w") as fp:
        json.dump(meta, fp, indent=1)
    secs = sum(os.path.getsize(os.path.join(out_dir, z["file"].replace(".ogg", ".wav"))) for z in zones) / (SR * 4)
    report.append(f"{inst['id']:18s} {len({z['lo'] for z in zones}):3d} keys x {len(vels)} vel  {secs:6.1f} s audio")
    return meta


def build_custom_kit(iid, name, table, src_dir, out_dir, report):
    """A kit assembled from single files: (key, repo, folder, regex)."""
    inst = I(iid, name, "Drums", "VCSL", "", kit=True, tune=False, max_s=3.0, rel=0.3)
    os.makedirs(out_dir, exist_ok=True)
    zones = []
    for key, repo, folder, rx in table:
        regs = sfzlib.regions_from_names(os.path.join(src_dir, repo), folder)
        if not regs:   # sub folders
            regs = []
            for sub in os.popen(f'git -C "{os.path.join(src_dir, repo)}" ls-tree --name-only HEAD "{folder}/"').read().split("\n"):
                if sub: regs += sfzlib.regions_from_names(os.path.join(src_dir, repo), sub)
        cand = [r for r in regs if (not rx or re.search(rx, os.path.basename(r["sample"]))) and r["seq"] == 1]
        cand = cand or [r for r in regs if not rx or re.search(rx, os.path.basename(r["sample"]))] or regs
        if not cand: report.append(f"  ? {iid} key {key}: nothing in {folder}"); continue
        cand.sort(key=lambda r: (-r["lovel"], r["sample"]))
        r = cand[min(1, len(cand) - 1)] if len(cand) > 2 else cand[0]   # a strong, not the very hardest, hit
        sfzlib.fetch(os.path.join(src_dir, repo), [r["sample"]])
        x = sfzlib.decode(os.path.join(src_dir, repo, r["sample"]))
        if len(x) < 64: continue
        env = envelope(x); on = int(np.argmax(env > env.max() * 0.003)); x = x[max(0, on - 88):]
        x = x[: int(inst["max_s"] * SR)]
        fl = min(int(0.3 * SR), len(x) // 6); x[-fl:] *= np.linspace(1, 0, fl, dtype=np.float32)[:, None] ** 2
        pk = np.abs(x).max() + 1e-9
        y = x * (0.7 / pk)
        fn = f"{key}_1.ogg"
        wavfile.write(os.path.join(out_dir, fn.replace(".ogg", ".wav")), SR, (np.clip(y, -1, 1) * 32767).astype(np.int16))
        zones.append({"file": fn, "root": key, "tune": 0.0, "loopStart": 0, "loopEnd": 0, "lo": key, "hi": key, "velLo": 0, "velHi": 127})
    meta = {"id": iid, "name": name, "category": "Drums", "sampleRate": SR, "sustained": False, "attack": 0.0, "release": 0.3, "zones": zones}
    with open(os.path.join(out_dir, "instrument.json"), "w") as fp: json.dump(meta, fp, indent=1)
    report.append(f"{iid:18s} {len(zones):3d} keys (assembled)")


def main(src_dir, out_root, only):
    report = []
    for inst in INSTRUMENTS:
        if only and inst["id"] not in only: continue
        try: build(inst, src_dir, os.path.join(out_root, inst["id"]), report)
        except Exception as e: report.append(f"!! {inst['id']}: {e}")
        print(report[-1], flush=True)
    for iid, name, table in (("orch_perc", "Orchestral Percussion", ORCH_PERC), ("hand_perc", "Hand Percussion", HAND_PERC)):
        if only and iid not in only: continue
        try: build_custom_kit(iid, name, table, src_dir, os.path.join(out_root, iid), report)
        except Exception as e: report.append(f"!! {iid}: {e}")
        print(report[-1], flush=True)


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], set(sys.argv[3:]))
