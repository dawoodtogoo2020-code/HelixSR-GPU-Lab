# Existing-installation migration candidate

This private candidate is for the existing verified regular-PS5 CONTROL r18 and AW2 t12 installation. It is host-tested; the Mac session owns console installation and runtime testing. The Windows build session must not contact either PS5. No new graphics feature or performance result is claimed.

## What changes

- AW2 accepts the exact verified 74,527,836-byte FTP representation `31a3ffd1b582b6d363da7432c41962e1b48ba59fcaa14befaae916d38cb66c35`, alongside the original SELF and expected ELF. Adoption does not replace the working game executable.
- CONTROL moves its four Helix overlay files individually. Its existing three known `fakelib2` libraries stay in the canonical backport directory in both ON and OFF states. Missing, altered, extra or linked library entries are rejected. Installations without that optional directory remain supported. OFF therefore removes the Helix overlays while retaining validated backport libraries.
- App/runtime readback accepts exact packaged forms or the complete candidate-specific normalized ELF digest. Only ELF section-table fields at byte ranges `[40:48]` and `[58:64]` are normalized; every other byte is hashed, and at most 75 additional bytes are allowed only when all are zero. Expected lengths and hashes are in the console manifest. An older app or libc is not accepted merely because its size matches.
- Adoption upgrades the app, launch/inventory payloads, device text and READY/CATALOG metadata. It verifies and retains game executable, shader/data, AW2 support and CONTROL dependency files.

## Mac deployment

Prerequisites: Python 3.10+, ftpsrv on port 2121, pldmgr on HTTP 8084 with a complete process list, ShadowMount+, and no competing console writer. Keep the Mac's existing backups and the installer journal under `my-ps5`. FTP copies of executables may be transformed ELF views; they are not automatically interchangeable with the original SELF files. The installer preserves replaced originals using console-side rename.

From the extracted candidate directory, with GPU Lab and all games closed:

```sh
python3 tools/gpulab.py install --adopt --ps5 PS5_IP --model ps5 --game control-r18 --game aw2-t12
```

Replace `PS5_IP` with the console address reachable from the Mac. This path needs no game-file rebuild, game download, NVIDIA input or weight generation. It must finish successfully before opening GPU Lab or launching a game. If it refuses a file, directory, process list or readback, retain the journal and investigate the exact refusal. Do not bypass a guard or delete a backport dependency. Use the journaled recovery path for an interrupted adoption; preserve console-side original backups until the Mac confirms acceptance.

After adoption, the Mac must verify app/payload and metadata readback, unchanged game/support/dependency hashes, native UI launch, and ON/OFF behavior. CONTROL dependencies must remain byte-identical at the canonical path throughout. An interrupted file switch reports mixed state and must be completed before launch. Neither host tests nor configured manifests establish successful native device execution.

## Exact game identities

CONTROL PPSA34547 01.300.300 and Alan Wake 2 PPSA02572 01.200.007, regular PS5 only. SOTC PS5 Pro is not part of this adoption flow. AW2 Quality/RCAS/history, SOTC temporal integration/performance and CONTROL graphics qualification remain unfinished.
