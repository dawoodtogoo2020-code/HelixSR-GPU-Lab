#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out=${GPULAB_BUILD_DIR:-"$here/build"}
mkdir -p "$out"
test_root=$(mktemp -d "$out/payload-test.XXXXXX")
trap 'rm -rf "$test_root"' EXIT
cc=(clang)
if [[ $(uname -s) == Darwin ]]; then
  cc+=(-D_DARWIN_C_SOURCE -Dst_mtim=st_mtimespec)
fi
"${cc[@]}" -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-function "$here/swap_test.c" -o "$out/swap_test"
"$out/swap_test"
"${cc[@]}" -std=c11 -O2 -Wall -Wextra -Werror "$here/scan_test.c" "$here/scan_engine.c" -o "$out/scan_test"
"$out/scan_test" "$here/scan_engine.c"
printf '%s\n' '{"contentVersion":"01.000.000","localizedParameters":{"en-US":{"titleName":"Example Game"}}}' > "$test_root/param.json"
printf '%s\n' 'ps5-regular' > "$test_root/device.txt"
"${cc[@]}" -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-function -DHELIX_ROOT="\"$test_root\"" "$here/payload_test.c" "$here/scan_engine.c" "$here/helixsr_swap.c" -o "$out/payload_test"
"$out/payload_test" "$test_root/param.json"
"${cc[@]}" -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-function -DHELIX_ROOT="\"$test_root\"" -DUSER_APPMETA="\"$test_root/user\"" -DSYSTEM_APPMETA="\"$test_root/system\"" "$here/inventory_test.c" -o "$out/inventory_test"
"$out/inventory_test"
