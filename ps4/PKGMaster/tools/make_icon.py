#!/usr/bin/env python3
"""Generate an original icon0.png using only Python's standard library."""
import struct
import zlib
from pathlib import Path

SIZE = 512
BG = (13, 19, 31, 255)
CYAN = (62, 220, 210, 255)
WHITE = (238, 245, 250, 255)
pixels = [[BG for _ in range(SIZE)] for _ in range(SIZE)]

def rect(x0, y0, x1, y1, color):
    for y in range(max(0, y0), min(SIZE, y1)):
        for x in range(max(0, x0), min(SIZE, x1)):
            pixels[y][x] = color

rect(52, 52, 460, 460, CYAN)
rect(68, 68, 444, 444, BG)
rect(154, 128, 226, 384, WHITE)
rect(226, 128, 342, 200, WHITE)
rect(342, 200, 414, 272, WHITE)
rect(226, 272, 342, 344, WHITE)
rect(278, 200, 342, 272, BG)

def chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xffffffff)

raw = b"".join(b"\x00" + bytes(channel for pixel in row for channel in pixel) for row in pixels)
png = (b"\x89PNG\r\n\x1a\n"
       + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))
       + chunk(b"IDAT", zlib.compress(raw, 9))
       + chunk(b"IEND", b""))
out = Path(__file__).resolve().parents[1] / "sce_sys" / "icon0.png"
out.parent.mkdir(parents=True, exist_ok=True)
out.write_bytes(png)
print(f"Generated {out} ({len(png)} bytes)")
