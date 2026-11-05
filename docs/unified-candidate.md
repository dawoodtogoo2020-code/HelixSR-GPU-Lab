# Unified app candidate

This is an unreleased implementation candidate built from v0.2.0-beta. One app and one pair of launch/inventory payloads are used for regular PS5 and PS5 Pro. [Native source and build instructions](../native/gpulab/README.md) are included.

Implemented:

- AW2 t12 appears on first run with an in-app ON/OFF choice and launch handoff. Cross selects the choice; Triangle verifies and applies it after GPU Lab exits, then starts AW2. Performance remains the only HelixSR mode. Quality uses stock FSR2.
- Package-owned catalog records let clean installations find the three supported integrations without an initial scan. The installer records the selected console model on both console types.
- Catalog entries are reconciled against the installed title and version. Installer identities take precedence over old scan records. Missing or mismatched metadata blocks managed game launch, so a saved OFF choice cannot silently launch an existing ON overlay.
- Both native and PC switching refuse mismatched, incomplete or foreign overlay contents. AW2 activation also verifies its external copy and chain data. OFF remains possible when those support files are damaged but the overlay itself is intact.
- Reinstall preserves OFF. Backup intent is journalled before mutation. Recovery refuses unrecognized partial uploads, unknown settings and incomplete directory listings. Uncertain files remain in place for inspection.
- SOTC's hook setting is written atomically, with checked flush, close and readback. A saved choice is not proof of game execution.

Only CONTROL PPSA34547 01.300.300 and AW2 PPSA02572 01.200.007 on regular PS5, and SOTC CUSA08809 01.00 on PS5 Pro are eligible. The device label is installer configuration. It does not qualify CONTROL or AW2 on Pro.

The existing installer commands and license flow remain. Source alone lacks the public beta's patch data; the candidate packaging tool combines a verified beta ZIP with locally built native outputs. It never includes game files, NVIDIA DLL/network data, console addresses or installation journals. Preserve each PC's existing my-ps5 state when updating an installation.

Remaining qualification includes actual native launch and switch behavior, homebrew filesystem semantics, clean installation, interrupted-install recovery and uninstall on owned consoles. Host simulation and repeated binary builds cannot substitute for those trials. No new firmware compatibility or FPS improvement is claimed.

Graphics work remains open: AW2 Quality requires a new 4K plan and bounded kernels; RCAS requires verified scratch/output resources; camera cuts and fallback re-entry need history qualification. SOTC still uses finished colour with placeholder depth/motion/jitter. CONTROL r18 matched timing, current-frame fallback, history reset and Pro qualification remain incomplete.
