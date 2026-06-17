# Voxels: building `run/voxels.wad`

The engine's voxel renderer (`hd_voxel.c`) does **not** read GZDoom pk3s. It
reads a small purpose-built PWAD, `run/voxels.wad`, that is generated from the
**Voxel Doom** pk3 by `tools/gen_voxels.py`.

## How `voxels.wad` is created

Source: `run/VoxelDoom_v2.4.pk3` (a GZDoom mod — see Credits below).

```sh
python tools/gen_voxels.py        # reads run/VoxelDoom_v2.4.pk3 -> writes run/voxels.wad
```

What the script does:

- Opens the pk3 (a zip) and copies the `VOXELDEF.txt` text lump verbatim into a
  `VOXELDEF` lump. This is the sprite-frame → KVX-model mapping (plus
  `AngleOffset` / scale) that `hd_voxel.c` parses at runtime.
- Packs every `.kvx` model under the pk3's root `voxels/` tree (items, ammo,
  weapons, decorations, barrels) as one lump each. The lump name is the
  uppercased file basename (must be ≤ 8 chars; longer names are skipped with a
  warning, duplicates keep the first).
- Writes a minimal `PWAD` (12-byte header + lump data + directory). This is a
  raw container read **directly** by `hd_voxel.c`, not through `w_wad`.

Scope (first pass): only the root `voxels/` models are packed. The per-game
`filter/.../voxels/` **monster** models are intentionally skipped — widen
`SRC_PREFIXES` in the script to include them later.

Each KVX lump is raw bytes; `hd_voxel.c` decodes mip 0 plus the trailing
768-byte palette on demand.

## Credits

Voxel models are from **Voxel Doom**, created by **Daniel Peterson ("cheello")**,
who hand-modelled every voxel in MagicaVoxel. ZScript/GZDoom integration by
**Nash Muhandes**. All credit for the artwork belongs to them — this project
only repacks their `.kvx` assets into a format its own renderer can read.

- Doom Wiki: <https://doomwiki.org/wiki/Voxel_Doom>
- Voxel Doom (Doom 1) on ModDB: <https://www.moddb.com/mods/doom-voxel-project>
- Voxel Doom II (Doom 2) on ModDB: <https://www.moddb.com/mods/voxel-doom-ii>

The `VoxelDoom_v2.4.pk3` asset itself is **not** distributed in this repo;
download it from the ModDB pages above and drop it into `run/` before running
`gen_voxels.py`.
