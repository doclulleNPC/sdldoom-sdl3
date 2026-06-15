SDL DOOM! v1.10 (SDL3 port, 64-bit Windows)
===========================================

Ready to play.  Just run:

    play.bat            (or double-click doom.exe)

doom.exe loads doom2.wad from this folder automatically.

The window opens at 640x480 (640x400 @ 4:3) and is resizable - drag a corner,
or pick a higher resolution (which makes a bigger window), or -fullscreen.

Internal resolution and aspect ratio:
    The 3D view renders at a true higher internal resolution (default 640x400),
    not just an upscaled 320x200.  Change it in-game via:
        ESC -> Options -> Video
    There you can set Resolution (320x200, 640x400, 960x600, 1280x800) and
    Aspect (4:3, 16:10, 16:9, Stretch).  Left/Right (or Enter) change a value.
    Changing the Resolution also resizes the window to match (capped to your
    screen); Aspect sets the window shape.

Useful command-line options (edit play.bat or run from a terminal):
    doom.exe -warp 1            start directly on MAP01
    doom.exe -skill 4           Ultra-Violence
    doom.exe -render 3          start resolution/window scale (1..4); default 2
    doom.exe -1 .. -4           same as -render (1=320x200 .. 4=1280x800)
    doom.exe -aspect 2          start aspect (0=4:3 1=16:10 2=16:9 3=stretch)
    doom.exe -fullscreen        fullscreen
    doom.exe -nomonsters        no monsters
    doom.exe -file mymod.wad    load a PWAD

Default controls: arrows move/turn, Ctrl fire, Space use/open, Alt strafe,
Shift run, Esc menu, number keys select weapons, Tab automap.

Files:
    doom.exe    the game
    SDL3.dll    required runtime library (keep next to doom.exe)
    doom2.wad   DOOM II game data
