#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
: "${PS5_VULKAN_ROOT:?Set PS5_VULKAN_ROOT to the built public PS5_Vulkan checkout}"
root=$PS5_VULKAN_ROOT
sdk=${PS5_PAYLOAD_SDK:-$root/.deps/native/ps5-payload-sdk}
native=$root/tooling/native
build=${GPULAB_BUILD_DIR:-$here/build}
mkdir -p "$build/obj" "$build/stubs" "$build/dist/PPSA99272/sce_sys" "$build/dist/PPSA99272/sce_module"
build=$(cd "$build" && pwd)
export TMPDIR="$build/tmp"
mkdir -p "$TMPDIR"
cc() { PS5_PAYLOAD_SDK="$sdk" sh "$root/tooling/prospero-clang18" -Dlstat=helix_native_lstat -ffile-prefix-map="$here"=gpulab-app -ffile-prefix-map="$root"=PS5_Vulkan "$@"; }
# Recompile all objects so changed compiler flags cannot reuse stale output.
objects=()
for src in "$here/source"/*.c; do
 obj="$build/obj/$(basename "${src%.c}").o"
 cc -std=c11 -O2 -Wall -Wextra -Werror -ffunction-sections -fdata-sections -I "$here/source" -c "$src" -o "$obj"
 objects+=("$obj")
done
for src in "$here/source/native_main.cpp" "$here/source/display.cpp" "$native/app_crt.cpp" "$native/app_cpp_runtime.cpp"; do
 obj="$build/obj/$(basename "${src%.cpp}").o"
 cc -std=c++20 -O2 -Wall -Wextra -Werror -fno-exceptions -fno-rtti -ffunction-sections -fdata-sections -I "$here/source" -c "$src" -o "$obj"
 objects+=("$obj")
done
stub() {
 local library=$1 source=$2
 cc -std=c11 -O2 -fPIC -c "$root/$source" -o "$build/stubs/${library}.o"
 "$sdk/bin/prospero-lld" --shared -soname "${library}.prx" -o "$build/stubs/${library}.so" "$build/stubs/${library}.o"
}
stub libSceAgc vendor/ps5/sdk/stubs/agc_canary_link_stub.c
stub libSceAgcDriver vendor/ps5/sdk/stubs/agc_driver_canary_link_stub.c
"$sdk/bin/llvm-ar" rcs "$build/empty.a"
# Isolate the upstream linker's generated map from the shared dependency checkout.
python3 - "$root/tools/radv-link.sh" "$build/radv-link.sh" <<'PY'
from pathlib import Path
import sys
source = Path(sys.argv[1]).read_text()
old = 'local map="$root/build/radv-platform-local.map"'
if source.count(old) != 1:
    raise SystemExit('Unsupported radv-link recipe: inspect its map output before building')
source = source.replace(old, 'local map="$GPULAB_LINK_WORK/radv-platform-local.map"')
source = source.replace('mkdir -p "$root/build"', 'mkdir -p "$GPULAB_LINK_WORK"')
Path(sys.argv[2]).write_text(source)
PY
export GPULAB_LINK_WORK="$build"
source "$build/radv-link.sh"
radv_link_recipe "$root" "$sdk" "$build/empty.a"
radv_link_flags+=("--defsym=fstatat=helix_native_fstatat")
"$sdk/bin/prospero-lld" "${radv_linker_script[@]}" --eh-frame-hdr "${radv_link_flags[@]}" --version-script "$native/app-symbols.map" --exclude-libs=ALL -e _start -o "$build/llvm-pie.elf" "${objects[@]}" "$build/stubs/libSceAgc.so" "$build/stubs/libSceAgcDriver.so" "${radv_link_inputs[@]}" --as-needed "$sdk"/target/lib/*.so
"$root/build/host/ps5-native-tool" link --in "$build/llvm-pie.elf" --out "$build/eboot.elf" --stub-dir "$sdk/target/lib" --stub "$build/stubs/libSceAgc.so" --stub "$build/stubs/libSceAgcDriver.so" --module-sdk 0x02000009 --companion-sdk 0x08050001 --file-name eboot.elf
"$root/build/host/ps5-native-tool" self --sign --in "$build/eboot.elf" --out "$build/dist/PPSA99272/eboot.bin" --magic 0x1D3D154F
cp "$here/source/param.json" "$here/source/icon0.png" "$build/dist/PPSA99272/sce_sys/"
cp "$root/runtime/libc.prx" "$build/dist/PPSA99272/sce_module/"
sha256sum "$build/dist/PPSA99272/eboot.bin" "$build/eboot.elf"
