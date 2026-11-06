"""Offline integration qualification with owner's exact input files.

Requires Unicorn 2.1.4. No console writes or payload injection.
python test_integrated.py /path/to/Test5.elf /path/to/Lite-v1.11.elf
"""
import struct
import sys
import unittest
from pathlib import Path

from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import *

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from lite1160 import integrate as link
from lite1160 import build_bridge as bridge

REFERENCE = Path(sys.argv.pop(1)).read_bytes()
LITE = Path(sys.argv.pop(1)).read_bytes()


class Integrated(unittest.TestCase):
    def test_unknown_inputs_refuse(self):
        for ref, lite in [(b'', LITE), (REFERENCE, b''), (REFERENCE[:-1], LITE),
                          (REFERENCE, LITE[:-1]), (REFERENCE, LITE[:-1] + b'X')]:
            with self.assertRaises(ValueError):
                link.build(ref, lite)

    def test_lite_load_segments_unchanged_except_one_call(self):
        candidate, receipt = link.build(REFERENCE, LITE)
        ph, _, _ = link.headers(LITE)
        call = link.file_offset(ph, link.LITE_MAIN_CALL, 6)
        for p in ph:
            if p[0] != 1:
                continue
            original = bytearray(LITE[p[2]:p[2] + p[5]])
            actual = bytearray(candidate[p[2]:p[2] + p[5]])
            if p[2] <= call < p[2] + p[5]:
                actual[call-p[2]:call-p[2]+6] = original[call-p[2]:call-p[2]+6]
            self.assertEqual(actual, original)
        start = receipt['a53_file_offset'] + bridge.TEXT_FILE_OFFSET + bridge.KSTUFF_IMAGE
        self.assertEqual(candidate[start:start + bridge.KSTUFF_IMAGE_BYTES], bytes(bridge.KSTUFF_IMAGE_BYTES))
        self.assertEqual(link.build(REFERENCE, LITE)[0], candidate)

    def test_loader_relocations_match_both_independent_images(self):
        candidate, receipt = link.build(REFERENCE, LITE)
        ph, sh, _ = link.headers(candidate)
        memory = bytearray(link.align(max(p[3] + p[6] for p in ph)))
        for p in ph:
            if p[0] == 1:
                self.assertEqual(p[2] % link.PAGE, p[3] % link.PAGE)
                memory[p[3]:p[3] + p[5]] = candidate[p[2]:p[2] + p[5]]
        base = 0x926100000
        locations = set()
        for section in sh:
            if section[1] != 4:
                continue
            for at in range(section[4], section[4] + section[5], 24):
                offset, info, addend = struct.unpack_from('<QQq', candidate, at)
                if info & 0xffffffff == 8:
                    self.assertNotIn(offset, locations)
                    locations.add(offset)
                    self.assertTrue(any(p[0] == 1 and p[3] <= offset and offset + 8 <= p[3] + p[6] for p in ph))
                    struct.pack_into('<Q', memory, offset, base + addend)
        expected = 0
        for raw, shift in [(LITE, 0), (REFERENCE, link.MODULE_VA)]:
            _, source_sections, _ = link.headers(raw)
            for section in source_sections:
                if section[1] != 4:
                    continue
                for at in range(section[4], section[4] + section[5], 24):
                    offset, info, addend = struct.unpack_from('<QQq', raw, at)
                    if info & 0xffffffff == 8:
                        self.assertEqual(struct.unpack_from('<Q', memory, offset + shift)[0], base + shift + addend)
                        expected += 1
        self.assertEqual(len(locations), expected)
        self.assertEqual(receipt['a53_loader_relative_relocations'], 1475)

    def test_wrapper_fail_closed_arguments_results_and_abi(self):
        for base in (0, 0x1000000):
            for a53_result in (0, 1, -1, -2, 0x12345678):
                with self.subTest(base=base, a53_result=a53_result):
                    emu = Uc(UC_ARCH_X86, UC_MODE_64)
                    emu.mem_map(base, 0x500000)
                    emu.mem_write(base + link.WRAPPER, link.wrapper())
                    targets = [link.LITE_GET_ARGS, link.MODULE_VA + 0xfaae8, link.LITE_MAIN]
                    for target in targets:
                        emu.mem_write(base + target, b'\xc3')
                    stack, sentinel = base + 0x4f0008, base + 0x4ff000
                    emu.mem_write(stack, struct.pack('<Q', sentinel))
                    saved = {UC_X86_REG_RBP: 0x111, UC_X86_REG_RBX: 0x222,
                             UC_X86_REG_R12: 0x333, UC_X86_REG_R13: 0x444, UC_X86_REG_R14: 0x555}
                    for reg, value in saved.items():
                        emu.reg_write(reg, value)
                    args = [7, base + 0x400000, base + 0x400100]
                    for reg, value in zip((UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RDX), args):
                        emu.reg_write(reg, value)
                    emu.reg_write(UC_X86_REG_RSP, stack)
                    calls = []
                    def hook(uc, address, size, _):
                        if address - base not in targets:
                            return
                        target = address - base
                        calls.append(target)
                        self.assertEqual(uc.reg_read(UC_X86_REG_RSP) % 16, 8)
                        if target == targets[0]:
                            result = base + 0x400200
                        elif target == targets[1]:
                            self.assertEqual(uc.reg_read(UC_X86_REG_RDI), base + 0x400200)
                            result = a53_result
                        else:
                            self.assertEqual([uc.reg_read(r) for r in (UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RDX)], args)
                            result = -17
                        # Called functions may clobber caller-saved registers.
                        for r in (UC_X86_REG_RDI, UC_X86_REG_RSI, UC_X86_REG_RDX):
                            uc.reg_write(r, 0xbad)
                        uc.reg_write(UC_X86_REG_RAX, result & ((1 << 64) - 1))
                    emu.hook_add(UC_HOOK_CODE, hook)
                    emu.emu_start(base + link.WRAPPER, sentinel, count=100)
                    self.assertEqual(calls, targets if a53_result == 0 else targets[:2])
                    self.assertEqual(emu.reg_read(UC_X86_REG_RAX) & 0xffffffff,
                                     (-17 if a53_result == 0 else a53_result) & 0xffffffff)
                    self.assertEqual(emu.reg_read(UC_X86_REG_RSP), stack + 8)
                    for reg, value in saved.items():
                        self.assertEqual(emu.reg_read(reg), value)

    def test_nested_runtime_returns_status_without_process_exit(self):
        for result in (0, 1, -1, -2, 0x12345678, None):
            with self.subTest(result=result):
                emu = Uc(UC_ARCH_X86, UC_MODE_64)
                emu.mem_map(0, 0x500000)
                emu.mem_write(link.A53_TERMINATE, link.a53_return_stub())
                ph, _, _ = link.headers(REFERENCE)
                at = link.file_offset(ph, link.A53_EPILOGUE, 15)
                emu.mem_write(link.A53_EPILOGUE, REFERENCE[at:at+15])
                pointer, value = 0x400000, 0x400100
                emu.mem_write(link.A53_PAYLOAD_ARGS, struct.pack('<Q', pointer))
                emu.mem_write(pointer + 40, struct.pack('<Q', value if result is not None else 0))
                if result is not None:
                    emu.mem_write(value, struct.pack('<I', result & 0xffffffff))
                stack, sentinel = 0x4f0000, 0x4ff000
                emu.mem_write(stack, struct.pack('<8Q', 0, 1, 2, 3, 4, 5, 6, sentinel))
                emu.reg_write(UC_X86_REG_RSP, stack)
                emu.emu_start(link.A53_TERMINATE, sentinel, count=30)
                self.assertEqual(emu.reg_read(UC_X86_REG_RAX) & 0xffffffff,
                                 (result if result is not None else 1) & 0xffffffff)
                self.assertEqual(emu.reg_read(UC_X86_REG_RSP), stack + 64)


if __name__ == '__main__':
    unittest.main()
