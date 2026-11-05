"""Bake the GPU Lab UI faces into source/ui_font_data.h.

Rasterises Ubuntu Sans (Ubuntu Font Licence 1.0) with the system FreeType
through ctypes, light autohinting, 8-bit coverage. Output is deterministic for
a given font file and FreeType build. Run in WSL:
    python3 -I tools/bake_font.py /usr/share/fonts/truetype/ubuntu/UbuntuSans[wdth,wght].ttf source/ui_font_data.h
"""
from ctypes import (CDLL, POINTER, Structure, byref, c_char_p, c_int, c_long, c_short, c_ubyte, c_uint,
                    c_ushort, c_void_p)
import ctypes.util, hashlib, pathlib, struct, sys

# Each face: enum name, pixel size, weight. Order is the uf_face index.
FACES = [('UF_CAPTION', 16, 650), ('UF_SMALL', 19, 400), ('UF_BODY', 22, 400), ('UF_BODY_MEDIUM', 22, 560),
         ('UF_LABEL', 25, 560), ('UF_TITLE', 30, 640), ('UF_STAT', 38, 640), ('UF_HERO', 50, 700)]
# Bytes 0x80.. in UI strings select these code points (see ui_font.h).
EXTRA = [0x00D7, 0x2022, 0x2026, 0x00B7, 0x2014]

class Generic(Structure): _fields_ = [('data', c_void_p), ('finalizer', c_void_p)]
class Vector(Structure): _fields_ = [('x', c_long), ('y', c_long)]
class BBox(Structure): _fields_ = [('xMin', c_long), ('yMin', c_long), ('xMax', c_long), ('yMax', c_long)]
class Metrics(Structure): _fields_ = [(n, c_long) for n in ('width', 'height', 'horiBearingX', 'horiBearingY',
                                       'horiAdvance', 'vertBearingX', 'vertBearingY', 'vertAdvance')]
class Bitmap(Structure):
    _fields_ = [('rows', c_uint), ('width', c_uint), ('pitch', c_int), ('buffer', POINTER(c_ubyte)),
                ('num_grays', c_ushort), ('pixel_mode', c_ubyte), ('palette_mode', c_ubyte), ('palette', c_void_p)]
class Slot(Structure):
    _fields_ = [('library', c_void_p), ('face', c_void_p), ('next', c_void_p), ('glyph_index', c_uint),
                ('generic', Generic), ('metrics', Metrics), ('linearHoriAdvance', c_long),
                ('linearVertAdvance', c_long), ('advance', Vector), ('format', c_int), ('bitmap', Bitmap),
                ('bitmap_left', c_int), ('bitmap_top', c_int)]
class SizeMetrics(Structure):
    _fields_ = [('x_ppem', c_ushort), ('y_ppem', c_ushort), ('x_scale', c_long), ('y_scale', c_long),
                ('ascender', c_long), ('descender', c_long), ('height', c_long), ('max_advance', c_long)]
class Size(Structure): _fields_ = [('face', c_void_p), ('generic', Generic), ('metrics', SizeMetrics)]
class Face(Structure):
    _fields_ = [('num_faces', c_long), ('face_index', c_long), ('face_flags', c_long), ('style_flags', c_long),
                ('num_glyphs', c_long), ('family_name', c_char_p), ('style_name', c_char_p),
                ('num_fixed_sizes', c_int), ('available_sizes', c_void_p), ('num_charmaps', c_int),
                ('charmaps', c_void_p), ('generic', Generic), ('bbox', BBox), ('units_per_EM', c_ushort),
                ('ascender', c_short), ('descender', c_short), ('height', c_short),
                ('max_advance_width', c_short), ('max_advance_height', c_short),
                ('underline_position', c_short), ('underline_thickness', c_short),
                ('glyph', POINTER(Slot)), ('size', POINTER(Size)), ('charmap', c_void_p)]

LOAD_RENDER, LOAD_FORCE_AUTOHINT, LOAD_TARGET_LIGHT = 1 << 2, 1 << 5, 1 << 16

def axes(font):
    """Axis tags in fvar order, read directly from the table directory."""
    count = struct.unpack('>H', font[4:6])[0]
    for i in range(count):
        tag, _, offset, _ = struct.unpack('>4sIII', font[12 + 16 * i:28 + 16 * i])
        if tag == b'fvar':
            _, _, axes_offset, _, axis_count, axis_size = struct.unpack('>HHHHHH', font[offset:offset + 12])
            base = offset + axes_offset
            return [font[base + j * axis_size:base + j * axis_size + 4].decode() for j in range(axis_count)]
    return []

def main(font_path, out_path):
    ft = CDLL(ctypes.util.find_library('freetype'))
    ft.FT_New_Face.argtypes = [c_void_p, c_char_p, c_long, POINTER(POINTER(Face))]
    ft.FT_Set_Pixel_Sizes.argtypes = [POINTER(Face), c_uint, c_uint]
    ft.FT_Load_Char.argtypes = [POINTER(Face), c_long, c_int]
    ft.FT_Get_Char_Index.argtypes = [POINTER(Face), c_long]
    ft.FT_Set_Var_Design_Coordinates.argtypes = [POINTER(Face), c_uint, POINTER(c_long)]
    library = c_void_p()
    assert ft.FT_Init_FreeType(byref(library)) == 0
    font = pathlib.Path(font_path).read_bytes()
    tags = axes(font)
    assert 'wght' in tags, tags
    codes = list(range(0x20, 0x7f)) + EXTRA
    alpha, glyph_rows, face_rows = bytearray(), [], []
    for name, px, weight in FACES:
        face = POINTER(Face)()
        assert ft.FT_New_Face(library, font_path.encode(), 0, byref(face)) == 0
        coords = (c_long * len(tags))(*[(weight if t == 'wght' else 100) << 16 for t in tags])
        assert ft.FT_Set_Var_Design_Coordinates(face, len(tags), coords) == 0
        assert ft.FT_Set_Pixel_Sizes(face, 0, px) == 0
        m = face.contents.size.contents.metrics
        ascent, descent = (m.ascender + 63) >> 6, (-m.descender + 63) >> 6
        first = len(glyph_rows)
        for code in codes:
            assert ft.FT_Get_Char_Index(face, code), (name, hex(code))
            assert ft.FT_Load_Char(face, code, LOAD_RENDER | LOAD_FORCE_AUTOHINT | LOAD_TARGET_LIGHT) == 0
            slot = face.contents.glyph.contents
            b = slot.bitmap
            assert b.pixel_mode in (0, 2) and b.width < 256 and b.rows < 256
            offset = len(alpha)
            for y in range(b.rows):
                row = b.buffer[y * b.pitch:y * b.pitch + b.width] if b.pitch > 0 else []
                alpha.extend(row)
            # Light hinting keeps fractional advances; store 26.6 for an exact pen.
            glyph_rows.append((offset, b.width, b.rows, slot.bitmap_left, slot.bitmap_top, slot.linearHoriAdvance >> 10))
        face_rows.append((name, px, ascent, descent, first))
    out = ['/* Generated by tools/bake_font.py from %s (Ubuntu Font Licence 1.0).' % pathlib.Path(font_path).name,
           ' * Font SHA-256 %s. Do not edit. */' % hashlib.sha256(font).hexdigest(),
           '#ifndef UI_FONT_DATA_H', '#define UI_FONT_DATA_H', '#include "ui_font.h"',
           '#define UF_GLYPHS_PER_FACE %du' % len(codes)]
    out.append('static const uf_glyph uf_glyphs[%d]={' % len(glyph_rows))
    for i in range(0, len(glyph_rows), 6):
        out.append(''.join('{%d,%d,%d,%d,%d,%d},' % g for g in glyph_rows[i:i + 6]))
    out.append('};')
    out.append('static const uf_face_data uf_faces[UF_FACE_COUNT]={')
    out.extend('{%d,%d,%d,%d}, /* %s */' % (px, a, d, first, name) for name, px, a, d, first in face_rows)
    out.append('};')
    out.append('static const uint8_t uf_alpha[%d]={' % len(alpha))
    for i in range(0, len(alpha), 40):
        out.append(','.join(str(v) for v in alpha[i:i + 40]) + ',')
    out.append('};')
    out.extend('_Static_assert(%s==%d,"face order matches ui_font.h");' % (r[0], i) for i, r in enumerate(face_rows))
    out.append('#endif')
    pathlib.Path(out_path).write_text('\n'.join(out) + '\n')
    print('faces', len(face_rows), 'glyphs', len(glyph_rows), 'alpha bytes', len(alpha))

if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])
