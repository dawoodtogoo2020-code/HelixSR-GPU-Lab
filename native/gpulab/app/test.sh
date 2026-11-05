#!/usr/bin/env bash
# Host tests of the GPU Lab logic: u06's game library test plus the u07 modules. Files u07 added or
# changed build with -Wall -Wextra -Werror; unchanged u03 support files that only link the HRO1 store build without
# -Werror here, because tiny_store.c has a Linux-only format warning (st_nlink is wider on the host than on PS5; the PS5
# build.sh compiles it with -Werror and passes).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
src=$here/source
tmp=$(mktemp -d "${TMPDIR:-/tmp}/gpulab-tests.XXXXXX")
trap 'rm -rf "$tmp"' EXIT
# The storage and hook tests deliberately reject symlinked ancestors.
tmp=$(cd "$tmp" && pwd -P)
cc=(clang -std=c11 -O1 -I"$src")
if [[ $(uname -s) == Darwin ]]; then
  # Darwin gates flock/O_NOFOLLOW and names the nanosecond mtime st_mtimespec.
  cc+=(-D_DARWIN_C_SOURCE -Dst_mtim=st_mtimespec)
fi
strict=("${cc[@]}" -Wall -Wextra -Werror)
"${strict[@]}" "$src/game_library.c" "$here/tests/test_game_library.c" -o "$tmp/test_game_library"
mkdir -p "$tmp/library" && "$tmp/test_game_library" "$tmp/library"
"${strict[@]}" "$src/launch_policy.c" "$here/tests/test_launch_policy.c" -o "$tmp/test_launch_policy"
"$tmp/test_launch_policy"
support=()
for n in profile_namespace profile_store tiny_store activation_policy encoder_sha256; do
  "${cc[@]}" -w -c "$src/$n.c" -o "$tmp/$n.o"; support+=("$tmp/$n.o")
done
"${strict[@]}" "$src/resolution_plan.c" "$src/render_options.c" "$here/tests/test_resolution_plan.c" "${support[@]}" -o "$tmp/test_resolution_plan"
"$tmp/test_resolution_plan"
"${strict[@]}" "$src/frame_log.c" "$here/tests/test_frame_log.c" -o "$tmp/test_frame_log"
mkdir -p "$tmp/logs" && "$tmp/test_frame_log" "$tmp/logs"

"${strict[@]}" "$here/tests/test_hook_setting.c" -o "$tmp/test_hook_setting"
mkdir -p "$tmp/hook" && "$tmp/test_hook_setting" "$tmp/hook"

