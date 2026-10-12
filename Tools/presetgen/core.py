"""PYRA preset generator - building blocks.

Every value is written by name (wavetable, filter type, LFO shape, mod source/destination...) and checked
against Tools/params.json (dumped by `PyraRenderTest --choices`), so a preset can never hold a value the
plugin does not have.
"""
import json, math, os, random

HERE = os.path.dirname(os.path.abspath(__file__))
PARAMS = json.load(open(os.path.join(HERE, "..", "params.json")))


def choice_index(param, name):
    ch = PARAMS[param]["choices"]
    for i, c in enumerate(ch):
        if c == name: return i
    low = name.lower()
    for i, c in enumerate(ch):
        if c.lower() == low: return i
    raise KeyError(f"{param}: no choice '{name}' (have {ch[:12]}...)")


def cut(hz):
    """Cutoff in Hz -> modulation distance (the cutoff knob is logarithmic 20 Hz .. 20 kHz)."""
    return math.log(hz / 20.0) / math.log(1000.0)


SEMI = 1.0 / 48.0      # one semitone of "Osc Pitch" modulation
CENT = 1.0 / 200.0     # one cent of "Osc Fine" modulation

OSC = "ABC"
FX_TYPE_FIELD = {"dist": "type", "rev": "type", "flt": "type", "dly": "mode", "shf": "mode", "res": "chord"}
DIVS = PARAMS["lfo1_div"]["choices"]


class P:
    """One preset under construction."""

    def __init__(self, name, cat, sub, tags=()):
        self.name, self.cat, self.subcat, self.tags = name, cat, sub, list(tags)
        self.p, self.mods, self.arp, self.chords, self.focus_slots = {}, [], None, None, []
        self.sample_inst = None

    # ------------------------------------------------------------------ raw values
    def set(self, **kw):
        for k, v in kw.items(): self._put(k, v)
        return self

    def _put(self, key, v):
        if key not in PARAMS: raise KeyError(f"unknown parameter {key}")
        spec = PARAMS[key]
        if "choices" in spec and isinstance(v, str): v = choice_index(key, v)
        if isinstance(v, bool): v = 1 if v else 0
        v = max(spec["min"], min(spec["max"], v))
        if "choices" in spec or spec.get("int") or spec.get("bool"): v = int(round(v))
        elif isinstance(v, float): v = round(v, 5)
        self.p[key] = v

    def get(self, key, default=None):
        if key in self.p: return self.p[key]
        return PARAMS[key]["def"] if default is None else default

    def on(self, key): return self.get(key) not in (0, 0.0, None)

    # ------------------------------------------------------------------ sections
    def osc(self, i, **kw):
        pre = f"osc{OSC[i]}_"
        if "on" not in kw: kw["on"] = 1
        for k, v in kw.items(): self._put(pre + k, v)
        return self

    def shape(self, i, name, **kw):
        pos = {"sine": 0.0, "tri": 1 / 3, "saw": 2 / 3, "square": 1.0}[name]
        return self.osc(i, wt="Basic Shapes", pos=pos, **kw)

    def osc_off(self, i): self._put(f"osc{OSC[i]}_on", 0); return self

    def filt(self, i, **kw):
        if "on" not in kw: kw["on"] = 1
        for k, v in kw.items(): self._put(f"f{i + 1}_{k}", v)
        return self

    def env(self, i, **kw):
        for k, v in kw.items(): self._put(f"env{i + 1}_{k}", v)
        return self

    def lfo(self, i, **kw):
        if "div" in kw: kw.setdefault("sync", 1)
        if "rate" in kw and "div" not in kw: kw.setdefault("sync", 0)
        for k, v in kw.items(): self._put(f"lfo{i + 1}_{k}", v)
        return self

    def fx(self, name, **kw):
        self._put(f"fx_{name}_on", 1)
        for k, v in kw.items(): self._put(f"fx_{name}_{k}", v)
        return self

    def fx_off(self, name): self._put(f"fx_{name}_on", 0); return self
    def has_fx(self, name): return self.get(f"fx_{name}_on") == 1

    def sub(self, **kw):
        self._put("sub_on", 1)
        for k, v in kw.items(): self._put(f"sub_{k}", v)
        return self

    def noise(self, **kw):
        self._put("noise_on", 1)
        for k, v in kw.items(): self._put(f"noise_{k}", v)
        return self

    def sample(self, inst, osc_off=True, **kw):
        self._put("smp_on", 1)
        self._put("smp_inst", inst)
        for k, v in kw.items(): self._put(f"smp_{k}", v)
        if osc_off: self.osc_off(0)
        self.sample_inst = inst
        self.add_tags("Sampled")
        return self

    def analog(self, **kw):
        for k, v in kw.items(): self._put(k if k in ("drift", "spread") else f"an_{k}", v)
        return self

    def mod(self, src, dst, amt):
        if len(self.mods) >= 16: return self
        self.mods.append((choice_index("mod1_src", src), choice_index("mod1_dst", dst), round(float(amt), 4)))
        return self

    def focus(self, dst, depth, curve=0.0):
        self.focus_slots.append((dst, depth, curve)); return self

    def arpeggio(self, pattern=None, **kw):
        self._put("arp_on", 1)
        for k, v in kw.items(): self._put(f"arp_{k}", v)
        if pattern is not None:
            steps = []
            for s in pattern:
                if s is None: steps.append([0, 0.8, 1.0, 0, 0])
                elif isinstance(s, (int, float)): steps.append([int(s), 0.8, 1.0, 1, 0])
                else:
                    d, vel, gate = (list(s) + [0.8, 1.0])[:3]
                    tie = s[3] if len(s) > 3 else 0
                    steps.append([int(d), round(vel, 3), round(gate, 3), 1, int(tie)])
            while len(steps) < 32: steps.append([0, 0.8, 1.0, 1, 0])
            self.arp = steps[:32]
        return self

    def chord_gen(self, **kw):
        self._put("chord_on", 1)
        for k, v in kw.items(): self._put(f"chord_{k}", v)
        return self

    def add_tags(self, *t):
        for x in t:
            if x and x not in self.tags: self.tags.append(x)
        return self

    # ------------------------------------------------------------------ output
    def to_json(self, pid):
        params = dict(self.p)
        for n, (s, d, a) in enumerate(self.mods[:16]):
            params[f"mod{n + 1}_src"] = s; params[f"mod{n + 1}_dst"] = d; params[f"mod{n + 1}_amt"] = a
        for n, (dst, depth, curve) in enumerate(self.focus_slots[:8]):
            params[f"focus{n + 1}_dst"] = choice_index("focus1_dst", dst)
            params[f"focus{n + 1}_depth"] = round(depth, 3); params[f"focus{n + 1}_curve"] = round(curve, 3)
        out = {"id": pid, "name": self.name, "category": self.cat, "sub": self.subcat, "tags": self.tags[:6], "params": params}
        if self.arp: out["arp"] = self.arp
        if self.chords: out["chords"] = self.chords
        return out


# ============================================================================== small musical helpers
def rr(r, a, b, nd=3): return round(r.uniform(a, b), nd)
def pick(r, seq): return seq[r.randrange(len(seq))]


def reverb(p, r, kind="Hall", mix=(0.12, 0.25), size=(0.4, 0.8), decay=(1.5, 4.0), damp=(0.3, 0.6), pre=(5, 30)):
    p.fx("rev", type=kind, mix=rr(r, *mix), size=rr(r, *size, 2), decay=rr(r, *decay, 2), damp=rr(r, *damp, 2), pre=rr(r, *pre, 1))
    if kind == "Shimmer": p.set(fx_rev_shim=rr(r, 0.3, 0.7, 2))
    return p


def delay(p, r, div=None, mode=None, fb=(0.25, 0.45), mix=(0.1, 0.22), highcut=(4000, 9000)):
    p.fx("dly", sync=1, div=div or pick(r, ["1/8 D", "1/4", "1/8", "1/4 D", "1/16 D"]), mode=mode or pick(r, ["Digital", "Tape Echo", "Ping-Pong"]),
         fb=rr(r, *fb, 2), mix=rr(r, *mix), highcut=round(r.uniform(*highcut)), lowcut=pick(r, [150, 250, 400]))
    return p


def chorus(p, r, mix=(0.25, 0.5), depth=(0.3, 0.7), rate=(0.2, 1.2)):
    p.fx("cho", mix=rr(r, *mix, 2), depth=rr(r, *depth, 2), rate=rr(r, *rate, 2), delay=rr(r, 7, 18, 1))
    return p


def vibrato(p, r, lfo=3, rate=(4.8, 6.2), cents=(8, 18), delay=(0.2, 0.6), oscs=(0,)):
    p.lfo(lfo, shape="Sine", rate=rr(r, *rate, 2), delay=rr(r, *delay, 2), fade=rr(r, 0.3, 0.8, 2), mode="Retrig")
    c = rr(r, *cents, 1)
    for o in oscs: p.mod(f"LFO {lfo + 1}", f"Osc {OSC[o]} Fine", c * CENT)
    return p


def velocity_tone(p, amt=0.18, filt=0):
    return p.mod("Velocity", f"Filter {filt + 1} Cutoff", amt)


def macros(p, tone=True, timbre=None, extra=None):
    """M1 = tone, M2 = timbre, M3/M4 = whatever the recipe gives them."""
    if tone and p.on("f1_on"): p.mod("Macro 1", "Filter 1 Cutoff", 0.3)
    if timbre: p.mod("Macro 2", timbre, 0.35)
    for src, dst, amt in (extra or []): p.mod(src, dst, amt)
    return p


# ============================================================================== characters
# Each preset gets one character on top of its recipe: this is what makes two presets of the same
# family sound clearly different (and what the names describe).
CHARACTERS = ["clean", "vintage", "digital", "wide", "dark", "bright", "lofi", "fx"]

ADJ = {
    "clean": ["Pure", "Clear", "Polished", "Studio", "Classic", "Crisp", "Honest", "Natural", "Simple", "Pristine"],
    "vintage": ["Vintage", "Tape", "Analog", "Retro", "Warm", "Old School", "70s", "80s", "Dusty", "Worn"],
    "digital": ["Digital", "FM", "Chrome", "Hyper", "Neon", "Laser", "Pixel", "Crystal", "Modern", "Hi-Fi"],
    "wide": ["Wide", "Huge", "Panoramic", "Stereo", "Vast", "Massive", "Spread", "Open", "Super", "Floating"],
    "dark": ["Dark", "Deep", "Shadow", "Midnight", "Murky", "Smoky", "Heavy", "Low", "Velvet", "Gothic"],
    "bright": ["Bright", "Shiny", "Golden", "Sparkling", "Airy", "Glassy", "Silver", "Sunny", "Radiant", "Fresh"],
    "lofi": ["Lo-Fi", "Cassette", "Vinyl", "Broken", "Crushed", "Dirty", "Bedroom", "VHS", "Rusty", "Grainy"],
    "fx": ["Mangled", "Glitched", "Fractured", "Morphing", "Haunted", "Alien", "Warped", "Twisted", "Cursed", "Liquid"],
}


def apply_character(p, r, char, role="lead"):
    """role: 'bass', 'lead', 'pad', 'keys', 'perc', 'fx', 'real' (sampled realism: gentle touches only)."""
    gentle = role in ("real",)
    low = role == "bass"
    if char == "vintage":
        p.analog(age=rr(r, 0.15, 0.45 if not gentle else 0.25, 2), drift=rr(r, 0.1, 0.35, 2), tolerance=rr(r, 0.1, 0.4, 2))
        if r.random() < 0.6: p.fx("tape", wow=rr(r, 0.05, 0.25, 2), flutter=rr(r, 0.05, 0.2, 2), sat=rr(r, 0.2, 0.5, 2), tone=rr(r, 0.45, 0.7, 2), mix=1.0)
        if not low and r.random() < 0.5 and not p.has_fx("cho"): chorus(p, r, mix=(0.2, 0.4))
        p.add_tags("Vintage")
    elif char == "digital":
        if not gentle and r.random() < 0.45: p.fx("dist", type=pick(r, ["Bitcrush", "Downsample", "Hard Clip"]), drive=rr(r, 0.1, 0.35, 2), mix=rr(r, 0.15, 0.45, 2))
        p.fx("eq", highf=rr(r, 6000, 10000, 0), highg=rr(r, 1.5, 4.0, 1), lowf=120, lowg=rr(r, -2, 1, 1))
        if not p.has_fx("dly") and r.random() < 0.5 and not low: delay(p, r, mode="Digital", mix=(0.08, 0.18))
        p.add_tags("Digital")
    elif char == "wide":
        if not low:
            if not p.has_fx("cho"): chorus(p, r, mix=(0.3, 0.55), depth=(0.4, 0.8))
            if not p.has_fx("dly"): delay(p, r, mode="Ping-Pong", mix=(0.1, 0.2))
            if not p.has_fx("rev"): reverb(p, r, "Hall", mix=(0.15, 0.3))
        else:
            p.set(spread=rr(r, 0.3, 0.7, 2))
        p.set(spread=max(p.get("spread"), rr(r, 0.2, 0.5, 2)))
        p.add_tags("Wide")
    elif char == "dark":
        p.fx("eq", highf=rr(r, 3000, 6000, 0), highg=rr(r, -9, -4, 1), lowf=rr(r, 80, 200, 0), lowg=rr(r, 1, 3, 1))
        if not p.has_fx("rev") and not low: reverb(p, r, "Hall", mix=(0.12, 0.25), damp=(0.6, 0.85))
        p.add_tags("Dark")
    elif char == "bright":
        p.fx("eq", highf=rr(r, 5000, 9000, 0), highg=rr(r, 2.5, 6.0, 1), midf=rr(r, 250, 500, 0), midg=rr(r, -3, -1, 1))
        if not p.has_fx("rev") and not low: reverb(p, r, pick(r, ["Hall", "Room", "Shimmer"]) if not gentle else "Room", mix=(0.1, 0.22))
        p.add_tags("Bright")
    elif char == "lofi":
        p.analog(bits=rr(r, 0.2, 0.5 if not gentle else 0.3, 2), rate=rr(r, 0.1, 0.4, 2), noise=rr(r, 0.1, 0.35, 2), age=rr(r, 0.2, 0.5, 2))
        p.fx("tape", wow=rr(r, 0.25, 0.6, 2), flutter=rr(r, 0.15, 0.45, 2), sat=rr(r, 0.3, 0.6, 2), tone=rr(r, 0.3, 0.55, 2), mix=1.0)
        if r.random() < 0.5 and not p.get("noise_on"):   # never replace a noise the sound is made of
            p.noise(type=pick(r, ["Vinyl", "Tape Hiss", "Crackle"]), level=rr(r, 0.04, 0.12, 3), route="Direct")
        p.add_tags("Lo-Fi")
    elif char == "fx":
        choice = r.randrange(6)
        if choice == 0: p.fx("grn", size=rr(r, 40, 220, 0), density=rr(r, 8, 35, 1), spray=rr(r, 0.2, 0.7, 2), pitch=pick(r, [0, 12, -12, 7, 0]), prand=rr(r, 0.05, 0.4, 2), mix=rr(r, 0.25, 0.55, 2))
        elif choice == 1: p.fx("shf", mode=pick(r, ["Freq Shift", "Ring Mod"]), freq=rr(r, -300, 300, 1), fb=rr(r, 0.0, 0.5, 2), mix=rr(r, 0.2, 0.45, 2))
        elif choice == 2: p.fx("glt", rate=pick(r, ["1/16", "1/8", "1/32"]), chance=rr(r, 0.2, 0.5, 2), mix=rr(r, 0.6, 1.0, 2))
        elif choice == 3: p.fx("res", chord=pick(r, ["Fifth", "Minor", "Maj7", "Octaves", "Sus4"]), note=pick(r, [36, 43, 48, 55]), decay=rr(r, 0.5, 0.9, 2), bright=rr(r, 0.4, 0.8, 2), mix=rr(r, 0.2, 0.45, 2))
        elif choice == 4: p.set(chaos_amt=rr(r, 0.25, 0.55, 2), chaos_speed=rr(r, 0.3, 3.0, 2), chaos_bite=rr(r, 0.3, 0.8, 2), chaos_space=rr(r, 0.3, 0.8, 2), chaos_seed=r.randrange(1, 9999))
        else: p.set(motion_amt=rr(r, 0.35, 0.8, 2), motion_shape=pick(r, ["Filter Sweep", "Wobble", "Auto-Pan", "Stutter", "Random Steps", "Trance Gate"]), motion_rate=pick(r, ["1/8", "1/4", "1/16", "1/8 D", "1/2"]))
        if not p.has_fx("rev") and not low: reverb(p, r, pick(r, ["Hall", "Shimmer"]), mix=(0.15, 0.3))
        p.add_tags("FX")
    else:   # clean
        if not p.has_fx("rev") and role not in ("bass", "perc") and r.random() < 0.7: reverb(p, r, "Room" if r.random() < 0.6 else "Hall", mix=(0.08, 0.18))
        p.add_tags("Clean")
    return p


# ============================================================================== names
class Namer:
    def __init__(self):
        self.used = set()

    def make(self, r, nouns, char, extra=None):
        """'{adjective} {noun}' first, then other combinations; always unique."""
        adjs = list(ADJ.get(char, ADJ["clean"]))
        r.shuffle(adjs)
        nouns = list(nouns); r.shuffle(nouns)
        cands = []
        for n in nouns:
            for a in adjs:
                cands.append(f"{a} {n}")
        if extra:
            ex = list(extra); r.shuffle(ex)
            for n in nouns:
                for e in ex: cands.append(f"{n} {e}")
        for c in cands:
            if c.lower() not in self.used:
                self.used.add(c.lower()); return c
        k = 2
        while True:
            c = f"{nouns[0]} {k}"
            if c.lower() not in self.used: self.used.add(c.lower()); return c
            k += 1
