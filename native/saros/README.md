# SAROS effects-on v6 native hook

This directory contains the v6 hook and exact-build interfaces. It preserves stock dynamic input and visual settings, reconstructs at 2560×1440, scales to 3840×2160, and has no TSR fallback. Unsupported contracts stop execution. See [qualification and limits](../../docs/saros.md).

Use Clang/LLVM 18.1.8 and Python with Capstone. Supply your own decoded SAROS ELF (SHA-256 `26585333efaf6a14b914da8d33d3b2fc5471155ffb7dc724d84371e028979f46`):

```sh
python native/saros/raw-chain/clone_grid.py /path/to/your/eboot.elf /path/to/private-clone --llvm /path/to/llvm18/bin
python native/saros/replacement1440-v6/build.py native/saros/replacement1440-v6/profile.json /path/to/private-clone /path/to/hook-build --llvm /path/to/llvm18/bin --optimization staged-lto --producer compact
python native/saros/pack_candidate.py /path/to/hook-build --stock /path/to/your/eboot.self
```

The graph object must hash to `ef69695411a48b866958147bce0c4ea92973577dc4f529148f53183644a84388`. The 10,017-byte hook SHA-256 is `5cd54c7a72d8a93b08db9670f649b535eeb9533f6177a1a23a29da475e1620b2`; it fits the unchanged 10,368-byte slot. The final SELF must match the game manifest.

The model asset is reconstructed by the HXD recipe from owner-supplied shader data and locally generated weights. `scalers1440` contains the authored pipelined scaler assembly. Build-time hardware-status fields describe only the build invocation; hardware evidence is separate. Historical v27 source is retained for provenance and does not describe this package.
