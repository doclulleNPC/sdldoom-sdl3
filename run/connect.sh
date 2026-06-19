#!/bin/sh
# Connect to the Chocolate/Crispy-Doom server at 192.168.2.10 and play.
# (Port defaults to 2342; doom2.wad etc. are found via the current dir.)
#
# Extra args pass through, e.g.:
#   ./connect.sh -netplayers 2     # act as host/controller, wait for a 2nd player
#   ./connect.sh -iwad doom1.wad   # pick a specific IWAD (must match the server)
cd "$(dirname "$0")"
exec ./doom-linux -connect 192.168.2.10 "$@"
