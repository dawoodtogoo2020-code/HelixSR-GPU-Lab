# SAROS effects-on v6 native hook

This directory contains the v6 hook and exact-build interfaces. It preserves stock dynamic input and visual settings, reconstructs at 2560×1440, scales to 3840×2160, and has no TSR fallback. Unsupported contracts stop execution. See [qualification and limits](../../docs/saros.md).

Use Clang/LLVM 18.1.8 and Python with Capstone. Supply your own decoded SAROS ELF (SHA-256 `26585333efaf6a14b914da8d33d3b2fc5471155ffb7dc724d84371e028979f46`):

```sh
python native/saros/raw-chain/clone_grid.py /path/to/your/eboot.elf /path/to/private-clone --llvm /path/to/llvm18/bin
python native/saros/replacement1440-v6/build.py native/saros/replacement1440-v6/profile.json /path/to/private-clone /path/to/hook-build --llvm /path/to/llvm18/bin --optimization staged-lto --producer compact
python native/saros/pack_candidate.py /path/to/hook-build --stock /path/to/your/eboot.self
```

The graph object must hash to `ef69695411a48b866958147bce0c4ea92973577dc4f529148f53183644a84388`. The 10,124-byte hook SHA-256 is `eaddbb8bd357dd523c545d3d49321ceb1684ea7445c84b4d5a0f7603c062a5dd`; it fits the unchanged 10,368-byte slot. The final SELF must match the game manifest.

The model asset is reconstructed by the HXD recipe from owner-supplied shader data and locally generated weights. `scalers1440` contains the authored pipelined scaler assembly. Build-time hardware-status fields describe only the build invocation; hardware evidence is separate. Historical v27 source is retained for provenance and does not describe this package.

The DOF input fix recognises the exact `DOF.Recombine` name at title-relative address `0x72f8ca0`. The original FP16 format, allocation, active-rectangle, depth and velocity checks still apply. The prior adapter rejected this valid gameplay producer and executed its diagnostic `ud2` (`e4410003`). Host execution tests cover the real guard, including refusal cases: `python tests/test_saros_native_input.py` (Unicorn and LLVM 18 required). Two spare job counters record the current DOF input and cumulative DOF frame count without changing the exported receipt ABI.

The separate-translucency correction changes the native compositor's existing defer-colour flag load at RVA `0xad445b` to `mov cl,0`, preserving the remainder of RCX. The compositor at `0xadf6de` therefore blends both colour and modulation before reconstruction instead of leaving colour for the replaced TSR shader. A second existing format load at `0xad45c4` selects FP16 RGBA, preserving the neural input contract. No compositor shader or blend formula is reimplemented. The resulting exact producer name is `PostDOFTranslucency.SceneColor` (`0x713b316`). Two other unused job counters record current/cumulative composed inputs. `pack_candidate.py` verifies and writes these two additional fixed ranges.

Developer CPU validation, requiring the exact owner's decoded ELF and Unicorn:

```sh
python native/saros/check_composition.py --stock-elf /path/to/your/eboot.elf --candidate /path/to/hook-build/candidate.self
```

This runs original compositor branch instructions against synthetic resources, comparing deferred/bypass, modulate-only and complete-colour paths, and checks both actual candidate instruction patches. It does not emulate GPU shading or establish visual correctness. See the [effects audit](../../docs/saros-effects-audit.json) for other pass consumers and qualification status.
