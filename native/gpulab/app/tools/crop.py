"""Crop and nearest-scale a region of a PNG written by ppm_to_png.py (stdlib only)."""
import struct, sys, zlib
src, dst, x0, y0, w, h, k = sys.argv[1], sys.argv[2], *map(int, sys.argv[3:8])
data = open(src, 'rb').read(); at = 8; idat = b''
while at < len(data):
    n = struct.unpack('>I', data[at:at+4])[0]; kind = data[at+4:at+8]; body = data[at+8:at+8+n]; at += 12 + n
    if kind == b'IHDR': W, H = struct.unpack('>II', body[:8])
    if kind == b'IDAT': idat += body
raw = zlib.decompress(idat); stride = W * 3 + 1
rows = []
for y in range(y0, y0 + h):
    line = raw[y * stride + 1 + x0 * 3:y * stride + 1 + (x0 + w) * 3]
    big = b''.join(line[i:i+3] * k for i in range(0, len(line), 3))
    rows.extend([b'\0' + big] * k)
chunk = lambda t, b: struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b))
open(dst, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w * k, h * k, 8, 2, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b''))
