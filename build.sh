#!/bin/sh
# Build SDLDoom (SDL3 port) with MinGW-w64 gcc on Windows.
#
# Adjust SDL3 to wherever the SDL3 SDK lives (headers in $SDL3/include/SDL3,
# import lib + DLL in $SDL3/lib/<arch>).
set -e

SDL3=${SDL3:-../SDL3}
ARCH=${ARCH:-x64}

gcc -O2 -o doom.exe *.c \
    -I"$SDL3/include" \
    -DSDL_MAIN_HANDLED \
    "$SDL3/lib/$ARCH/SDL3.lib" \
    -lm

# Put the runtime DLL next to the executable.
cp -f "$SDL3/lib/$ARCH/SDL3.dll" .

echo "Built doom.exe.  Run with e.g.:"
echo "  DOOMWADDIR=/path/to/wad ./doom.exe -warp 1"
