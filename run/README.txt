SDL DOOM! v1.10 (SDL3 port, 64-bit Windows)
===========================================

Ready to play.  Just run:

    play.bat            (or double-click doom.exe)

doom.exe loads doom2.wad from this folder automatically.

The window opens at 3x size (960x720, 4:3) and is resizable - just drag a
corner, or use -1 .. -5 / -fullscreen below.

Internal resolution and aspect ratio:
    The 3D view renders at a true higher internal resolution (default 640x400),
    not just an upscaled 320x200.  Change it in-game via:
        ESC -> Options -> Video
    There you can set Resolution (320x200, 640x400, 960x600, 1280x800) and
    Aspect (4:3, 16:10, 16:9, Stretch).  Left/Right (or Enter) change a value.

Useful command-line options (edit play.bat or run from a terminal):
    doom.exe -warp 1            start directly on MAP01
    doom.exe -skill 4           Ultra-Violence
    doom.exe -render 3          internal resolution scale (1..4); default 2
    doom.exe -1 .. -5           window scale (1x..5x); default is 3x
    doom.exe -fullscreen        fullscreen
    doom.exe -nomonsters        no monsters
    doom.exe -file mymod.wad    load a PWAD

Default controls: arrows move/turn, Ctrl fire, Space use/open, Alt strafe,
Shift run, Esc menu, number keys select weapons, Tab automap.

Files:
    doom.exe    the game
    SDL3.dll    required runtime library (keep next to doom.exe)
    doom2.wad   DOOM II game data
