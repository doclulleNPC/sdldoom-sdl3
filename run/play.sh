#!/bin/sh
# Launch SDL3 DOOM 2 from this folder (doom2.wad is found via the current dir).
# Extra args pass through, e.g.:  ./play.sh -warp 1   |   ./play.sh -render 4
cd "$(dirname "$0")"
exec ./doom-linux "$@"
