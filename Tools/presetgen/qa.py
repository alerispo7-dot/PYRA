#!/usr/bin/env python3
"""Renders the whole bank, levels it, and replaces presets that sound like another one of their sub-category.

    python3 Tools/presetgen/qa.py <old FactoryPresets.json> <work dir> <out FactoryPresets.json> [rounds]

Uses `PyraRenderTest --normalize` (loudness + a 12 x 16 band spectral fingerprint of every preset).
New presets that are too close to a sibling are rebuilt with another seed; the earlier presets get a new
character (FX, analog, tone) and a nudge of their main sound so that they stand apart.
"""
import json, os, random, subprocess, sys
import numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build
from core import P, Namer, apply_character, CHARACTERS, PARAMS, choice_index, rr, pick, reverb
from registry import ROLE

RT = os.path.join(os.path.dirname(__file__), "..", "..", "build-linux", "PyraRenderTest_artefacts", "Release", "PyraRenderTest")


def render(bank, work):
    raw = os.path.join(work, "bank_in.json"); out = os.path.join(work, "bank_out.json")
    json.dump(bank, open(raw, "w"))
    subprocess.run([os.path.abspath(RT), "--normalize", raw, out], cwd=work, check=False)
    return json.load(open(out)), json.load(open(os.path.join(work, "features.json"))), open(os.path.join(work, "normalize_report.txt")).read()


def fingerprint(f):
    a = np.array(f["fp"], dtype=np.float32).reshape(12, 16)
    a = np.maximum(a, a.max() - 60)          # ignore what is 60 dB under the loudest band
    return a - a.mean(), float(f["width"])


def distance(fa, fb):
    (a, wa), (b, wb) = fa, fb
    return float(np.sqrt(np.mean((a - b) ** 2)) + 6.0 * abs(min(wa, 1.5) - min(wb, 1.5)))


def limit_for(cat, sub, base):
    """Some families live in a narrow sound space (a sub is mostly a sine, a kick is a kick): there only real twins count."""
    if cat == "Bass" and sub in ("Sub Bass", "808"): return base * 0.55
    if cat == "Drums": return base * 0.65
    if cat == "Bass" or (cat, sub) in (("Keyboard", "Acoustic Piano"), ("Keyboard", "Grand Piano")): return base * 0.8
    return base


def twins(bank, feats, limit):
    """-> indices of presets that sound like an earlier preset of their sub-category."""
    by_sub = {}
    for k, f in enumerate(feats): by_sub.setdefault((bank[k]["category"], bank[k]["sub"]), []).append(k)
    fps = [fingerprint(f) for f in feats]
    flagged, pairs = set(), []
    for key, idx in by_sub.items():
        lim = limit_for(key[0], key[1], limit)
        for x in range(len(idx)):
            for y in range(x):
                d = distance(fps[idx[x]], fps[idx[y]])
                if d < lim:
                    pairs.append((d, bank[idx[y]]["name"], bank[idx[x]]["name"]))
                    flagged.add(idx[x])
    return flagged, sorted(pairs)


def as_builder(pr):
    p = P(pr["name"], pr["category"], pr["sub"], pr.get("tags", []))
    prm = dict(pr["params"])
    for n in range(1, 17):
        s, d, a = prm.pop(f"mod{n}_src", None), prm.pop(f"mod{n}_dst", None), prm.pop(f"mod{n}_amt", None)
        if s is not None and d is not None: p.mods.append((int(s), int(d), float(a or 0)))
    p.p = {k: v for k, v in prm.items() if k in PARAMS and not k.startswith("focus")}
    p.focus_raw = {k: v for k, v in prm.items() if k.startswith("focus")}
    p.arp, p.chords = pr.get("arp"), pr.get("chords")
    return p


def _scale(p, key, lo, hi, mn, mx, nd=3):
    if key in p.p: p.set(**{key: round(min(mx, max(mn, float(p.p[key]) * random_uniform(lo, hi))), nd)})


def random_uniform(a, b):
    return _R.uniform(a, b)


_R = random.Random(0)


def vary_drum(p, r):
    """Kicks, snares, hats...: another tuning, length, punch and grit (no pads of reverb on a kick)."""
    global _R; _R = r
    p.set(transpose=int(p.p.get("transpose", 0)) + pick(r, [-5, -3, -2, 2, 3, 5]))
    _scale(p, "env1_dec", 0.45, 1.7, 0.01, 6.0)
    for n in range(1, 17):
        if p.mods and n <= len(p.mods):
            s_, d_, a_ = p.mods[n - 1]
            if d_ in (choice_index("mod1_dst", "Osc A Pitch"), choice_index("mod1_dst", "Osc B Pitch")):
                p.mods[n - 1] = (s_, d_, round(max(-1.0, min(1.0, a_ * r.uniform(0.6, 1.6))), 4))
    p.fx("dist", type=pick(r, ["Soft Clip", "Tube", "Hard Clip", "Fold", "Bitcrush"]), drive=rr(r, 0.15, 0.55, 2), mix=rr(r, 0.6, 1.0, 2))
    p.fx("eq", lowf=rr(r, 60, 120, 0), lowg=rr(r, -3, 4, 1), highf=rr(r, 3000, 9000, 0), highg=rr(r, -6, 5, 1))
    if r.random() < 0.4 and not p.has_fx("rev"): reverb(p, r, "Room", mix=(0.06, 0.14), size=(0.2, 0.4))


def vary_bass(p, r):
    """Subs and 808s: another harmonic colour, drive, length and glide; the low end stays dry and mono."""
    global _R; _R = r
    what = r.randrange(4)
    if what == 0 and not p.p.get("oscB_on"):
        p.osc(1, wt=pick(r, ["Triangle", "Square", "Saw", "Half Sine", "Warm Square"]), oct=pick(r, [0, 1]), level=rr(r, 0.1, 0.3, 2))
    elif what == 1:
        p.fx("dist", type=pick(r, ["Tube", "Soft Clip", "Fold", "Hard Clip", "Diode", "Overdrive"]), drive=rr(r, 0.2, 0.6, 2), tone=rr(r, 0.3, 0.7, 2), mix=1.0)
    elif what == 2:
        p.fx("dist", type=pick(r, ["Bitcrush", "Downsample"]), drive=rr(r, 0.1, 0.35, 2), mix=rr(r, 0.3, 0.6, 2))
        p.add_tags("Lo-Fi")
    else:
        p.filt(0, type=pick(r, ["LP Ladder", "LP 24"]), cutoff=rr(r, 250, 900, 0), res=rr(r, 0.2, 0.45, 2), on=1)
        p.env(1, att=0.0, dec=rr(r, 0.08, 0.3, 2), sus=0.0); p.set(f1_env=rr(r, 0.2, 0.5, 2))
    _scale(p, "env1_dec", 0.5, 1.6, 0.05, 8.0)
    _scale(p, "glide", 0.4, 2.2, 0.0, 0.5)
    for o in "AB":
        if p.p.get(f"osc{o}_on"): p.set(**{f"osc{o}_pos": round(r.uniform(0.0, 1.0), 3)})


def refresh_old(pr, r):
    """Gives an earlier preset a clearly different colour, keeping its name and its idea."""
    p = as_builder(pr)
    role = ROLE.get((pr["category"], pr["sub"]), "lead")
    if pr["category"] == "Drums" and pr["sub"] not in ("Drum Loop", "Percs Loop"):
        vary_drum(p, r); return _finish(p, pr)
    if pr["category"] == "Bass" and pr["sub"] in ("Sub Bass", "808"):
        vary_bass(p, r); return _finish(p, pr)
    used = set(t.lower() for t in p.tags)
    allowed = build.CHARS.get((pr["category"], pr["sub"]), CHARACTERS)
    chars = [c for c in allowed if c not in used and c != "clean"] or [c for c in CHARACTERS if c not in used and c != "clean"]
    apply_character(p, r, pick(r, chars), ROLE.get((pr["category"], pr["sub"]), "lead"))
    for o in "ABC":   # move the wavetable position and the tone a little
        if p.p.get(f"osc{o}_on"): p.set(**{f"osc{o}_pos": round(min(1.0, max(0.0, float(p.p.get(f"osc{o}_pos", 0.5)) + r.uniform(-0.3, 0.3))), 3)})
    if p.p.get("f1_cutoff"): p.set(f1_cutoff=round(min(20000.0, float(p.p["f1_cutoff"]) * r.uniform(0.6, 1.6))))
    return _finish(p, pr)


def _finish(p, pr):
    out = p.to_json(pr["id"])
    out["params"].update(p.focus_raw)
    if pr.get("arp"): out["arp"] = pr["arp"]
    if pr.get("chords"): out["chords"] = pr["chords"]
    return out


def rescue_level(pr, line):
    """The normaliser could not reach the target with the master alone: add the missing gain inside the preset."""
    import re
    m = re.search(r"rms\s+(-?[\d.]+)\s+peak\s+(-?[\d.]+)\s+master\s+(-?[\d.]+)", line)
    if not m: return False
    rms, peak, old = map(float, m.groups())
    target = -14.0 if pr["category"] == "Drums" else -16.0
    want = old + min(target - rms, -1.5 - peak)
    prm = pr["params"]
    if want <= 24.0: return False
    need = want - 22.0
    if prm.get("fx_comp_on"):
        room = 24.0 - float(prm.get("fx_comp_makeup", 0))
        prm["fx_comp_makeup"] = round(min(24.0, float(prm.get("fx_comp_makeup", 0)) + need), 1)
        need -= room
    else:
        prm.update(fx_comp_on=1, fx_comp_thresh=0.0, fx_comp_ratio=1.0, fx_comp_makeup=round(min(24.0, need), 1), fx_comp_mix=1.0)
        need -= 24.0
    if need > 0:
        for k in ("smp_level", "noise_level", "oscA_level", "oscB_level", "oscC_level"):
            if k in prm and float(prm[k]) > 0: prm[k] = 1.0
    return True


def browse_key(p):
    """Inside a sub-category: realistic first, then hybrids and pure synthesis; simple starters before the
    heavily modulated / effected ones."""
    prm = p["params"]
    osc_on = any(prm.get(f"osc{o}_on", 1 if o == "A" else 0) for o in "ABC")
    sampled = bool(prm.get("smp_on"))
    tier = 0 if sampled and not osc_on else 1 if sampled else 2
    fx = sum(1 for k, v in prm.items() if k.startswith("fx_") and k.endswith("_on") and v)
    mods = sum(1 for n in range(1, 17) if prm.get(f"mod{n}_src"))
    extra = sum(2 for k in ("chaos_amt", "motion_amt", "focus_amt") if float(prm.get(k, 0)) > 0) + (2 if prm.get("arp_on") else 0) \
          + (1 if any(prm.get(k) for k in ("an_age", "an_bits", "an_noise")) else 0) + (2 if "FX" in p.get("tags", []) else 0)
    return (tier, fx + 0.5 * mods + extra, p["name"])


def main(old_path, work, out_path, rounds=3):
    os.makedirs(work, exist_ok=True)
    old = json.load(open(old_path))
    namer = Namer()
    old2 = build.remap_old(old, namer)
    new = build.build_new(namer)
    bank = new + old2
    salt, level_fix = {}, {}
    for rnd in range(rounds):
        normed, feats, report = render(bank, work)
        flagged, pairs = twins(normed, feats, limit=2.2)
        problems = [l for l in report.split("\n") if any(w in l for w in ("SILENT", "NAN", "CLAMPED", " DC"))]
        print(f"round {rnd + 1}: {len(flagged)} presets too close to a sibling, {len(problems)} with level problems", flush=True)
        for d, a, b in pairs[:25]: print(f"   {d:5.2f}  {a}  ~  {b}")
        for l in problems[:40]: print("   !", l)
        if not flagged and not problems: break
        if rnd == rounds - 1: break
        new_ids = {p["id"]: k for k, p in enumerate(new)}
        rescued = set()
        for l in problems:
            k = int(l.split()[0])
            if "CLAMPED" in l and not any(w in l for w in ("SILENT", "NAN", " DC")):
                pid = normed[k]["id"]
                src = new[new_ids[pid]] if pid in new_ids else next(q for q in old2 if q["id"] == pid)
                if rescue_level(src, l): rescued.add(k); level_fix[pid] = (salt.get(pid, 0), dict(src["params"]))
        bad = (set(flagged) | {int(l.split()[0]) for l in problems}) - rescued
        for k in sorted(bad):
            pid = normed[k]["id"]
            r = random.Random(hash((pid, rnd)) & 0xffffffff)
            if pid in new_ids:
                salt[pid] = salt.get(pid, 0) + 1
            else:
                j = next(n for n, q in enumerate(old2) if q["id"] == pid)
                old2[j] = refresh_old(old2[j], r)
        # rebuild the new presets that need another take (others stay identical: same seeds)
        if salt:
            namer = Namer()
            for q in old2: namer.used.add(q["name"].lower())
            new = build_new_salted(namer, salt)
        for q in new:
            if q["id"] in level_fix and level_fix[q["id"]][0] == salt.get(q["id"], 0): q["params"] = dict(level_fix[q["id"]][1])
        bank = new + old2
    order = {m: n for n, (m, _) in enumerate(build.LAYOUT)}
    subs = {(m, s): n for m, ss in build.LAYOUT for n, (s, _) in enumerate(ss)}
    final = sorted(normed, key=lambda p: (order.get(p["category"], 99), subs.get((p["category"], p["sub"]), 99), browse_key(p)))
    json.dump(final, open(out_path, "w"), indent=0)
    print(f"wrote {len(final)} presets")


def build_new_salted(namer, salt):
    """Same as build.build_new, but presets listed in `salt` get another seed."""
    from core import P as _P
    bank = []
    for macro, subs in build.LAYOUT:
        for sub, count in subs:
            arches = build.ARCH[(macro, sub)]
            role = build.ROLE.get((macro, sub), "lead")
            chars = build.CHARS.get((macro, sub), CHARACTERS)
            for i in range(count):
                pid = f"factory:v9/{macro.lower()}/{sub.lower().replace(' ', '-')}/{i + 1:02d}"
                s = salt.get(pid, 0)
                a = arches[(i + s) % len(arches)]
                k = i // len(arches)
                char = chars[(k * 3 + arches.index(a) + s * 5) % len(chars)]
                r = random.Random(build.seed_of(macro, sub, i, s or ""))
                p = _P("", macro, sub)
                a["build"](p, r, k)
                apply_character(p, r, char, role)
                p.name = namer.make(r, a["nouns"], char, a["extra"])
                bank.append(p.to_json(pid))
    return bank


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]) if len(sys.argv) > 4 else 3)
