#!/usr/bin/env python3
# Pack the HD sprite PNGs (weapon + item/decoration packs) into a single WAD,
# one lump per sprite frame (lump name = uppercased PNG basename), raw PNG
# bytes.  hd_sprite.c reads this WAD directly and decodes via stb_image.
#
# Source packs (drop in run/; either alone is fine -- a missing one is skipped):
#   hd_weapons.pk3           HD weapon sprites
#   marcelus_hd_sprites.pk3  Marcelus HD item/decoration sprites, extracted from
#                            hdsprites9224.rar (see README -> Optional HD assets)
import os, zipfile, struct, sys

ROOT = os.path.dirname(os.path.abspath(__file__))
RUN  = os.path.join(ROOT, "..", "run")
SRC  = ["hd_weapons.pk3", "marcelus_hd_sprites.pk3"]
OUT  = os.path.join(RUN, "hdsprites.wad")

lumps = []          # (NAME, bytes)
seen = {}
for pk in SRC:
    path = os.path.join(RUN, pk)
    if not os.path.exists(path):
        print("  skip (not present):", pk, file=sys.stderr); continue
    with zipfile.ZipFile(path) as z:
        for info in z.infolist():
            n = info.filename
            if n.endswith("/"): continue
            if "hires/sprites/" not in n.lower(): continue
            if not n.lower().endswith(".png"): continue
            base = os.path.splitext(os.path.basename(n))[0].upper()
            if len(base) > 8:
                print("  skip (name >8):", n, file=sys.stderr); continue
            data = z.read(info)
            if base in seen:
                print("  dup lump", base, "(", pk, ") overrides", file=sys.stderr)
                lumps[seen[base]] = (base, data)
            else:
                seen[base] = len(lumps)
                lumps.append((base, data))
    print(f"  {pk}: total lumps now {len(lumps)}")

with open(OUT, "wb") as w:
    w.write(b"PWAD")
    w.write(struct.pack("<i", len(lumps)))
    w.write(struct.pack("<i", 0))
    dirents = []; pos = 12
    for name, data in lumps:
        dirents.append((pos, len(data), name)); w.write(data); pos += len(data)
    info = pos
    for fp, sz, name in dirents:
        w.write(struct.pack("<ii", fp, sz)); w.write(name.encode("ascii")[:8].ljust(8, b"\0"))
    w.seek(8); w.write(struct.pack("<i", info))

print(f"Wrote {OUT}: {len(lumps)} sprite lumps, {os.path.getsize(OUT)//1024} KB")
