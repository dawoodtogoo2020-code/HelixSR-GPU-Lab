#!/usr/bin/env python3
"""Build a local A53-only adapter from the owner's exact Test5 reference.

This does not build or rename K-Stuff Lite. It retains the reference's CRT,
SDK and verified A53 installer, replaces main, and erases its K-Stuff image.
The resulting helper must precede the original Lite on an otherwise clean boot.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

REFERENCE_SHA256 = '829b45fe871dd64fbd53874ae4558076c2f1e1f203fe1ab52db5510d5fc45923'
REFERENCE_BYTES = 1802280
TEXT_FILE_OFFSET = 0x4000
MAIN = 0x2910
MAIN_END = 0x3630
GET_FW = 0xfcc20
A53_INSTALL = 0x8920
KSTUFF_IMAGE = 0x136ae7
KSTUFF_IMAGE_BYTES = 434088
SUPPORTED_FW = 0x11600000


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def entry_stub():
    # SysV x86-64: preserve RBP and align RSP before both calls.
    code = bytearray.fromhex('55 48 89 e5')
    def call(target):
        code.append(0xe8)
        code.extend(struct.pack('<i', target - (MAIN + len(code) + 4)))
    call(GET_FW)
    code.extend(bytes.fromhex('25 00 00 ff ff'))  # and eax, 0xffff0000
    code.append(0x3d)
    code.extend(struct.pack('<I', SUPPORTED_FW))
    code.extend(bytes.fromhex('75 07'))  # mismatch -> return 1, no A53 call
    call(A53_INSTALL)
    code.extend(bytes.fromhex('5d c3'))  # preserve installer return value
    code.extend(bytes.fromhex('b8 01 00 00 00 5d c3'))
    return bytes(code)


def build(reference):
    if len(reference) != REFERENCE_BYTES or sha256(reference) != REFERENCE_SHA256:
        raise ValueError('The source is not the exact verified Test5 reference')
    start = TEXT_FILE_OFFSET + MAIN
    if reference[start:start + 16] != bytes.fromhex('554889e54157415641554154534883e4'):
        raise ValueError('Unexpected original main')
    image = TEXT_FILE_OFFSET + KSTUFF_IMAGE
    phoff = struct.unpack_from('<Q', reference, image + 32)[0]
    phentsize, phnum = struct.unpack_from('<HH', reference, image + 54)
    # frankenelf.py replaces unused ELF header fields; the loader uses
    # sizeof(Elf64_Phdr), not the zeroed e_phentsize field.
    if (phoff, phentsize, phnum) != (64, 0, 2):
        raise ValueError('Unexpected embedded kernel image headers')
    headers = [struct.unpack_from('<II6Q', reference, image + phoff + i * 56)
               for i in range(phnum)]
    if any(h[0] != 1 for h in headers) or max(h[2] + h[5] for h in headers) != KSTUFF_IMAGE_BYTES:
        raise ValueError('Unexpected embedded kernel image extent')
    result = bytearray(reference)
    stub = entry_stub()
    result[start:TEXT_FILE_OFFSET + MAIN_END] = b'\xcc' * (MAIN_END - MAIN)
    result[start:start + len(stub)] = stub
    result[image:image + KSTUFF_IMAGE_BYTES] = bytes(KSTUFF_IMAGE_BYTES)
    return bytes(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reference', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    source = args.reference.read_bytes()
    result = build(source)
    args.output.write_bytes(result)
    receipt = {
        'purpose': 'A53 initializer only; not a K-Stuff payload',
        'source_sha256': REFERENCE_SHA256,
        'output_sha256': sha256(result), 'output_bytes': len(result),
        'firmware': '11.60 only; all other system firmware returns 1',
        'entry_stub_va': hex(MAIN), 'entry_stub_hex': entry_stub().hex(),
        'a53_initializer_va': hex(A53_INSTALL),
        'erased_embedded_kstuff_va': hex(KSTUFF_IMAGE),
        'erased_embedded_kstuff_bytes': KSTUFF_IMAGE_BYTES,
        'unchanged': 'ELF entry/CRT, dynamic imports/relocations, A53 installer and its data',
        'hardware_qualification': 'pending',
    }
    args.output.with_suffix('.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
