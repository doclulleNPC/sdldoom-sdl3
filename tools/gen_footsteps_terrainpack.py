#!/usr/bin/env python3
# Alternative footsteps.wad generator for a "generic terrain" footstep pack --
# i.e. a footsteps.pk3 laid out as  sound/footstep/<terrain>/<name>N.wav  with
# no SNDINFO/language.txt.  (The main gen_footsteps.py targets the zk-resources
# pack, which carries those text defs; it will not read a pack like this.)
#
# This does NOT regenerate footstep_tables.h / sounds.h -- it instead maps the
# pack's terrain folders onto the lump names the *committed* tables already
# expect (DSSTP1, DSWTR1, DSDIRT1, ...).  Terrains the pack lacks (carpet,
# metal_b, snow, tile_b, slimy) are filled from a close present terrain so every
# expected lump exists (no missing-lump fallback, no silence).
#
# Requires Python 3 and ffmpeg on PATH (WAV -> DMX 11025 Hz mono u8).
import os, re, sys, zipfile, subprocess, struct, tempfile, glob

ROOT = os.path.dirname(os.path.abspath(__file__))
PK3  = os.path.join(ROOT, "..", "run", "footsteps.pk3")
OUT  = os.path.join(ROOT, "..", "run", "footsteps.wad")
TBL  = os.path.join(ROOT, "..", "footstep_tables.h")

# terrain enum name (lower, minus TER_) -> source folder in the pack.
# The right-hand side is the pack folder used; where it differs from the
# terrain name, that terrain is being substituted (the pack lacks its own).
FOLDER = {
    "default":"default", "water":"water", "carpet":"default",
    "dirt":"dirt", "gravel":"gravel", "hard":"hard",
    "metal_a":"metal", "metal_b":"metal", "rock":"rock",
    "snow":"dirt", "tile_a":"tile", "tile_b":"tile",
    "wood":"wood", "slime":"slime", "slimy":"slime", "lava":"lava",
}

# --- parse the committed tables: terrain -> ordered list of sfx names ---
src = open(TBL).read()
terr = {}   # "default" -> ["stp1","stp2",...]
for m in re.finditer(r'static const int ter_(\w+)\[\] = \{([^}]*)\};', src):
    terr[m.group(1)] = re.findall(r'sfx_(\w+)', m.group(2))
if not terr:
    sys.exit("could not parse ter_*[] arrays from " + TBL)

# --- extract the pack, inventory its terrain folders ---
tmp = tempfile.mkdtemp()
zipfile.ZipFile(PK3).extractall(tmp)
base = os.path.join(tmp, "sound", "footstep")
folder_files = {}
for d in sorted(os.listdir(base)) if os.path.isdir(base) else []:
    fs = sorted(glob.glob(os.path.join(base, d, "*.wav")))
    if fs:
        folder_files[d] = fs
if not folder_files:
    sys.exit("no sound/footstep/<terrain>/*.wav found in " + PK3)

def to_dmx(path):
    raw = subprocess.run(
        ["ffmpeg","-v","error","-y","-i",path,"-ac","1","-ar","11025","-f","u8","-"],
        stdout=subprocess.PIPE, check=True).stdout
    return struct.pack("<HHI", 3, 11025, len(raw)) + raw

lumps = []
subbed = []
for terrain, sfxlist in terr.items():
    folder = FOLDER.get(terrain)
    files  = folder_files.get(folder)
    if not files:
        sys.exit(f"no source folder '{folder}' for terrain '{terrain}'")
    if folder != terrain and folder not in (terrain,):
        subbed.append(f"{terrain}<-{folder}")
    for i, sfx in enumerate(sfxlist):
        lumps.append(("DS"+sfx.upper(), to_dmx(files[i % len(files)])))

with open(OUT, "wb") as w:
    w.write(b"PWAD"); w.write(struct.pack("<i", len(lumps))); w.write(struct.pack("<i", 0))
    dirents = []; pos = 12
    for name, data in lumps:
        dirents.append((pos, len(data), name)); w.write(data); pos += len(data)
    info = pos
    for fp, sz, name in dirents:
        w.write(struct.pack("<ii", fp, sz)); w.write(name.encode("ascii")[:8].ljust(8, b"\0"))
    w.seek(8); w.write(struct.pack("<i", info))

print(f"Wrote {OUT}: {len(lumps)} lumps ({os.path.getsize(OUT)//1024} KB)")
if subbed:
    print("substituted terrains (pack lacked them):", ", ".join(subbed))
