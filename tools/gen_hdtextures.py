#!/usr/bin/env python3
# Pack the DOOM2-relevant HD wall/flat textures from the DHTP pack (dhtp.pk3)
# into a WAD (one PNG lump per texture/flat name).  Uses the GZDoom filter
# precedence: filter/doom (common) then filter/doom.doom2 (doom2 overrides).
#
# DHTP source: https://github.com/KuriKai/DHTP  (downloads: its wiki).
# Expects the GZDoom/ZDoom-format build (PNGs under filter/doom/hires/).  That
# download is named e.g. zdoom-dhtp-20180514.pk3; we accept any zdoom-dhtp-*.pk3
# in run/, or a plain dhtp.pk3.
import os, zipfile, struct, sys, glob

ROOT = os.path.dirname(os.path.abspath(__file__))
RUN  = os.path.join(ROOT, "..", "run")
_cands = sorted(glob.glob(os.path.join(RUN, "zdoom-dhtp-*.pk3")), reverse=True) \
       + [os.path.join(RUN, "dhtp.pk3")]
SRC  = next((p for p in _cands if os.path.exists(p)), _cands[-1])
OUT  = os.path.join(RUN, "hdtextures.wad")
print("source:", os.path.basename(SRC))

# later filters override earlier ones for the same texture name
FILTERS = ["filter/doom/hires/", "filter/doom.doom2/hires/"]

order = []          # preserve first-seen order
data_by_name = {}
with zipfile.ZipFile(SRC) as z:
    names = z.namelist()
    for filt in FILTERS:
        for n in names:
            ln = n.lower()
            if not ln.startswith(filt): continue
            if not ln.endswith(".png"): continue
            base = os.path.splitext(os.path.basename(n))[0].upper()
            if len(base) > 8:
                print("  skip (>8):", n, file=sys.stderr); continue
            if base not in data_by_name:
                order.append(base)
            data_by_name[base] = z.read(n)     # later filter overrides
        print(f"  after {filt}: {len(order)} names")

with open(OUT, "wb") as w:
    w.write(b"PWAD")
    w.write(struct.pack("<i", len(order)))
    w.write(struct.pack("<i", 0))
    dirents = []; pos = 12
    for name in order:
        d = data_by_name[name]
        dirents.append((pos, len(d), name)); w.write(d); pos += len(d)
    info = pos
    for fp, sz, name in dirents:
        w.write(struct.pack("<ii", fp, sz)); w.write(name.encode("ascii")[:8].ljust(8, b"\0"))
    w.seek(8); w.write(struct.pack("<i", info))

print(f"Wrote {OUT}: {len(order)} texture/flat lumps, {os.path.getsize(OUT)//(1024*1024)} MB")
