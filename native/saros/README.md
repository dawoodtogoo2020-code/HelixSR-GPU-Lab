# SAROS v27 native hook

This directory contains the authored v27 runtime hook, its exact-build interface definitions, and an owner-file generator for the private graph-emitter object. No generated retail instruction copy or game executable is included. See [qualification and limits](../../docs/saros.md).

Use Clang/LLVM 18.1.8 and Python with Capstone. Supply your own decoded SAROS ELF, SHA-256 `26585333efaf6a14b914da8d33d3b2fc5471155ffb7dc724d84371e028979f46`:

```sh
python native/saros/raw-chain/clone_grid.py /path/to/your/eboot.elf /path/to/private-clone --llvm /path/to/llvm18/bin
python native/saros/replacement-v27/build.py native/saros/replacement-v27/profile.json /path/to/private-clone /path/to/hook-build --llvm /path/to/llvm18/bin --optimization staged-lto --producer compact
python native/saros/pack_candidate.py /path/to/hook-build --stock /path/to/your/eboot.self
```

The graph object must hash to `ef69695411a48b866958147bce0c4ea92973577dc4f529148f53183644a84388`. The hook must be exactly 10,261 bytes with SHA-256 `cee4b65047dd07cd7cab28fd03bd663ea96d880fb7adf0e4e465c2d46a3267fb`. It fits the original 10,368-byte slot. The packaged SELF must match the manifest. These steps were reproduced from this delivered source.

`profile.json` describes the qualified 28-record asset and buffer layout; the sidecar itself is reconstructed by the HXD recipe in `games/saros-native-v1` using owner inputs and locally generated model weights. The build receipt's “hardware untested” field describes the build invocation alone; the separate native verification is documented in `docs/saros.md`.
