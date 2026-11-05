"""The ELF a PS5 FTP readback returns for a fake-signed SELF.

The console serves an installed SELF as its ELF: the embedded ELF header
and program headers plus every stored segment at its ELF file offset, zero
everywhere else, at the standalone ELF's length. Unbacked program regions stay
zero: the general-purpose host extractor's version-record trailer recovery is
not part of the FTP representation. Both public fake-SELF magics are supported.
Checked against the K8 build and the clean-room runtime's console readback:
    python3 -I tools/self_image.py SELF ELF_LENGTH_SOURCE [EXPECTED_SHA256]
"""
import hashlib, struct, sys

def segments(self):
    magic, = struct.unpack_from('<I', self, 0)
    assert magic in (0x1D3D154F, 0xEEF51454), hex(magic)
    count, = struct.unpack_from('<H', self, 0x18)
    rows = {}
    for i in range(count):
        props, offset, filesz, memsz = struct.unpack_from('<4Q', self, 0x20 + i * 32)
        if props & 0x800 and not props & 0xA:  # data blocks, neither encrypted nor compressed
            rows[(props >> 20) & 0xFFFF] = (offset, filesz)
    return 0x20 + count * 32, rows

def installed(self, length):
    elf_at, rows = segments(self)
    elf = self[elf_at:]
    assert elf[:4] == b'\x7fELF'
    phoff, = struct.unpack_from('<Q', elf, 0x20)
    phentsize, phnum = struct.unpack_from('<HH', elf, 0x36)
    out = bytearray(length)
    header = phoff + phentsize * phnum
    out[:header] = elf[:header]
    for index, (offset, filesz) in rows.items():
        _, _, at, _, _, size, _, _ = struct.unpack_from('<II6Q', elf, phoff + index * phentsize)
        assert size == filesz and at + size <= length, (index, size, filesz)
        out[at:at + size] = self[offset:offset + filesz]
    return bytes(out), sorted(rows)

if __name__ == '__main__':
    self = open(sys.argv[1], 'rb').read()
    image, stored = installed(self, len(open(sys.argv[2], 'rb').read()))
    digest = hashlib.sha256(image).hexdigest()
    print('stored', stored, 'bytes', len(image), 'sha256', digest)
    if len(sys.argv) > 3:
        assert digest == sys.argv[3], 'mismatch'
        print('matches expected')
