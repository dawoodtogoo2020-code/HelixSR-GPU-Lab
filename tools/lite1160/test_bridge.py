"""Execute the actual adapter entry bytes in Unicorn; no console access.

python -m pip install unicorn==2.1.4
python test_bridge.py /path/to/the/exact/Test5.elf
"""
import sys
import struct
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RBP, UC_X86_REG_RSP
import build_bridge as bridge

REFERENCE = Path(sys.argv.pop(1)).read_bytes()


class Adapter(unittest.TestCase):
    def test_unknown_or_modified_reference_refuses(self):
        for raw in (b'', REFERENCE[:-1], bytes([REFERENCE[0] ^ 1]) + REFERENCE[1:]):
            with self.subTest(size=len(raw)), self.assertRaises(ValueError):
                bridge.build(raw)

    def test_only_declared_ranges_change(self):
        candidate = bridge.build(REFERENCE)
        lo = bridge.TEXT_FILE_OFFSET
        ranges = [(lo + bridge.MAIN, lo + bridge.MAIN_END),
                  (lo + bridge.KSTUFF_IMAGE, lo + bridge.KSTUFF_IMAGE + bridge.KSTUFF_IMAGE_BYTES)]
        cursor = 0
        for start, end in ranges:
            self.assertEqual(candidate[cursor:start], REFERENCE[cursor:start])
            cursor = end
        self.assertEqual(candidate[cursor:], REFERENCE[cursor:])
        self.assertEqual(candidate[ranges[1][0]:ranges[1][1]], bytes(bridge.KSTUFF_IMAGE_BYTES))

    def test_firmware_refusal_result_propagation_and_stack(self):
        for fw in (0, 0x11400000, 0x11600000, 0x11600007, 0x12000000, 0x13600000):
            for installer_result in (0, 1, -1, -2):
                with self.subTest(fw=hex(fw), installer_result=installer_result):
                    emulator = Uc(UC_ARCH_X86, UC_MODE_64)
                    emulator.mem_map(0, 0x200000)
                    emulator.mem_write(bridge.MAIN, bridge.entry_stub())
                    for target in (bridge.GET_FW, bridge.A53_INSTALL):
                        emulator.mem_write(target, b'\xc3')
                    stack = 0x1f0008
                    sentinel = 0x1ff000
                    emulator.mem_write(stack, struct.pack('<Q', sentinel))
                    emulator.reg_write(UC_X86_REG_RSP, stack)
                    emulator.reg_write(UC_X86_REG_RBP, 0x1234567890)
                    calls = []
                    def on_instruction(uc, address, size, _):
                        if address in (bridge.GET_FW, bridge.A53_INSTALL):
                            self.assertEqual(uc.reg_read(UC_X86_REG_RSP) % 16, 8)
                            calls.append(address)
                            uc.reg_write(UC_X86_REG_RAX,
                                         (fw if address == bridge.GET_FW else installer_result) & ((1 << 64) - 1))
                    emulator.hook_add(UC_HOOK_CODE, on_instruction)
                    emulator.emu_start(bridge.MAIN, sentinel, count=30)
                    compatible = fw & 0xffff0000 == bridge.SUPPORTED_FW
                    self.assertEqual(calls, [bridge.GET_FW] + ([bridge.A53_INSTALL] if compatible else []))
                    self.assertEqual(emulator.reg_read(UC_X86_REG_RAX) & 0xffffffff,
                                     (installer_result if compatible else 1) & 0xffffffff)
                    self.assertEqual(emulator.reg_read(UC_X86_REG_RSP), stack + 8)
                    self.assertEqual(emulator.reg_read(UC_X86_REG_RBP), 0x1234567890)


if __name__ == '__main__':
    unittest.main()
