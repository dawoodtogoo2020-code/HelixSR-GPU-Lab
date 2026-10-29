# Installation status

## Available now: developer tools on your computer

1. Download the developer preview from [Releases](https://github.com/dawoodtogoo2020-code/HelixSR-GPU-Lab/releases).
2. Extract the archive. Use Python 3.10 or newer on Windows, Linux or macOS.
3. Run `python tools/install_support.py --help` to inspect the bundle verifier, or run the tests in the README.

This installs no console component. The archive contains source and host tools, not a PS5 payload. No Python package installation or account credential is required by the tools.

## PS5 upscaler installation: not yet released

There is no approved public PS5 app, payload or CONTROL package in this preview. We cannot give a working end-user install command until the native application, model assets and game adapter can be distributed and validated together.

The private lab uses a compatible homebrew environment, GPU Lab, a payload manager and an exact game-specific package. The current lab installer contains local assumptions and cannot be reused safely as a general installer. This project does not install homebrew prerequisites, supply firmware exploits, bypass game licensing or establish support for a stock retail console. No firmware range or general PS5 Pro compatibility is certified by this release.

## Planned end-user flow

1. Check the exact title ID, game version and supported console environment against the released compatibility list.
2. Download a versioned GPU Lab support bundle and verify its published digest.
3. Review the install plan and choose the intended console.
4. Install only GPU Lab's support files, then launch the application and discover games.
5. Select a separately qualified game adapter. Apply only while the game is closed, with validated recovery available.
6. Launch the game, confirm the active rendering path, and use the documented original-path restore flow when required.

These are release design requirements, not instructions for an available console release. A one-payload delivery option remains planned.

## Bundle verification

The [verifier documentation](installer.md) describes the supported manifest format and dry-run checks. A future support bundle is limited to GPU Lab application/payload files. Game files, model packages, activation requests and console device markers are outside its scope. The tool refuses installation in this version.

## Recovery and removal

This preview changes no console files, so it requires no PS5 rollback. Remove the extracted developer-tools folder when you no longer need it. For an existing private lab installation, use its owning version's recorded recovery procedure; these tools must not be used to remove or overwrite that installation.
