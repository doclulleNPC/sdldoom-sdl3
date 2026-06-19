#!/bin/sh
# Build SDLDoom (SDL3 port) with MinGW-w64 gcc on Windows.
#
# Adjust SDL3 to wherever the SDL3 SDK lives (headers in $SDL3/include/SDL3,
# import lib + DLL in $SDL3/lib/<arch>).
set -e

SDL3=${SDL3:-../SDL3}
ARCH=${ARCH:-x64}

# Compile the Windows resource (exe icon).  See sdldoom.rc / tools/gen_icon.py.
windres sdldoom.rc -O coff -o sdldoom_res.o

# -fno-strict-aliasing is MANDATORY: the engine type-puns constantly (e.g.
# *(int*)lumpinfo[l].name to compare 4 chars), which gcc -O2 miscompiles under
# default strict aliasing (classic symptom: "Sprite TROO frame A is missing
# rotations" at startup).  -fcommon for the era's tentative-definition globals.
gcc -O2 -fno-strict-aliasing -fcommon -o sdldoom.exe *.c sdldoom_res.o \
    -I"$SDL3/include" \
    -DSDL_MAIN_HANDLED \
    "$SDL3/lib/$ARCH/SDL3.lib" \
    -lws2_32 \
    -lm

# Put the runtime DLL next to the executable.
cp -f "$SDL3/lib/$ARCH/SDL3.dll" .

# Drop fresh binaries into the run/ folder (where the IWADs/PWADs live).
RUN=${RUN:-run}
mkdir -p "$RUN"
cp -f sdldoom.exe "$RUN/"
cp -f "$SDL3/lib/$ARCH/SDL3.dll" "$RUN/"

echo "Built sdldoom.exe (also copied to $RUN/).  Run with e.g.:"
echo "  cd $RUN && ./sdldoom.exe -warp 1"
