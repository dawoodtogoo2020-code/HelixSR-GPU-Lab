#!/usr/bin/env python3
"""Build the console weight file (weights.ngx.bin) from NVIDIA's DLSS DLL, on your own PC.

    python helixsr_weights.py --download --out weights.ngx.bin     (asks you to accept NVIDIA's license first)
    python helixsr_weights.py --dll nvngx_dlss.dll --out weights.ngx.bin

GPU Lab ships no NVIDIA material. Like HelixSR's own setup, this downloads NVIDIA's DLSS 310.7.0 DLL from NVIDIA's
GitHub only after you accept NVIDIA's license, reads the network from it, and deletes the DLL again. Two steps, with
HelixSR 1.8.1's tables (helixsr_model_tables.py):
  1. each tensor is found in the DLL by its fingerprint and converted to fp16 (round toward zero, as NGX does): HelixSR's
     canonical blob (762adfde...);
  2. the tensors are reordered into the NGX upload layout the console kernels read.
The result must be the exact weight file every GPU Lab package was tested with (f2f09558...); nothing is written
otherwise. Standard library only.
"""
import argparse
import hashlib
import os
import sys
import tempfile
import urllib.request
from array import array
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import helixsr_model_tables as T  # noqa: E402

NGX_SHA256 = 'f2f09558a2fbfa1dd1c20a96ec1620c68b37b0958512fa16ad32a9e2da94a7cd'
DLSS_URL = 'https://raw.githubusercontent.com/NVIDIA/DLSS/v310.7.0/lib/Windows_x86_64/rel/nvngx_dlss.dll'
DLSS_LICENSE = 'https://github.com/NVIDIA/DLSS/blob/v310.7.0/LICENSE.txt'


def sha(data):
    return hashlib.sha256(data).hexdigest()


def locate(dll, align):
    """File offset of every tensor, found by fingerprint; None if one is missing."""
    want = {fp: i for i, fp in enumerate(T.FINGERPRINTS)}
    found = [None] * len(T.FINGERPRINTS)
    view = memoryview(dll)
    left = len(found)
    for p in range(0, len(dll) - 64, align):
        i = want.get(hashlib.blake2b(view[p:p + 64], digest_size=8).hexdigest())
        if i is not None and found[i] is None:
            found[i] = p
            left -= 1
            if not left:
                return found
    return None


def f16_rtz(bits):
    """fp32 bit pattern -> fp16 bit pattern, rounding toward zero (NGX's conversion)."""
    s = (bits >> 16) & 0x8000
    e = (bits >> 23) & 0xFF
    m = bits & 0x7FFFFF
    if e == 0xFF:
        return s | (0x7E00 if m else 0x7C00)
    E = e - 127
    if E > 15:
        return s | 0x7BFF
    if E >= -14:
        return s | ((E + 15) << 10) | (m >> 13)
    if E < -24 or e == 0:
        return s
    return s | ((0x800000 | m) >> (-1 - E))


def canonical(dll):
    offsets = locate(dll, 16) or locate(dll, 4)
    if offsets is None:
        raise SystemExit("this nvngx_dlss.dll does not contain the network HelixSR uses (DLSS 310.7.0); nothing written")
    out = array('H', bytes(T.CANON_BYTES))
    for (coff, O, I, k, lay), foff in zip(T.TENSORS, offsets):
        n = O * I * k * k
        src = array('I', dll[foff:foff + 4 * n])
        if sys.byteorder != 'little':
            src.byteswap()
        if lay == 'OIHW':        # -> O,H,W,I
            vals = [src[((o * I + i) * k + h) * k + w] for o in range(O) for h in range(k) for w in range(k)
                    for i in range(I)]
        elif lay == 'OI_SHUF':   # output channel o = 8g + 2p + b stored at row 10p + 2g + b
            row = n // O
            vals = []
            for o in range(O):
                r = 10 * ((o % 8) // 2) + 2 * (o // 8) + (o % 2)
                vals.extend(src[r * row:(r + 1) * row])
        else:
            vals = src
        base = coff // 2
        out[base:base + n] = array('H', map(f16_rtz, vals))
    if sys.byteorder != 'little':
        out.byteswap()
    data = out.tobytes()
    if sha(data) != T.CANON_SHA256:
        raise SystemExit('the network read from the DLL does not match HelixSR 1.8.1; nothing written')
    return data


def ngx_layout(canon):
    """HelixSR's canonical OHWI fp16 tensors -> NGX upload layout (bcm::build_weight_blob)."""
    src = array('H', canon)
    out = array('H', bytes(T.NGX_BYTES))
    for blob_off, size, canon_off, O, I, K, layout in T.NGX_ROWS:
        b, c0, n = blob_off // 2, canon_off // 2, size // 2
        if layout in (0, 3):
            out[b:b + n] = src[c0:c0 + n]
            continue
        g = I // 8
        for o in range(O):
            for h in range(K):
                for w in range(K):
                    s0 = c0 + ((o * K + h) * K + w) * I
                    for c in range(I):
                        if layout == 1:      # [I/8][H][W][O][i8]
                            d = ((((c // 8) * K + h) * K + w) * O + o) * 8 + c % 8
                        else:                # [H][W][I/8][O][i8]
                            d = (((h * K + w) * g + c // 8) * O + o) * 8 + c % 8
                        out[b + d] = src[s0 + c]
    data = out.tobytes()
    if sha(data) != NGX_SHA256:
        raise SystemExit('console weights do not match the tested file (f2f09558...); nothing written')
    return data


def download_dll(ask=input):
    print("HelixSR needs NVIDIA's DLSS 310.7.0 DLL to build its network on this PC. It is downloaded from NVIDIA's\n"
          f"GitHub ({DLSS_URL}), used once and deleted. The DLL is NVIDIA's, under NVIDIA's license:\n  {DLSS_LICENSE}")
    if ask('Do you accept NVIDIA\'s license for this download? Type yes: ').strip().lower() != 'yes':
        raise SystemExit('Not accepted; nothing downloaded. You can also pass --dll with your own nvngx_dlss.dll.')
    with urllib.request.urlopen(DLSS_URL, timeout=180) as r:
        return r.read()


def build(dll=None, canonical_blob=None):
    canon = canonical_blob if canonical_blob is not None else canonical(dll)
    if sha(canon) != T.CANON_SHA256:
        raise SystemExit('canonical weights do not match HelixSR 1.8.1 (762adfde...); nothing written')
    return ngx_layout(canon)


def write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(dir=path.parent, prefix=path.name + '.')
    with os.fdopen(fd, 'wb') as f:
        f.write(data)
    os.replace(tmp, path)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.split('\n\n')[0])
    src = ap.add_mutually_exclusive_group(required=True)
    src.add_argument('--download', action='store_true', help="download NVIDIA's DLL (asks you to accept its license)")
    src.add_argument('--dll', help="your own copy of NVIDIA's nvngx_dlss.dll (DLSS 310.7.0)")
    src.add_argument('--canonical', help="HelixSR 1.8.1's canonical weight file (helixsr_weights.bin), if you have it")
    ap.add_argument('--out', required=True, help='where to write weights.ngx.bin')
    a = ap.parse_args(argv)
    if a.canonical:
        data = build(canonical_blob=Path(a.canonical).read_bytes())
    else:
        data = build(dll=download_dll() if a.download else Path(a.dll).read_bytes())
    write(a.out, data)
    print(f'wrote {a.out} ({len(data)} bytes, sha256 {NGX_SHA256[:16]}..., the tested console weights)')


if __name__ == '__main__':
    main()
