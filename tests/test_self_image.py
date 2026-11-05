# SPDX-License-Identifier: GPL-3.0-or-later
"""FTP representation regression: unsigned SELF trailers are not mapped ELF data."""
import importlib.util
import struct
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('ftp_self_image', ROOT / 'native/gpulab/app/tools/self_image.py')
self_image = importlib.util.module_from_spec(spec)
spec.loader.exec_module(self_image)


def fixture(magic=0xEEF51454):
    elf = bytearray(64 + 2 * 56)
    elf[:6] = b'\x7fELF\x02\x01'
    struct.pack_into('<Q', elf, 32, 64)
    struct.pack_into('<HH', elf, 54, 56, 2)
    struct.pack_into('<II6Q', elf, 64, 1, 5, 256, 0, 0, 6, 6, 16)
    struct.pack_into('<II6Q', elf, 120, 0x6fffff01, 0, 300, 0, 0, 26, 26, 1)
    data = bytearray(288)
    struct.pack_into('<I', data, 0, magic)
    struct.pack_into('<H', data, 24, 1)
    struct.pack_into('<4Q', data, 32, 0x800, 256, 6, 6)
    data[64:240] = elf
    data[256:262] = b'loaded'
    data[262:288] = bytes(range(1, 27))
    expected = bytearray(326)
    expected[:176] = elf
    expected[256:262] = b'loaded'
    return data, bytes(expected)


class SelfImageTest(unittest.TestCase):
    def test_both_public_self_magics_reconstruct_only_backed_regions(self):
        for magic in (0xEEF51454, 0x1D3D154F):
            data, expected = fixture(magic)
            image, indices = self_image.installed(data, len(expected))
            self.assertEqual(image, expected)
            self.assertEqual(indices, [0])
            self.assertEqual(image[300:326], bytes(26))

    def test_unsigned_version_trailer_is_not_an_elf_payload(self):
        data, expected = fixture()
        data[262:288] = b'x' * 26
        self.assertEqual(self_image.installed(data, len(expected))[0], expected)

    def test_stored_payload_change_is_preserved_not_masked(self):
        data, expected = fixture()
        data[257] ^= 1
        actual, _ = self_image.installed(data, len(expected))
        self.assertNotEqual(actual, expected)
        self.assertEqual(actual[257], data[257])


if __name__ == '__main__':
    unittest.main()
