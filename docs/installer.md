# Support bundle verification preview

> To install HelixSR GPU Lab on a PS5, use the [install guide](install-ps5.md) and `gpulab.py`. This page describes the older, separate bundle verifier from the v0.1.0 developer preview.

No approved or provenance-verified binary bundle is available. This tool validates a local extracted bundle and prints a dry-run plan. It never connects to a console. The `--install` option deliberately fails closed, including after a successful verification. Do not use the plan as evidence of installation or platform compatibility.

A prospective bundle has `manifest.json` at its root and exactly these manifest entries: `app/eboot.bin`, `app/sce_module/libc.prx`, `app/sce_sys/icon0.png`, `app/sce_sys/param.json`, `payloads/helixsr-launch.elf`, and `payloads/helixsr-inventory.elf`. The JSON format is `{"format":1,"files":{"app/eboot.bin":{"size":123,"sha256":"<64 lowercase hex characters>"}, ...}}`, with an entry for each file. The manifest itself must be UTF-8, and the separately supplied SHA-256 must cover its exact bytes. Files must be ordinary files, and path components may not be symbolic links or Windows reparse points.

Run `python tools/install_support.py PATH_TO_EXTRACTED_BUNDLE --manifest-sha256 TRUSTED_MANIFEST_SHA256`. Obtain that hash independently from a source you trust. A matching hash verifies the chosen manifest, not its publisher or the rights to distribute its contents. The tool checks only listed files; do not treat unrelated files in the folder as verified. The plan contains no game files, device marker, request, or game-modifying operations.

Use a trusted local bundle that stays unchanged during verification. The checks compare opened file metadata and bound reads, but cannot prevent every path race on every filesystem. A successful plan describes a point in time; it does not ensure files remain unchanged afterward. A future installer must independently revalidate the files it actually uses.

Before any live installer is offered, establish provenance and redistribution rights for every binary and dependency, then implement and test conflict handling, staging, readback verification, partial-failure receipts, and recovery on a suitable independently configured console. No such device validation is claimed here.
