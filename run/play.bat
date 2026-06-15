@echo off
rem Launch SDL3 DOOM 2 from this folder (doom2.wad is found via the current dir).
cd /d "%~dp0"
start "" doom.exe %*
