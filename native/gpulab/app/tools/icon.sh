#!/usr/bin/env bash
# Renders source/icon0.png, which build.sh packages as sce_sys/icon0.png.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$here/build/host"
clang -std=c11 -O2 -Wall -Wextra -Werror -I "$here/source" "$here/tools/icon.c" "$here/source/ui_screen.c" \
  "$here/source/ui_canvas.c" "$here/source/ui_font.c" -o "$here/build/host/icon"
"$here/build/host/icon" "$here/build/host/icon0.ppm"
python3 -I "$here/tools/ppm_to_png.py" "$here/build/host/icon0.ppm"
mv "$here/build/host/icon0.png" "$here/source/icon0.png"
rm -f "$here/build/host/icon0.ppm"
