# Unified GPU Lab native candidate

This source builds one GPU Lab app and one pair of launch/inventory payloads for both console models. It starts from the published v0.2.0-beta GPL source archive (SHA-256 `3798d63f88d5ac8d1ba9e9fabe9a8a840bc308c6170a0c64e8e461c50e0fe028`): app u10, payload u08 and u06's PS4 metadata reader. Original notices and [GPL-3.0-or-later terms](COPYING) remain in force.

This working candidate adds SAROS PPSA07631 01.000.000 package switching on a regular PS5. SAROS controller-driven ON/OFF launch is verified on the regular PS5. The effects-on package measured 59.93 FPS in a clean 90-second display-counter gameplay capture. GPU Lab OFF and ON launches and normal closure are verified for this package. It retains CONTROL r18 and AW2 t12 package switching on a regular PS5 and SOTC live3's hook setting on PS5 Pro. Exact identities are PPSA34547 01.300.300, PPSA02572 01.200.007 and CUSA08809 01.00 respectively. Installer-written device.txt selects the model; this is configuration, not hardware autodetection or Pro qualification of the other games.

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

The host tests use synthetic filesystem and frame data. Native compilation, rendered previews and installer simulation are separate evidence levels. None proves that this candidate launches, switches, recovers or uninstalls on hardware. Console ownership and an explicit trial handback are required before live validation. Release publication and hardware evidence are recorded in the SAROS qualification document.

## SAROS candidate

The exact-version package uses a single atomic whole-folder rename between the backport directory and the package's `stored/game` directory. Enabling checks the complete model asset by size and SHA-256 before any move. Disabling remains available with a missing or damaged model asset so stock restoration is possible. The native launch payload closes the app before applying the switch and refuses a running game, unknown files, unsupported console labels and other game versions.

Host verification covers SAROS ON/OFF roundtrips, missing and corrupt assets, symlink rejection, busy gating and interrupted rename recovery. Controller OFF and ON launches passed on the regular PS5; full mounted executable hashes matched stock and the effects-on candidate respectively. Both reached gameplay and closed normally.

### SAROS effects-on package

The v6 package reconstructs at 2560×1440, then scales the reconstructed output to 3840×2160. Shadows and volumetric fog remain enabled. Stock dynamic input settings remain unchanged: minimum 41.666%, maximum 60%, frame budget 16.67 ms. Native probes observed 1600×900 and 2304×1296 input. There is no TSR fallback; unsupported render contracts are rejected.

A clean 90-second gameplay sample from ps5-exporter v0.3.1's display-controller flip counter averaged 59.933 FPS, with all 90 samples at 59–60 FPS. Three native callback traces during camera movement, walking and a ledge-return sequence recorded zero TSR calls, zero withheld outputs and no tick gaps. This is bounded scene coverage; callback timing is not GPU duration, and the once-per-second counter is not a per-frame or HDMI measurement. Full death/respawn coverage and comprehensive temporal image-quality acceptance remain unverified.

Native full-frame scaler readbacks passed independent bilinear FP16 reference comparisons (one packed-channel code-step tolerance): primary 8,294,400 pixels, quarter 518,400 pixels, and early larger-input primary 8,294,400 pixels. These establish scaler correctness for the captured frames, not neural reconstruction fidelity or all-scene image quality. Prior network outages remain unexplained; one occurred before SAROS launched. The latest run stayed reachable through a normal game close.
