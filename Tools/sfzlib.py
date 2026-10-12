"""Small helpers for building PYRA sample instruments from SFZ libraries and sample folders.

* parse_sfz(repo, sfz)  -> list of regions (dicts with the opcodes that matter, sample path relative to the repo)
* regions_from_names()  -> regions from file names such as "Vibes_soft_A2_v1_rr1_Main.wav"
* fetch(repo, paths)    -> makes sure the files exist in a blob-less git clone (downloads them in one go)
* decode(path)          -> float32 stereo numpy array at 44.1 kHz (any format ffmpeg reads)
"""
import os, re, subprocess
import numpy as np

SR = 44100
NOTE = {"c": 0, "d": 2, "e": 4, "f": 5, "g": 7, "a": 9, "b": 11}


def note_number(s, c4=60):
    """'c#4', 'Eb3', '60' -> MIDI note (C4 = 60 by default)."""
    s = str(s).strip()
    if re.fullmatch(r"-?\d+", s): return int(s)
    m = re.fullmatch(r"([A-Ga-g])([#b]?)(-?\d)", s)
    if not m: return None
    n = NOTE[m.group(1).lower()] + (1 if m.group(2) == "#" else -1 if m.group(2) == "b" else 0)
    return c4 + 12 * (int(m.group(3)) - 4) + n


# ----------------------------------------------------------------------------- SFZ
def _read(path, defines, seen, root=None):
    if not os.path.exists(path): return ""
    txt = open(path, encoding="utf-8", errors="ignore").read()
    txt = re.sub(r"/\*.*?\*/", "", txt, flags=re.S)
    out = []
    for line in txt.splitlines():
        line = line.split("//")[0]
        m = re.match(r'\s*#define\s+(\$\w+)\s+(.*)', line)
        if m: defines[m.group(1)] = m.group(2).strip(); continue
        for k in sorted(defines, key=len, reverse=True): line = line.replace(k, defines[k])
        m = re.match(r'\s*#include\s+"(.*)"', line)
        if m:
            inc = m.group(1).replace("\\", "/")
            cand = [os.path.join(root or os.path.dirname(path), inc), os.path.join(os.path.dirname(path), inc)]
            target = next((c for c in cand if os.path.exists(c)), cand[0])
            out.append(_read(target, defines, seen, root or os.path.dirname(path))); continue
        out.append(line)
    return "\n".join(out)


def parse_sfz(repo, sfz):
    """Flattens <control>/<global>/<master>/<group>/<region> into a list of regions."""
    defines = {}
    path = os.path.join(repo, sfz)
    text = _read(path, defines, set())
    for k in sorted(defines, key=len, reverse=True): text = text.replace(k, defines[k])
    tokens = re.split(r"(<\w+>)", text)
    level = {"control": {}, "global": {}, "master": {}, "group": {}}
    order = ["control", "global", "master", "group"]
    regions, cur, header = [], None, None
    def opcodes(chunk):
        d = {}
        parts = re.split(r"(?:^|\s)([A-Za-z_][\w]*)=", chunk)
        for i in range(1, len(parts) - 1, 2):
            key, val = parts[i], parts[i + 1].strip()
            if key != "sample": val = val.split()[0] if val.split() else ""
            d[key] = val
        return d
    for t in tokens:
        h = re.fullmatch(r"<(\w+)>", t)
        if h:
            if cur is not None: regions.append(cur); cur = None
            header = h.group(1)
            if header == "region":
                cur = {}
                for lv in order: cur.update(level[lv])
            elif header in level:
                for lv in order[order.index(header):]: level[lv] = {}   # a new header clears the levels below it
            continue
        ops = opcodes(t)
        if cur is not None: cur.update(ops)
        elif header in level: level[header].update(ops)
    if cur is not None: regions.append(cur)
    base = os.path.dirname(sfz)
    out = []
    for r in regions:
        if "sample" not in r: continue
        smp = r["sample"].replace("\\", "/").strip()
        if smp.startswith("*"): continue                     # *sine and friends
        dp = r.get("default_path", "").replace("\\", "/")
        rel = os.path.normpath(os.path.join(base, dp, smp))
        key = note_number(r["key"]) if "key" in r else None
        lo = note_number(r.get("lokey", key if key is not None else 0))
        hi = note_number(r.get("hikey", key if key is not None else 127))
        pk = note_number(r.get("pitch_keycenter", key if key is not None else lo))
        out.append({
            "sample": rel, "root": pk if pk is not None else lo, "lo": lo, "hi": hi,
            "lovel": int(float(r.get("lovel", 0))), "hivel": int(float(r.get("hivel", 127))),
            "seq": int(float(r.get("seq_position", 1))), "trigger": r.get("trigger", "attack"),
            "tune": float(r.get("tune", 0)) + 100 * float(r.get("transpose", 0)),
            "volume": float(r.get("volume", 0)), "offset": int(float(r.get("offset", 0))),
            "ops": r,
        })
    return out


# ----------------------------------------------------------------------------- file names
def regions_from_names(repo, folder, pattern=r".*\.(wav|flac|ogg)$", c4=60):
    """Regions from file names: note (A#3), velocity (v2 / vl2 / pp..ff) and round robin (rr1 / var2)."""
    files = subprocess.run(["git", "-C", repo, "ls-tree", "--name-only", "HEAD", folder.rstrip("/") + "/"],
                           capture_output=True, text=True).stdout.split("\n")
    dyn = {"ppp": 1, "pp": 2, "p": 3, "mp": 4, "mf": 5, "f": 6, "ff": 7, "fff": 8}
    out = []
    for f in files:
        if not f or not re.search(pattern, f, re.I): continue
        base = os.path.splitext(os.path.basename(f))[0]
        toks = re.split(r"[_\- ]", base)
        note = None; vel = 1; rr = 1
        for t in toks:
            n = note_number(t, c4) if re.fullmatch(r"[A-Ga-g][#b]?-?\d", t) else None
            if n is not None: note = n
            m = re.fullmatch(r"(?:v|vl|vel)(\d+)", t, re.I)
            if m: vel = int(m.group(1))
            elif t.lower() in dyn: vel = dyn[t.lower()]
            m = re.fullmatch(r"(?:rr|var)(\d+)", t, re.I)
            if m: rr = int(m.group(1))
        out.append({"sample": f, "root": note, "lo": note, "hi": note, "lovel": vel, "hivel": vel, "seq": rr,
                    "trigger": "attack", "tune": 0.0, "volume": 0.0, "offset": 0, "ops": {"name": base}})
    return out


# ----------------------------------------------------------------------------- git + audio
def fetch(repo, paths):
    missing = [p for p in paths if not os.path.exists(os.path.join(repo, p))]
    for i in range(0, len(missing), 200):
        subprocess.run(["git", "-C", repo, "checkout", "HEAD", "--"] + missing[i:i + 200],
                       check=False, capture_output=True)
    # sparse checkouts ignore paths outside their cone: read those straight from the object store
    for p in [p for p in paths if not os.path.exists(os.path.join(repo, p))]:
        r = subprocess.run(["git", "-C", repo, "cat-file", "-p", "HEAD:" + p], capture_output=True)
        if r.returncode == 0 and r.stdout:
            os.makedirs(os.path.dirname(os.path.join(repo, p)), exist_ok=True)
            open(os.path.join(repo, p), "wb").write(r.stdout)
    return [p for p in paths if not os.path.exists(os.path.join(repo, p))]


def decode(path, channels=2):
    """Any audio file -> float32 (frames, channels) at 44.1 kHz."""
    r = subprocess.run(["ffmpeg", "-v", "error", "-i", path, "-ar", str(SR), "-ac", str(channels), "-f", "f32le", "-"],
                       capture_output=True)
    x = np.frombuffer(r.stdout, dtype=np.float32)
    return x.reshape(-1, channels).copy()
