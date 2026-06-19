@echo off
rem Connect to the Chocolate/Crispy-Doom server at 192.168.2.10 and play.
rem (Port defaults to 2342; doom2.wad etc. are found via the current dir.)
rem Extra args pass through, e.g.:
rem   connect.bat -netplayers 2     act as host/controller, wait for a 2nd player
rem   connect.bat -iwad doom1.wad   pick a specific IWAD (must match the server)
cd /d "%~dp0"
if exist sdldoom.exe (
    start "" sdldoom.exe -connect 192.168.2.10 %*
) else (
    start "" doom.exe -connect 192.168.2.10 %*
)
