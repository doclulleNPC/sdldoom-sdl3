#!/bin/sh
# Build script for SDLDoom (SDL3 port).
# Automatically detects Linux vs Windows (MinGW) and places built binaries and DLLs in run/.
set -e

RUN=${RUN:-run}
mkdir -p "$RUN"

if command -v windres >/dev/null 2>&1; then
    SDL3=${SDL3:-../SDL3}
    ARCH=${ARCH:-x64}

    windres sdldoom.rc -O coff -o sdldoom_res.o

    gcc -O2 -fno-strict-aliasing -fcommon -o sdldoom.exe *.c sdldoom_res.o \
        -I"$SDL3/include" \
        -DSDL_MAIN_HANDLED \
        "$SDL3/lib/$ARCH/SDL3.lib" \
        -lws2_32 \
        -lm

    if [ -f "$SDL3/lib/$ARCH/SDL3.dll" ]; then
        cp -f "$SDL3/lib/$ARCH/SDL3.dll" .
        cp -f "$SDL3/lib/$ARCH/SDL3.dll" "$RUN/"
    fi
    cp -f sdldoom.exe "$RUN/"
    echo "Built sdldoom.exe -> copied to $RUN/"
else
    # Linux / Unix build via pkg-config
    SDL_CFLAGS=$(pkg-config --cflags sdl3 2>/dev/null || echo "-I/usr/include/SDL3")
    SDL_LIBS=$(pkg-config --libs sdl3 2>/dev/null || echo "-lSDL3")

    gcc -O2 -fno-strict-aliasing -fcommon -o sdldoom *.c $SDL_CFLAGS $SDL_LIBS -lm

    cp -f sdldoom "$RUN/"
    if [ -f SDL3.dll ]; then
        cp -f SDL3.dll "$RUN/"
    fi
    echo "Built sdldoom -> copied to $RUN/"
fi
