# Lite v1.11 compatibility on the tested firmware 11.60 setup

**Historical separate-bridge control.** For the simpler current setup, use [integrated Lite + A53](lite-1160-integrated.md), which creates one payload and removes the separate bridge from the boot sequence.

The separate A53-only bridge restored SAROS startup with the original, unchanged K-Stuff Lite v1.11. The tested scope is regular PS5, firmware **11.60**, SAROS **PPSA07631 01.000.000**, and the existing effects-on HelixSR package. This is a local compatibility helper, not an upstream Lite release.

## Fresh-boot order

1. Restart normally, run the existing jailbreak and stop at Payload Manager with kernel payloads unloaded.
2. Load **lite1160-a53-bridge.elf** once and wait for successful completion. The verified log messages include `READMEM applied and read back 72 PPR words; GET_MMU restored` and `dynamic Native/PLAINTEXT_NOAUTH patch installed and read back by READMEM`.
3. Load **ShadowMountPlus_1.7-beta5fix1.elf** once and wait for its startup scan.
4. Load the original **kstuff-lite_v1.11.elf** once and wait for initialization.
5. Open GPU Lab, select SAROS with HelixSR On and press Triangle once.

Do not load the full Test5 K-Stuff payload on this boot or reinject these payloads into the already configured session. Autoload was disabled during qualification. If initialization fails, stop before launching the game. The [original Test5 recipe](SAROS-START-HERE.md) is a separate alternative that starts from a fresh boot.

## Exact identities and provenance

| File | Bytes | SHA-256 |
| --- | ---: | --- |
| Owner's `FPKG-11.60-Kstuff-test5.elf` reference | 1,802,280 | `829b45fe871dd64fbd53874ae4558076c2f1e1f203fe1ab52db5510d5fc45923` |
| Generated `lite1160-a53-bridge.elf` | 1,802,280 | `242de121a423f377fa4e46c0a2e6e1a2a43f9fb7e2935ffcbc72abf50642b9ca` |
| Original `kstuff-lite_v1.11.elf` | 1,737,080 | `ab9a6cb4d3b1daf139d4d646e402b1cf569071acd64599c936d7a3a6164dc779` |
| `ShadowMountPlus_1.7-beta5fix1.elf` | 2,498,376 | `e85fd63b705498488f39c09c9aea6f9fa22e0df2e932865de49a8cedf25172d5` |

The bridge reuses the A53 initializer, SDK and CRT from the exact existing Test5 reference. It replaces the original main with a firmware check and one A53 call, fills the remaining old main with INT3, and zeros the entire 434,088-byte embedded Test5 K-Stuff kernel image. The original kernel-loader entry path is removed. Imports, relocations and the A53 implementation are preserved. This is binary adaptation; it is not a source rebuild of the reference's A53 implementation.

Neither reference nor derived ELF is redistributed in this repository or its release assets. The generator requires an owner's existing reference matching the complete hash above and refuses modified or unknown inputs. A similarly named download is insufficient.

```sh
python3 tools/lite1160/build_bridge.py /path/to/exact/Test5.elf /path/to/lite1160-a53-bridge.elf
```

The expected output hash is listed above. The bridge returns 1 on firmware outside 11.60 before calling the initializer. It propagates the initializer's result on 11.60.

## Verification and limits

The [entry tests](../tools/lite1160/test_bridge.py) passed with Unicorn 2.1.4: unknown-reference refusal, changes confined to the declared ranges, complete embedded-kernel erasure, and execution of the actual entry bytes for 24 firmware/result combinations. The emulation checks firmware refusal, call order, return propagation, preserved RBP/RSP and call-stack alignment. It does not emulate the A53 installer or establish console safety by itself.

```sh
python3 tools/lite1160/test_bridge.py /path/to/exact/Test5.elf
```

On 6 November 2026, one fresh boot passed A53 patch installation/readback, both SAROS PPR mounts, interactive gameplay, a same-boot relaunch and two normal closes. GPU Lab used the unchanged effects-on candidate; its native mounted SELF matched SHA-256 `6797e08fe14354bb21b8c0de39e27b8222eeaf712fa4f61c5f170598c389ef11`. The original Lite and reference Test5 files were rehashed afterward and remained unchanged. The console ended at its home screen with 82 processes, no eboot, and FTP/manager reachable.

The earlier bare-Lite control stalled before gameplay with PPR/mount errno 5 and repeated I/O errors. Those failure patterns were absent from the captured bridge boot. All-zero ICV warnings remained during successful mounts. This is evidence for the A53 initialization dependency in the tested configuration, not universal Lite compatibility.

**Rest-mode/resume is unqualified.** The bridge omits Test5's resident resume monitor. Other games, firmware, long-session stability, FPS and image quality were not qualified by this test. Use the full-restart recipe above for the tested sequence.
