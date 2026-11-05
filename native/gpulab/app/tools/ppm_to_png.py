"""Convert binary P6 PPM files to PNG beside them (stdlib only)."""
import pathlib, struct, sys, zlib

def convert(path):
    data = path.read_bytes()
    fields, at = [], 0
    while len(fields) < 4:
        while data[at:at + 1].isspace():
            at += 1
        start = at
        while not data[at:at + 1].isspace():
            at += 1
        fields.append(data[start:at])
    assert fields[0] == b'P6' and fields[3] == b'255'
    width, height = int(fields[1]), int(fields[2])
    pixels = data[at + 1:at + 1 + width * height * 3]
    raw = b''.join(b'\0' + pixels[y * width * 3:(y + 1) * width * 3] for y in range(height))
    chunk = lambda kind, body: struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))
    png = b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b'')
    path.with_suffix('.png').write_bytes(png)

for name in sys.argv[1:]:
    convert(pathlib.Path(name))
