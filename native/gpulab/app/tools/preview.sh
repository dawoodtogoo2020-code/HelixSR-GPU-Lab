#!/usr/bin/env bash
# Host rendering of candidate UI, not console evidence. Output must be an absolute path.
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
out=${1:?Provide an absolute output directory}
[[ $out == /* ]] || { echo "Output directory must be absolute" >&2; exit 1; }
mkdir -p "$out"
tmp=$(mktemp -d "$out/.preview-build.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
clang -std=c11 -O2 -Wall -Wextra -Werror -I "$here/source" "$here/tools/preview.c" \
  "$here/source/ui_screen.c" "$here/source/ui_editor.c" "$here/source/ui_frames.c" \
  "$here/source/ui_canvas.c" "$here/source/ui_font.c" -o "$tmp/preview"
"$tmp/preview" "$tmp"
for name in aw2-off aw2-on aw2-hud-unavailable sotc-hud-available control-recorder-unavailable; do
  python3 -I "$here/tools/ppm_to_png.py" "$tmp/$name-host-candidate.ppm"
  mv "$tmp/$name-host-candidate.png" "$out/$name-host-candidate.png"
done
