#!/usr/bin/env python3
# Pack the item/decoration/weapon KVX voxel models from VoxelDoom (a GZDoom
# pk3) into a single PWAD that hd_voxel.c reads directly (NOT through w_wad).
#
# See tools/VOXELS.md for the full pipeline description.
#
# Voxel models are from "Voxel Doom" by Daniel Peterson ("cheello") (modelling)
# and Nash Muhandes (ZScript).  Info + downloads:
#   https://doomwiki.org/wiki/Voxel_Doom
#   https://www.moddb.com/mods/doom-voxel-project   (Voxel Doom)
#   https://www.moddb.com/mods/voxel-doom-ii         (Voxel Doom II)
# VoxelDoom_v2.4.pk3 is NOT shipped here; download it and put it in run/.
#
# Output: run/voxels.wad
#   - one lump per .kvx model, lump name = uppercased basename (<=8 chars),
#     raw KVX bytes (hd_voxel.c decodes mip 0 + the trailing 768-byte palette).
#   - one "VOXELDEF" lump holding the pk3's VOXELDEF text (sprite-frame ->
#     kvx-name mapping + AngleOffset/scale), parsed at runtime by hd_voxel.c.
#
# We pack only the root "voxels/" tree (items, ammo, weapons, decorations,
# barrels) for the first pass; the per-game "filter/.../voxels/" monster models
# are skipped (they can be added later by widening SRC_PREFIXES).
import os, zipfile, struct, sys

ROOT = os.path.dirname(os.path.abspath(__file__))
RUN  = os.path.join(ROOT, "..", "run")
SRC  = os.path.join(RUN, "VoxelDoom_v2.4.pk3")
OUT  = os.path.join(RUN, "voxels.wad")

SRC_PREFIXES = ("voxels/",)            # root item/decoration/weapon models
VOXELDEF_NAME = "VOXELDEF.txt"         # root mapping (DOOM2-relevant)

lumps = []          # (NAME, bytes)
seen  = {}

with zipfile.ZipFile(SRC) as z:
    names = z.namelist()

    # the mapping lump first, so hd_voxel.c can find it by name
    if VOXELDEF_NAME in names:
        lumps.append(("VOXELDEF", z.read(VOXELDEF_NAME)))
        seen["VOXELDEF"] = 0
    else:
        print("WARNING: no", VOXELDEF_NAME, "in pk3", file=sys.stderr)

    for n in names:
        ln = n.lower()
        if not ln.endswith(".kvx"):
            continue
        if not any(ln.startswith(p) for p in SRC_PREFIXES):
            continue
        base = os.path.splitext(os.path.basename(n))[0].upper()
        if len(base) > 8:
            print("  skip (name >8):", n, file=sys.stderr); continue
        if base in seen:
            print("  dup lump", base, "- keeping first", file=sys.stderr); continue
        seen[base] = len(lumps)
        lumps.append((base, z.read(n)))

with open(OUT, "wb") as w:
    w.write(b"PWAD")
    w.write(struct.pack("<i", len(lumps)))
    w.write(struct.pack("<i", 0))           # infotableofs patched below
    dirents = []; pos = 12
    for name, data in lumps:
        dirents.append((pos, len(data), name)); w.write(data); pos += len(data)
    info = pos
    for fp, sz, name in dirents:
        w.write(struct.pack("<ii", fp, sz)); w.write(name.encode("ascii")[:8].ljust(8, b"\0"))
    w.seek(8); w.write(struct.pack("<i", info))

print(f"Wrote {OUT}: {len(lumps)} lumps ({len(lumps)-1} voxels + VOXELDEF), "
      f"{os.path.getsize(OUT)//1024} KB")
