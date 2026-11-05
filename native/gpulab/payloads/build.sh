#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
out=${GPULAB_BUILD_DIR:-"$here/build"}
sdk=${PS5_PAYLOAD_SDK:-${PS5_VULKAN_ROOT:+$PS5_VULKAN_ROOT/.deps/native/ps5-payload-sdk}}
: "${sdk:?Set PS5_PAYLOAD_SDK or PS5_VULKAN_ROOT}"
mkdir -p "$out"
cc="$sdk/bin/prospero-clang"
"$cc" -std=gnu11 -O2 -Wall -Wextra -Werror -o "$out/helixsr-launch.elf" "$here/helixsr_launch.c" "$here/scan_engine.c" "$here/helixsr_swap.c" -lSceSystemService -lSceUserService
"$cc" -std=gnu11 -O2 -Wall -Wextra -Werror -o "$out/helixsr-inventory.elf" "$here/helixsr_inventory.c"
sha256sum "$out/helixsr-launch.elf" "$out/helixsr-inventory.elf"
