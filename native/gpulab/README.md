# Unified GPU Lab native candidate

This source builds one GPU Lab app and one pair of launch/inventory payloads for both console models. It starts from the published v0.2.0-beta GPL source archive (SHA-256 `3798d63f88d5ac8d1ba9e9fabe9a8a840bc308c6170a0c64e8e461c50e0fe028`): app u10, payload u08 and u06's PS4 metadata reader. Original notices and [GPL-3.0-or-later terms](COPYING) remain in force.

This working candidate adds SAROS PPSA07631 01.000.000 package switching on a regular PS5. SAROS controller-driven ON/OFF launch is verified on the regular PS5. The v27 performance preset measured 59.93 FPS in three native stationary/movement captures. The final v27 app passed controller ON/OFF, exact-build launch and stock restoration. It retains CONTROL r18 and AW2 t12 package switching on a regular PS5 and SOTC live3's hook setting on PS5 Pro. Exact identities are PPSA34547 01.300.300, PPSA02572 01.200.007 and CUSA08809 01.00 respectively. Installer-written device.txt selects the model; this is configuration, not hardware autodetection or Pro qualification of the other games.

AW2 Cross selects ON/OFF; Triangle closes GPU Lab, verifies the folder and support files, applies the choice, then starts the game. No PC switch command is needed after installation. The game still uses HelixSR only in Performance. Quality, sharpening and cut/reset qualification remain pending. A READY marker only enables the UI; the payload checks actual files before activation.

Each package has its own CATALOG record for first-run discovery. The installer leaves the scanner's shared catalog alone. The identity hash in this package record identifies the installed executable or hook integration, not a fresh game scan. Incorrect versions, unknown device labels, extra overlay files, damaged support files and symlink paths are refused. Existing unknown backports must be resolved before installation.

## Build and tests

Use Linux or WSL with Clang 18 and built public [PS5_Vulkan](https://github.com/mihawk-99/PS5_Vulkan) dependencies, including its payload SDK, clean-room runtime and native conversion tool. No proprietary SDK is used. The dependency checkout is read-only during these builds. Set `PS5_VULKAN_ROOT` to that checkout, `PS5_PAYLOAD_SDK` if its SDK is elsewhere, and `PS5_CLANG` to the matching compiler driver. Its resource directory must contain `lib/linux/libclang_rt.builtins-x86_64.a`.

```sh
bash native/gpulab/app/test.sh
bash native/gpulab/payloads/test.sh
bash native/gpulab/app/build.sh
bash native/gpulab/payloads/build.sh
```

`GPULAB_BUILD_DIR` selects an alternate output directory for either build. The app build reconstructs its public AGC link stubs from source and writes all generated files locally. A dependency receipt records exact hashes of the local compiler, SDK libraries and conversion inputs. Two fresh builds must match; that demonstrates repeatability with these inputs, not yet an independent clean toolchain bootstrap.

The recorded dependency pins are PS5_Vulkan `252bf912a745e3566a93b69b098bdc8ec4048837` and its public [PS5_PayloadSDK fork](https://github.com/mihawk-99/PS5_PayloadSDK) `b83202be73e930050e00de0f1b4d9ec46c0391bf`, with Clang 18.1.8. [dependency-receipt.json](dependency-receipt.json) records the actual library and header hashes. The candidate source ZIP also carries the verified original `baseline-native-source.zip` as corresponding source for unchanged beta components, including the SOTC hook. It is the published original archive, not a build output from this candidate.

The host tests use synthetic filesystem and frame data. Native compilation, rendered previews and installer simulation are separate evidence levels. The separate SAROS native qualification is documented in [SAROS results](../../docs/saros.md). Other-game and Pro qualifications do not follow from SAROS tests.

## SAROS qualification

See [SAROS results and limits](../../docs/saros.md): full 4K reconstruction, four nominal-60 captures, controller ON/OFF, launch and stock restoration. The preset disables dynamic shadows and volumetric fog. Comprehensive temporal quality and all-area performance are not established.

## Exact macOS build used for SAROS

The scripts in `tools/` compile and link with Homebrew LLVM 18.1.8. Set `LLVM_BIN`, `PS5_PAYLOAD_SDK`, `GPULAB_DEPS`, and `GPULAB_BUILD_DIR`. `GPULAB_DEPS` holds the pinned public `vulkan/` and `sdk/` inputs, built `platform-build/libps5platform.a`, `compiler-rt/emutls.o`, and `ps5-native-tool`. Input commit/file identities are in `tools/provenance.json` and `tools/extra-provenance.json`; the original Linux dependency receipt above is historical.

```sh
python native/gpulab/tools/build-gpulab-mac.py
python native/gpulab/tools/link-gpulab-mac.py
python native/gpulab/tools/build-gpulab-payloads.py
```

A fresh build from these delivered scripts matched the installed app and both payloads byte for byte. These scripts reuse already built, pinned public dependencies; they do not claim a fresh dependency bootstrap.
